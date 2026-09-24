#!/usr/bin/env python3
"""Measure and report SAMD21 flash/RAM usage for a built F Prime ELF.

Two subcommands:

  measure <elf> <out.json> [--nm <out.nm>]
      Runs ``<prefix>readelf -S -W`` and ``<prefix>size -A`` and writes a JSON
      section map with flash/RAM rollups.  ``<out.json>.readelf`` and
      ``<out.json>.size`` are written alongside for the workflow artifact, and
      ``--nm`` additionally captures a demangled, size-sorted symbol dump.
      Exits non-zero if the image does not fit in the target's memory.

  render --current <cur.json> [--baseline <base.json>] [--nm <dump.nm>]
      Renders the sticky pull-request comment body as markdown.  A missing or
      unparseable baseline degrades to absolute sizes plus a "no baseline"
      callout; it is never an error.

Section classification is driven by ELF flags and types, not by section names:
``flash_without_bootloader.ld`` names the initialised-data output section
``.relocate``, not ``.data``, so a name-based parser reports 0 bytes of data
forever and never notices.  GNU ld also stamps ``.relocate`` with section type
``REL`` (a side effect of the ``.rel*`` name convention), so the type is only
consulted to spot ``NOBITS``.

  * ALLOC and not NOBITS  -> occupies flash (present in a PT_LOAD FileSiz)
  * ALLOC and addr in RAM -> occupies RAM
  * not ALLOC             -> .debug_*, .comment, .symtab, .ARM.attributes:
                             carried in the ELF, free on the target

Initialised data is charged to *both* regions: ``.relocate : AT (__etext)``
stores it in flash and ``Reset_Handler`` copies it into RAM at boot.
"""

import argparse
import json
import os
import re
import subprocess
import sys

# SAMD21G17A/D, per lib/fprime-samd/cmake/toolchain/samd21/curiosity_nano/
# linker_scripts/flash_without_bootloader.ld:
#   FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00020000
#   RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00004000
FLASH_BYTES = 0x20000
RAM_BYTES = 0x4000
RAM_ORIGIN = 0x20000000

# GitHub hard-caps an issue/pull-request comment body at 65536 characters.
COMMENT_LIMIT = 65536
# Upper bound on the collapsed nm dump.  The real budget is computed from the
# rendered size of everything else, so the tables are never the thing that gets
# cut; this cap only keeps the comment from becoming unreadably long.
NM_BUDGET_MAX = 40000
# Head-room reserved for the truncation footer and the closing fence/details.
NM_FOOTER_RESERVE = 1024

# readelf -S -W row, e.g.
#   [ 4] .relocate         REL             20000000 020000 0000ac 08  WA  0   0  4
# The flags column is empty for non-ALLOC sections, hence the optional group.
_SECTION_ROW = re.compile(
    r"^\s*\[\s*\d+\]\s+(?P<name>\S*)\s+(?P<type>[A-Z][A-Z0-9_]*)"
    r"\s+(?P<addr>[0-9a-fA-F]+)\s+(?P<off>[0-9a-fA-F]+)\s+(?P<size>[0-9a-fA-F]+)"
    r"\s+(?P<es>[0-9a-fA-F]+)(?P<flags>(?:\s+[A-Za-z]+)?)"
    r"\s+\d+\s+\d+\s+\d+\s*$"
)


def tool(name, prefix=None):
    """Return the binutils executable name for ``name``, honouring the prefix."""
    if prefix is None:
        prefix = os.environ.get("ARM_TOOL_PREFIX", "arm-none-eabi-")
    return prefix + name


def parse_readelf(text):
    """Parse ``readelf -S -W`` output into ``{name: {size, addr, type, alloc}}``."""
    sections = {}
    for line in text.splitlines():
        match = _SECTION_ROW.match(line)
        if match is None or not match.group("name"):
            continue
        sections[match.group("name")] = {
            "size": int(match.group("size"), 16),
            "addr": int(match.group("addr"), 16),
            "type": match.group("type"),
            "alloc": "A" in match.group("flags"),
        }
    return sections


def rollup(sections):
    """Sum the flash and RAM cost of ``sections`` by ELF flags, not by name."""
    flash = 0
    ram = 0
    for info in sections.values():
        if not info["alloc"]:
            continue
        if info["type"] != "NOBITS":
            flash += info["size"]
        if RAM_ORIGIN <= info["addr"] < RAM_ORIGIN + RAM_BYTES:
            ram += info["size"]
    return flash, ram


def measure(elf, out_path, nm_path=None, prefix=None):
    """Measure ``elf``, write the JSON report, and return a process exit code."""
    readelf = subprocess.run(
        [tool("readelf", prefix), "-S", "-W", elf],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    size_a = subprocess.run(
        [tool("size", prefix), "-A", elf], check=True, capture_output=True, text=True
    ).stdout

    sections = parse_readelf(readelf)
    if not sections:
        print(f"error: no section headers parsed from {elf}", file=sys.stderr)
        return 1

    flash, ram = rollup(sections)

    def section_size(name):
        return sections.get(name, {}).get("size", 0)

    report = {
        "elf": os.path.basename(elf),
        "all_sections": {name: info["size"] for name, info in sections.items()},
        "alloc_sections": {
            name: info["size"] for name, info in sections.items() if info["alloc"]
        },
        # `.relocate` is this linker script's spelling of `.data`; accept either.
        "data": section_size(".data") + section_size(".relocate"),
        "text": section_size(".text"),
        "bss": section_size(".bss"),
        "flash": flash,
        "ram": ram,
        "flash_capacity": FLASH_BYTES,
        "ram_capacity": RAM_BYTES,
    }

    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
    with open(out_path + ".readelf", "w", encoding="utf-8") as handle:
        handle.write(readelf)
    with open(out_path + ".size", "w", encoding="utf-8") as handle:
        handle.write(size_a)

    if nm_path is not None:
        # --reverse-sort puts the largest symbols first, which is what matters
        # when only part of the dump survives truncation in the comment.
        nm_out = subprocess.run(
            [
                tool("nm", prefix),
                "--print-size",
                "--size-sort",
                "--reverse-sort",
                "--demangle",
                elf,
            ],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        with open(nm_path, "w", encoding="utf-8") as handle:
            handle.write(nm_out)

    print(
        f"{report['elf']}: text={report['text']} data={report['data']}"
        f" bss={report['bss']}"
    )
    print(
        f"  flash {flash}/{FLASH_BYTES} ({flash / FLASH_BYTES * 100:.2f}%)"
        f"  ram {ram}/{RAM_BYTES} ({ram / RAM_BYTES * 100:.2f}%)"
    )
    if flash > FLASH_BYTES or ram > RAM_BYTES:
        # The link should already have failed on a region overflow; this is a
        # belt-and-braces gate so a budget blow-out is a red check either way.
        print("error: image does not fit in the target's memory", file=sys.stderr)
        return 1
    return 0


def load_report(path):
    """Load a measurement JSON, returning ``None`` if it is absent or broken."""
    if not path or not os.path.isfile(path):
        return None
    try:
        with open(path, encoding="utf-8") as handle:
            report = json.load(handle)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return None
    return report if isinstance(report, dict) and "flash" in report else None


def format_delta(current, baseline):
    """Render a signed byte delta, or ``n/a`` when there is nothing to compare."""
    if current is None or baseline is None:
        return "n/a"
    diff = current - baseline
    if diff == 0:
        return "no change"
    percent = f" ({diff / baseline * 100:+.2f}%)" if baseline else ""
    return f"**{diff:+,} B**{percent}"


def bar(used, capacity, width=30):
    """Render a fixed-width ASCII usage bar."""
    filled = min(width, round(used / capacity * width)) if capacity else 0
    return "#" * filled + "." * (width - filled)


def read_nm(path):
    """Read an nm dump into a list of lines; missing or unreadable means empty."""
    if not path:
        return []
    try:
        with open(path, encoding="utf-8", errors="replace") as handle:
            return handle.read().splitlines()
    except OSError:
        return []


def truncate_nm(lines, budget):
    """Take whole lines from ``lines`` while they fit in ``budget`` characters."""
    kept = []
    used = 0
    for line in lines:
        if used + len(line) + 1 > budget:
            return kept, True
        kept.append(line)
        used += len(line) + 1
    return kept, False


def render(
    current,
    baseline=None,
    nm_lines=(),
    head_sha="",
    base_sha="",
    baseline_origin="unavailable",
    marker="samd21-size-report",
):
    """Render the pull-request comment body for ``current`` vs ``baseline``."""
    out = []
    # Marker used by the reporter workflow to find and edit its own comment.
    out.append(f"<!-- {marker} -->")
    out.append("## SAMD21 flash / RAM size report")
    out.append("")
    out.append(
        f"`{current['elf']}` at `{head_sha[:8] or 'unknown'}` -- SAMD21G17A/D,"
        " 128 KiB flash / 16 KiB RAM"
    )
    out.append("")

    if baseline is None:
        out.append("> [!NOTE]")
        out.append(
            "> No `main` baseline was available for this run, so only absolute sizes"
            " are shown. That is expected on the first pull request, on a cold cache,"
            " or when the baseline build could not be completed (for example when this"
            " pull request bumps a submodule or a pinned autocoder version). The build"
            " itself still passed or failed on its own merits."
        )
        out.append("")

    out.append("| Section | This PR | `main` | Delta | Consumes |")
    out.append("| :-- | --: | --: | --: | :-- |")
    for label, key, consumes in (
        ("`.text`", "text", "flash"),
        ("`.data` (emitted as `.relocate`)", "data", "flash **and** RAM"),
        ("`.bss`", "bss", "RAM"),
    ):
        base_value = baseline.get(key) if baseline else None
        base_cell = f"`{base_value:,}` B" if base_value is not None else "--"
        out.append(
            f"| {label} | `{current[key]:,}` B | {base_cell} |"
            f" {format_delta(current[key], base_value)} | {consumes} |"
        )
    out.append("")

    out.append("### Budget")
    out.append("")
    out.append("| Region | Used | Capacity | Free | Used | Delta |")
    out.append("| :-- | --: | --: | --: | --: | --: |")
    for label, key, capacity_key in (
        ("Flash", "flash", "flash_capacity"),
        ("RAM (static)", "ram", "ram_capacity"),
    ):
        capacity = current[capacity_key]
        used = current[key]
        base_value = baseline.get(key) if baseline else None
        out.append(
            f"| {label} | `{used:,}` B ({used / 1024:.2f} KiB) | `{capacity:,}` B |"
            f" `{capacity - used:,}` B | {used / capacity * 100:.2f}% |"
            f" {format_delta(used, base_value)} |"
        )
    out.append("")
    out.append("```text")
    for label, key, capacity_key in (
        ("flash", "flash", "flash_capacity"),
        ("ram  ", "ram", "ram_capacity"),
    ):
        percent = current[key] / current[capacity_key] * 100
        out.append(
            f"{label} [{bar(current[key], current[capacity_key])}] {percent:5.1f}%"
        )
    out.append("```")
    out.append("")
    out.append(
        "Flash is every `ALLOC` section that is not `NOBITS`; RAM is every `ALLOC`"
        " section placed at `0x2000_0000`. Initialised data is charged to **both**"
        " regions: it is stored in flash (`.relocate : AT (__etext)`) and copied into"
        " RAM by `Reset_Handler`. The heap and the stack are carved out of whatever RAM"
        " is left at runtime and are not counted here -- `flash_without_bootloader.ld`"
        " only asserts `__StackLimit >= __HeapLimit` at link time. Non-`ALLOC` sections"
        " (`.debug_*`, `.comment`, `.symtab`, `.ARM.attributes`) cost nothing on the"
        " target."
    )
    out.append("")

    out.append("<details>")
    out.append(
        "<summary>Per-section breakdown (<code>ALLOC</code> sections only)</summary>"
    )
    out.append("")
    out.append("| Section | This PR | `main` | Delta |")
    out.append("| :-- | --: | --: | --: |")
    base_alloc = (baseline or {}).get("alloc_sections", {})
    for name in sorted(set(current["alloc_sections"]) | set(base_alloc)):
        current_value = current["alloc_sections"].get(name)
        base_value = base_alloc.get(name)
        current_cell = f"`{current_value:,}`" if current_value is not None else "--"
        base_cell = f"`{base_value:,}`" if base_value is not None else "--"
        out.append(
            f"| `{name}` | {current_cell} | {base_cell} |"
            f" {format_delta(current_value, base_value)} |"
        )
    out.append("")
    out.append("</details>")
    out.append("")

    # Everything above, plus the footer, is fixed cost; the nm dump gets what is
    # left of GitHub's comment budget, capped for readability.
    footer = (
        f"<sub>Baseline: {baseline_origin}"
        f"{f' (`{base_sha[:8]}`)' if base_sha else ''}."
        " This comment is updated in place on every push.</sub>"
    )
    fixed = len("\n".join(out + [footer]))
    budget = min(NM_BUDGET_MAX, max(0, COMMENT_LIMIT - fixed - NM_FOOTER_RESERVE))
    kept, truncated = truncate_nm(list(nm_lines), budget)

    out.append("<details>")
    out.append(
        "<summary>Symbol sizes -- <code>nm --print-size --size-sort --reverse-sort"
        f" --demangle</code> ({len(kept):,} of {len(nm_lines):,} symbols, largest"
        " first)</summary>"
    )
    out.append("")
    out.append("```text")
    out.extend(kept)
    if truncated:
        out.append("")
        out.append(
            f"[truncated: {len(nm_lines) - len(kept):,} smaller symbols omitted to stay"
            " under GitHub's 65536-character comment limit. The complete dump is in the"
            " `size-report` workflow artifact.]"
        )
    out.append("```")
    out.append("")
    out.append("</details>")
    out.append("")
    out.append(footer)

    body = "\n".join(out)
    if len(body) > COMMENT_LIMIT:  # belt and braces
        body = body[: COMMENT_LIMIT - 256].rsplit("\n", 1)[0] + "\n```\n\n</details>\n"
    return body


def _cmd_measure(args):
    return measure(args.elf, args.output, nm_path=args.nm, prefix=args.tool_prefix)


def _cmd_render(args):
    current = load_report(args.current)
    if current is None:
        print(
            f"error: current measurement {args.current!r} is missing or unparseable",
            file=sys.stderr,
        )
        return 1
    body = render(
        current,
        baseline=load_report(args.baseline),
        nm_lines=read_nm(args.nm),
        head_sha=args.head_sha,
        base_sha=args.base_sha,
        baseline_origin=args.baseline_origin,
    )
    with open(args.output, "w", encoding="utf-8") as handle:
        handle.write(body)
    print(f"rendered {len(body):,} characters (GitHub limit {COMMENT_LIMIT:,})")
    return 0


def build_parser():
    """Build the argument parser for the command-line entry point."""
    parser = argparse.ArgumentParser(
        prog="size_report.py",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    measure_parser = subparsers.add_parser(
        "measure", help="measure a built ELF into a JSON report"
    )
    measure_parser.add_argument("elf", help="path to the linked .elf")
    measure_parser.add_argument("output", help="path of the JSON report to write")
    measure_parser.add_argument(
        "--nm", default=None, help="also write a demangled, size-sorted symbol dump here"
    )
    measure_parser.add_argument(
        "--tool-prefix",
        default=None,
        help="binutils prefix (default: $ARM_TOOL_PREFIX or arm-none-eabi-)",
    )
    measure_parser.set_defaults(func=_cmd_measure)

    render_parser = subparsers.add_parser(
        "render", help="render the pull-request comment markdown"
    )
    render_parser.add_argument(
        "--current",
        default=os.environ.get("CURRENT_JSON", "size/current.json"),
        help="measurement JSON for this build",
    )
    render_parser.add_argument(
        "--baseline",
        default=os.environ.get("BASELINE_JSON", ""),
        help="measurement JSON for the baseline; missing or broken means no baseline",
    )
    render_parser.add_argument(
        "--nm", default=os.environ.get("NM_TXT", ""), help="symbol dump to embed"
    )
    render_parser.add_argument(
        "--output",
        default=os.environ.get("OUTPUT", "size-report.md"),
        help="markdown file to write",
    )
    render_parser.add_argument("--head-sha", default=os.environ.get("HEAD_SHA", ""))
    render_parser.add_argument("--base-sha", default=os.environ.get("BASE_SHA", ""))
    render_parser.add_argument(
        "--baseline-origin",
        default=os.environ.get("BASELINE_ORIGIN", "unavailable"),
        help="human-readable provenance of the baseline measurement",
    )
    render_parser.set_defaults(func=_cmd_render)
    return parser


def main(argv=None):
    """Command-line entry point; returns a process exit code."""
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
