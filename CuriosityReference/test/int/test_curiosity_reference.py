""" Integration tests for CuriosityReference on the SAMD21 Curiosity Nano

Run through fprime-ci (lib/fprime-samd/ci/sample-configs/samd21g17.yml) or by hand against a running GDS:

    pytest CuriosityReference/test/int --dictionary build-artifacts/samd21g17/CuriosityReference/dict/TopTopologyDictionary.json
"""

# Positional id of `packet Health` in `telemetry packets Main` (CuriosityReference/Top/topology.fpp)
HEALTH_PACKET_ID = 2


def test_no_op_command(fprime_test_api):
    """ Uplink -> deframer -> StaticCmdDispatcher -> event downlink round trip """
    fprime_test_api.send_and_assert_event(
        "CuriosityReference.cmdDisp.CMD_NO_OP",
        events=[fprime_test_api.get_event_pred("CuriosityReference.cmdDisp.NoOpReceived")],
        timeout=5,
    )
    fprime_test_api.assert_event("CuriosityReference.cmdDisp.OpCodeCompleted", timeout=5)


def test_health_packet_telemetry(fprime_test_api):
    """ Telemetry is ground-polled: SEND_PKT must produce the Health packet's channels """
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_event(
        "CuriosityReference.tlm.SEND_PKT",
        [str(HEALTH_PACKET_ID)],
        events=[fprime_test_api.get_event_pred("CuriosityReference.cmdDisp.OpCodeCompleted")],
        timeout=5,
    )
    fprime_test_api.assert_telemetry("CuriosityReference.fwHealth.Counter", timeout=5)
    fprime_test_api.assert_telemetry("CuriosityReference.rg8Hz.CycleCount", timeout=5)


def test_gpio_out_set(fprime_test_api):
    """ A command that reaches a hardware register completes without WriteFailed """
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_event(
        "CuriosityReference.pinOut.SET",
        ["HIGH"],
        events=[fprime_test_api.get_event_pred("CuriosityReference.cmdDisp.OpCodeCompleted")],
        timeout=5,
    )
    assert not fprime_test_api.get_event_test_history().retrieve(
        fprime_test_api.get_event_pred("CuriosityReference.pinOut.WriteFailed")
    )
