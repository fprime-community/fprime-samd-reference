"""test_fram_tester.py:

Integration tests for CuriosityReference.FramTester, and through it the Fram.FramDriver,
Samd21.SpiDriver and Samd21.DmaDriver stack, against a running deployment wired to a
CY15B102QN F-RAM. Every test leaves the F-RAM contents it touched in a known state and
polling disabled, so the tests are order independent.

Requirement coverage is noted per test: REQ-FRAM-* are defined in
fprime-devices/Fram/Components/FramDriver/docs/sdd.md and REQ-FRAMT-* in
CuriosityReference/FramTester/docs/sdd.md.

Not covered here: Fram.Status.BUSY and the FramTester.Busy event. The GDS serialises commands
and a command completes within one rate-group tick, so an overlapping request cannot be provoked
deterministically from the ground; the FramDriver unit test covers BUSY.
"""

import pytest
from fprime_gds.common.testing_fw import predicates

DEVICE_SIZE = 0x40000
MAX_TRANSFER = 128
CMD_TIMEOUT = 10
TLM_TIMEOUT = 20


def fram_tester(api):
    """Deployment mnemonic of the FramTester instance"""
    return api.get_mnemonic("CuriosityReference.FramTester")


def send_ok(api, command, args=None, timeout=CMD_TIMEOUT):
    """Send a command and assert it was dispatched and completed successfully.

    Replaces send_and_assert_command: on a fully passive deployment the command handler
    runs inside the dispatch call, so OpCodeCompleted is logged before OpCodeDispatched
    and the ordered sequence check in the stock helper fails.
    """
    disp = api.get_mnemonic("Svc.CommandDispatcher")
    start = api.event_history.size()
    api.send_command(command, args or [])
    api.assert_event(f"{disp}.OpCodeDispatched", start=start, timeout=timeout)
    api.assert_event(f"{disp}.OpCodeCompleted", start=start, timeout=timeout)
    api.assert_event_count(0, f"{disp}.OpCodeError", start=start)


def send_expect_event(api, command, args, event, event_args, timeout=CMD_TIMEOUT):
    """Send a command that is expected to fail and assert the event it must raise"""
    start = api.event_history.size()
    api.send_command(command, args)
    return api.assert_event(event, args=event_args, start=start, timeout=timeout)


def write_pattern(api, offset, length, seed):
    """WRITE_PATTERN and wait for its WriteComplete"""
    start = api.event_history.size()
    send_ok(api, f"{fram_tester(api)}.WRITE_PATTERN", [offset, length, seed])
    api.assert_event(
        f"{fram_tester(api)}.WriteComplete", args=[offset, length, seed], start=start, timeout=CMD_TIMEOUT
    )


def read_verify(api, offset, length, seed):
    """READ_VERIFY and wait for its VerifyComplete"""
    start = api.event_history.size()
    send_ok(api, f"{fram_tester(api)}.READ_VERIFY", [offset, length, seed])
    api.assert_event(
        f"{fram_tester(api)}.VerifyComplete", args=[offset, length, seed], start=start, timeout=CMD_TIMEOUT
    )


def current_telemetry(api, channel):
    """Latest value of a channel, waiting for one update to arrive"""
    result = api.await_telemetry(f"{fram_tester(api)}.{channel}", timeout=TLM_TIMEOUT)
    assert result is not None, f"no {channel} telemetry within {TLM_TIMEOUT} s"
    return result.get_val()


@pytest.fixture(autouse=True)
def polling_off(fprime_test_api):
    """Every test starts and ends with the soak loop disabled"""
    send_ok(fprime_test_api, f"{fram_tester(fprime_test_api)}.SET_POLLING", [False])
    yield
    send_ok(fprime_test_api, f"{fram_tester(fprime_test_api)}.SET_POLLING", [False])


def test_write_read_verify_round_trip(fprime_test_api):
    """Write a pattern, verify it, and read raw bytes back.

    Covers: REQ-FRAM-001, REQ-FRAM-002, REQ-FRAM-003, REQ-FRAMT-001, REQ-FRAMT-002
    """
    offset, length, seed = 0x100, 32, 0x10
    write_pattern(fprime_test_api, offset, length, seed)
    read_verify(fprime_test_api, offset, length, seed)

    start = fprime_test_api.event_history.size()
    send_ok(fprime_test_api, f"{fram_tester(fprime_test_api)}.READ", [offset, 8])
    fprime_test_api.assert_event(
        f"{fram_tester(fprime_test_api)}.ReadComplete",
        args=[offset, 8, 0x10111213, 0x14151617],
        start=start,
        timeout=CMD_TIMEOUT,
    )


def test_short_read_pads_with_zero(fprime_test_api):
    """A READ shorter than eight bytes reports zero for the bytes past its length.

    Covers: REQ-FRAM-003, REQ-FRAMT-003
    """
    offset = 0x200
    write_pattern(fprime_test_api, offset, 4, 0xA0)
    start = fprime_test_api.event_history.size()
    send_ok(fprime_test_api, f"{fram_tester(fprime_test_api)}.READ", [offset, 4])
    fprime_test_api.assert_event(
        f"{fram_tester(fprime_test_api)}.ReadComplete",
        args=[offset, 4, 0xA0A1A2A3, 0],
        start=start,
        timeout=CMD_TIMEOUT,
    )


def test_verify_detects_mismatch(fprime_test_api):
    """READ_VERIFY against a different seed reports the first mismatching byte and fails.

    Covers: REQ-FRAMT-002
    """
    offset, length = 0x300, 16
    write_pattern(fprime_test_api, offset, length, 0x40)
    send_expect_event(
        fprime_test_api,
        f"{fram_tester(fprime_test_api)}.READ_VERIFY",
        [offset, length, 0x41],
        f"{fram_tester(fprime_test_api)}.PatternMismatch",
        [offset, 0x41, 0x40],
    )
    # the data is intact: verifying with the right seed still passes
    read_verify(fprime_test_api, offset, length, 0x40)


def test_end_of_device(fprime_test_api):
    """The largest transfer ending exactly at the last byte of the device succeeds.

    Covers: REQ-FRAM-004, REQ-FRAM-006
    """
    offset = DEVICE_SIZE - MAX_TRANSFER
    write_pattern(fprime_test_api, offset, MAX_TRANSFER, 0x80)
    read_verify(fprime_test_api, offset, MAX_TRANSFER, 0x80)


def test_out_of_range_rejected(fprime_test_api):
    """Requests at or wrapping past the end of the device fail with OUT_OF_RANGE.

    Covers: REQ-FRAM-004, REQ-FRAMT-004
    """
    api = fprime_test_api
    send_expect_event(
        api, f"{fram_tester(api)}.READ", [DEVICE_SIZE, 4],
        f"{fram_tester(api)}.FramError", [None, "OUT_OF_RANGE", DEVICE_SIZE],
    )
    send_expect_event(
        api, f"{fram_tester(api)}.WRITE_PATTERN", [DEVICE_SIZE - 2, 4, 1],
        f"{fram_tester(api)}.FramError", [None, "OUT_OF_RANGE", DEVICE_SIZE - 2],
    )
    send_expect_event(
        api, f"{fram_tester(api)}.READ_VERIFY", [0xFFFFFFFF, 1, 0],
        f"{fram_tester(api)}.FramError", [None, "OUT_OF_RANGE", 0xFFFFFFFF],
    )


def test_zero_length_rejected(fprime_test_api):
    """A zero-length transfer is refused by the driver with TOO_LARGE.

    Covers: REQ-FRAM-005
    """
    api = fprime_test_api
    send_expect_event(
        api, f"{fram_tester(api)}.READ", [0, 0],
        f"{fram_tester(api)}.FramError", [None, "TOO_LARGE", 0],
    )


def test_oversized_length_rejected_by_fram_tester(fprime_test_api):
    """A length above MaxTransfer never reaches the driver.

    Covers: REQ-FRAMT-004
    """
    api = fprime_test_api
    send_expect_event(
        api, f"{fram_tester(api)}.READ", [0, MAX_TRANSFER + 1],
        f"{fram_tester(api)}.InvalidArgument", [MAX_TRANSFER + 1],
    )


def test_counters_and_last_status(fprime_test_api):
    """Successful transactions advance the read/write counters and report LastStatus OK.

    Covers: REQ-FRAMT-005
    """
    api = fprime_test_api
    reads = current_telemetry(api, "ReadCount")
    writes = current_telemetry(api, "WriteCount")

    write_pattern(api, 0x400, 8, 0x01)
    read_verify(api, 0x400, 8, 0x01)
    send_ok(api, f"{fram_tester(api)}.READ", [0x400, 8])

    api.assert_telemetry(
        f"{fram_tester(api)}.WriteCount", predicates.greater_than_or_equal_to(writes + 1), timeout=TLM_TIMEOUT
    )
    api.assert_telemetry(
        f"{fram_tester(api)}.ReadCount", predicates.greater_than_or_equal_to(reads + 2), timeout=TLM_TIMEOUT
    )
    api.assert_telemetry(f"{fram_tester(api)}.LastStatus", "OK", timeout=TLM_TIMEOUT)
    api.assert_telemetry(f"{fram_tester(api)}.ReadData0", 0x01020304, timeout=TLM_TIMEOUT)
    api.assert_telemetry(f"{fram_tester(api)}.ReadData1", 0x05060708, timeout=TLM_TIMEOUT)


def test_polling_soak(fprime_test_api):
    """The rate-group soak loop completes write/verify cycles without failures.

    Covers: REQ-FRAMT-003, REQ-FRAM-008
    """
    api = fprime_test_api
    send_ok(api, f"{fram_tester(api)}.SET_POLL_REGION", [0x1000, 64])
    cycles = current_telemetry(api, "PollCycles")
    failures = current_telemetry(api, "VerifyFailures")
    errors = current_telemetry(api, "FramErrors")

    send_ok(api, f"{fram_tester(api)}.SET_POLLING", [True])
    api.assert_telemetry(
        f"{fram_tester(api)}.PollCycles", predicates.greater_than_or_equal_to(cycles + 3), timeout=30
    )
    send_ok(api, f"{fram_tester(api)}.SET_POLLING", [False])

    assert current_telemetry(api, "VerifyFailures") == failures
    assert current_telemetry(api, "FramErrors") == errors
