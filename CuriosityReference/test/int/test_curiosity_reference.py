""" CuriosityReference hardware integration tests

Run through fprime-ci (see ci/curiosity-nano.yml) or directly against a running GDS:

    pytest CuriosityReference/test/int/test_curiosity_reference.py \
        --dictionary build-artifacts/microchip_curiosity/CuriosityReference/dict/TopTopologyDictionary.json
"""


def test_no_op_command(fprime_test_api):
    """ Command uplink and event downlink: NO_OP is received, dispatched, and completed """
    fprime_test_api.send_and_assert_event(
        "CuriosityReference.cmdDisp.CMD_NO_OP",
        events=[fprime_test_api.get_event_pred("CuriosityReference.cmdDisp.NoOpReceived")],
        timeout=5,
    )
    fprime_test_api.assert_event("CuriosityReference.cmdDisp.OpCodeCompleted", timeout=5)


def test_health_packet_telemetry(fprime_test_api):
    """ Telemetry downlink: requesting the Health packet produces its channels """
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_event(
        "CuriosityReference.tlm.SEND_PKT",
        ["2"],
        events=[fprime_test_api.get_event_pred("CuriosityReference.cmdDisp.OpCodeCompleted")],
        timeout=5,
    )
    fprime_test_api.assert_telemetry("CuriosityReference.fwHealth.Counter", timeout=5)
    fprime_test_api.assert_telemetry("CuriosityReference.comDriver.rxBytes", timeout=5)
