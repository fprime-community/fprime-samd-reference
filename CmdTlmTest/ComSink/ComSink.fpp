module CmdTlmTest {
    @ Stands in for the byte-stream driver this topology has neither on the
    @ uplink nor the downlink side: Samd21.UsartDriver's role for
    @ Samd21.Framer (downlink) and for Svc.ComStub (uplink buffer return).
    @
    @ Test-only: mirrors the ports UsartDriver/ComStub offer their neighbors
    @ in the real topology (CuriosityReference/Top/topology.fpp's `Link`
    @ connections: comDriver.ready / comDriver.$send / comDriver.sendReturnOut,
    @ and the Uplink connections' frameAccumulator.dataReturnOut ->
    @ comStub.dataReturnIn). There is no DMA to wait on here, so the buffer is
    @ captured and handed back synchronously, inside the $send call.
    passive component ComSink {

        @ Signals the driver is ready to send. The test fires this once
        @ during topology setup, wired to framer.drvConnected.
        output port ready: Drv.ByteStreamReady

        @ Receive a framed packet from Samd21.Framer. Copies the buffer
        @ contents into m_captured for the test to inspect via
        @ getCapturedData() / getCapturedSize(), then returns ownership via
        @ sendReturnOut.
        sync input port $send: Fw.BufferSend

        @ Returns ownership of the buffer received on $send, wired to
        @ framer.drvReturnIn.
        output port sendReturnOut: Drv.ByteStreamData

        @ Receives back ownership of the uplink buffer frameAccumulator was
        @ handed on dataIn, once it has extracted a frame (or given up on
        @ one) -- the same role comStub.dataReturnIn plays in production.
        @ The test injects stack-local buffers directly, so there is nothing
        @ to do with the returned buffer; this only needs to exist so
        @ frameAccumulator.dataReturnOut has somewhere to go.
        sync input port dataReturnIn: Svc.ComDataWithContext

    }
}
