#!/usr/bin/env python3
"""
J-Link flash script for the SAMD21 F Prime deployments in this repository.
Wraps JLinkExe with convenient deployment selection.

Deployments are discovered from the `=== Deployments ===` markers in the root
CMakeLists.txt, and the toolchain defaults to `default_toolchain` in settings.ini.

Requires SEGGER's JLinkExe on PATH and a J-Link probe wired to the Curiosity Nano's
SWD pads. The Curiosity Nano's on-board nEDBG debugger speaks CMSIS-DAP rather than
the J-Link protocol; to flash through it, use `pymcuprog` or OpenOCD against the
`.elf.bin` this build produces instead of this script.
"""

import argparse
import configparser
import logging
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path


# ANSI color codes
class Colors:
    RESET = '\033[0m'
    BOLD = '\033[1m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'


class ColoredFormatter(logging.Formatter):
    """Custom formatter with colors for different log levels."""

    FORMATS = {
        logging.DEBUG: Colors.BLUE + '%(levelname)s: %(message)s' + Colors.RESET,
        logging.INFO: '%(message)s',
        logging.WARNING: Colors.YELLOW + 'WARNING: %(message)s' + Colors.RESET,
        logging.ERROR: Colors.RED + 'ERROR: %(message)s' + Colors.RESET,
        logging.CRITICAL: Colors.RED + Colors.BOLD + 'CRITICAL: %(message)s' + Colors.RESET,
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


# Global logger
logger = logging.getLogger('jlink_flash')


def setup_logging(verbose=False):
    """Setup logging with colored output."""
    logger.setLevel(logging.DEBUG if verbose else logging.INFO)

    handler = logging.StreamHandler()
    handler.setFormatter(ColoredFormatter())
    logger.addHandler(handler)


def find_deployments(project_root):
    """Find all deployment directories by parsing CMakeLists.txt markers."""
    deployments = []
    cmake_file = Path(project_root) / "CMakeLists.txt"

    if not cmake_file.exists():
        return deployments

    try:
        in_deployments = False
        with open(cmake_file, 'r') as f:
            for line in f:
                line = line.strip()

                # Check for deployment section markers
                if '=== Deployments ===' in line:
                    in_deployments = True
                    continue
                elif '=== /Deployments ===' in line:
                    in_deployments = False
                    break

                # Extract deployment paths from add_fprime_subdirectory calls
                if in_deployments and 'add_fprime_subdirectory' in line:
                    # Extract path from add_fprime_subdirectory("path")
                    match = re.search(r'add_fprime_subdirectory\s*\(\s*["\']([^"\']+)["\']', line)
                    if match:
                        dep_path = match.group(1)
                        # Handle ${CMAKE_CURRENT_LIST_DIR} and absolute/relative paths
                        dep_path = dep_path.replace('${CMAKE_CURRENT_LIST_DIR}/', '')
                        dep_path = dep_path.strip('/')

                        # Get just the directory name
                        dep_name = Path(dep_path).name
                        if dep_name:
                            deployments.append(dep_name)
    except Exception as e:
        logger.debug(f"Error parsing CMakeLists.txt: {e}")
        return deployments

    return sorted(deployments)


def get_default_toolchain(project_root):
    """Read default toolchain from settings.ini."""
    settings_file = Path(project_root) / "settings.ini"
    if settings_file.exists():
        config = configparser.ConfigParser()
        config.read(settings_file)
        if 'fprime' in config and 'default_toolchain' in config['fprime']:
            return config['fprime']['default_toolchain']
    return None


def find_bin_file(project_root, deployment, toolchain):
    """Find the built .bin file for a deployment."""
    patterns = [
        f"build-fprime-automatic-{toolchain}/bin/{toolchain}/{deployment}.elf.bin",
        f"build-artifacts/{toolchain}/{deployment}/bin/{deployment}.elf.bin",
    ]

    for pattern in patterns:
        bin_path = Path(project_root) / pattern
        if bin_path.exists():
            return bin_path

    return None


def get_device_name(toolchain):
    """Get JLink device name from toolchain."""
    # Map toolchain names to JLink device names. The Curiosity Nano carries an
    # ATSAMD21G17D; JLinkExe has no D variant, and the A shares the same flash and
    # RAM geometry, so the A is the correct selection.
    device_map = {
        'microchip_curiosity': 'ATSAMD21G17A',
    }

    return device_map.get(toolchain, 'ATSAMD21G17A')


def get_flash_address(device_name):
    """Get bin flash address from the device name"""
    address_map = {
        # No bootloader on the Curiosity Nano: the image starts at the vector table.
        # This matches MEMORY { FLASH : ORIGIN = 0x0 } in
        # lib/fprime-samd/cmake/toolchain/samd21/curiosity_nano/linker_scripts/
        # flash_without_bootloader.ld
        'ATSAMD21G17A': '0x0000',
    }

    return address_map.get(device_name, '0x0000')


def create_jlink_script(bin_file, flash_addr):
    """Create a temporary JLink command script from template."""
    # Get template path
    script_dir = Path(__file__).parent
    template_path = script_dir / "flash.jlink"

    # Load template
    try:
        with open(template_path, 'r') as f:
            template = f.read()
    except FileNotFoundError:
        logger.error(f"Template file not found: {template_path}")
        raise

    # Render template
    rendered = template.format(bin_file=bin_file, flash_addr=flash_addr)

    # Create temporary file for JLink commands
    fd, script_path = tempfile.mkstemp(suffix='.jlink', text=True)

    try:
        with os.fdopen(fd, 'w') as f:
            f.write(rendered)
    except:
        os.close(fd)
        raise

    return script_path


def flash_device(bin_file, device, verbose=False):
    """Flash the device using JLinkExe."""
    script_path = None

    try:
        # Create JLink command script
        script_path = create_jlink_script(bin_file, get_flash_address(device))

        # Build JLinkExe command
        cmd = [
            'JLinkExe',
            '-device', device,
            '-CommandFile', script_path
        ]

        print(f"\n{Colors.CYAN}{'='*60}{Colors.RESET}")
        print(f"{Colors.BOLD}Flashing:{Colors.RESET} {Colors.GREEN}{bin_file.name}{Colors.RESET}")
        print(f"{Colors.BOLD}Device:{Colors.RESET} {device}")
        print(f"{Colors.BOLD}Interface:{Colors.RESET} SWD")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")

        if verbose:
            logger.debug(f"Command: {' '.join(cmd)}")
            logger.debug(f"Script contents:")
            with open(script_path, 'r') as f:
                for line in f:
                    logger.debug(f"  {line.rstrip()}")

        logger.info("Connecting to target...")

        # Run JLinkExe
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False
        )

        # Parse output for success/failure
        output = result.stdout
        if verbose or result.returncode != 0:
            print(output)

        # Check for success indicators
        if 'Downloading file' in output and result.returncode == 0:
            logger.info(f"\n{Colors.GREEN}Flash successful!{Colors.RESET}")
            return 0
        elif 'Cannot connect to target' in output:
            logger.error("Cannot connect to target!")
            logger.info("  1. Check that JLink is connected")
            logger.info("  2. Check that target power is on")
            logger.info("  3. Try resetting the target")
            return 1
        elif result.returncode != 0:
            logger.error(f"Flash failed with exit code {result.returncode}")
            if not verbose:
                logger.info("Run with -v for detailed output")
            return result.returncode
        else:
            logger.warning("Flash completed but success unclear")
            if not verbose:
                logger.info("Run with -v for detailed output")
            return 0

    except FileNotFoundError:
        logger.error("JLinkExe not found. Is J-Link Software installed?")
        logger.info("  Download from: https://www.segger.com/downloads/jlink/")
        return 1
    except Exception as e:
        logger.error(f"Flash failed: {e}")
        return 1
    finally:
        # Clean up temporary script
        if script_path and os.path.exists(script_path):
            try:
                os.unlink(script_path)
            except:
                pass


def main():
    parser = argparse.ArgumentParser(
        description='Flash a SAMD21 F Prime deployment using JLink',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                             # Interactive mode
  %(prog)s CuriosityReference          # Flash a specific deployment
  %(prog)s -t microchip_curiosity      # Use a specific toolchain
  %(prog)s -d ATSAMD21G17A             # Use a specific device
        """
    )

    parser.add_argument('deployment', nargs='?', help='Deployment name to flash')
    parser.add_argument('-t', '--toolchain', help='Toolchain to use')
    parser.add_argument('-d', '--device', help='JLink device name (e.g., ATSAMD21E18A)')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')

    args = parser.parse_args()

    # Setup logging
    setup_logging(args.verbose)

    # Get project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # Get or prompt for deployment
    deployment = args.deployment
    if not deployment:
        deployments = find_deployments(project_root)
        if not deployments:
            logger.error("No deployments found in project")
            return 1

        print(f"{Colors.BOLD}Available deployments:{Colors.RESET}")
        for i, dep in enumerate(deployments, 1):
            print(f"  {Colors.CYAN}{i}.{Colors.RESET} {dep}")

        while True:
            try:
                choice = input(f"\n{Colors.BOLD}Select deployment (number or name):{Colors.RESET} ").strip()
                if choice.isdigit():
                    idx = int(choice) - 1
                    if 0 <= idx < len(deployments):
                        deployment = deployments[idx]
                        break
                elif choice in deployments:
                    deployment = choice
                    break
                logger.warning("Invalid selection, try again")
            except (KeyboardInterrupt, EOFError):
                print(f"\n\n{Colors.YELLOW}Aborted{Colors.RESET}")
                return 1

    logger.debug(f"Selected deployment: {deployment}")

    # Get toolchain
    toolchain = args.toolchain or get_default_toolchain(project_root)
    if not toolchain:
        logger.error("No toolchain specified and none found in settings.ini")
        return 1

    logger.debug(f"Using toolchain: {toolchain}")

    # Find bin file
    bin_file = find_bin_file(project_root, deployment, toolchain)
    if not bin_file:
        logger.error(f"Could not find .bin file for {deployment} with toolchain {toolchain}")
        logger.info("  Did you build it first?")
        return 1

    logger.debug(f"Found bin file: {bin_file}")

    # Get device name
    device = args.device or get_device_name(toolchain)
    logger.debug(f"Using device: {device}")

    # Check JLink connection
    logger.info("Make sure JLink debugger is connected to target")

    # Flash
    return flash_device(bin_file, device, args.verbose)


if __name__ == '__main__':
    sys.exit(main())
