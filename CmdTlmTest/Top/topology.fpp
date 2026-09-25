module CmdTlmTest {
    deployment topology Top {
        # ----------------------------------------------------------------------
        # Instances used in the topology
        # ----------------------------------------------------------------------

        instance frameAccumulator
        instance commsBufferManager
        instance deframer
        instance fprimeRouter
        instance cmdDisp
        instance target

        instance tlm
        instance framer
        instance comSink

        instance eventSink
        instance timeHandler

        telemetry packets Main {
            packet Test group 0 {
                target.Value
                framer.DroppedPackets
            }
        }

        # ----------------------------------------------------------------------
        # Pattern graph specifiers
        # ----------------------------------------------------------------------

        command connections instance cmdDisp

        event connections instance eventSink

        telemetry connections instance tlm

        time connections instance timeHandler

        # ----------------------------------------------------------------------
        # Direct graph specifiers
        # ----------------------------------------------------------------------

        @ Uplink. The test injects raw bytes at frameAccumulator.dataIn --
        @ exactly where comStub.dataOut feeds it in CuriosityReference's
        @ topology -- so nothing upstream of the accumulator exists here.
        connections Uplink {
            frameAccumulator.bufferDeallocate -> commsBufferManager.bufferDeallocate[0]
            frameAccumulator.bufferAllocate   -> commsBufferManager.bufferAllocate[0]

            frameAccumulator.dataOut       -> deframer.dataIn
            deframer.dataReturnOut         -> frameAccumulator.dataReturnIn

            # Returns ownership of the buffer the test injected at dataIn --
            # comStub.dataReturnIn's role in CuriosityReference's topology.
            frameAccumulator.dataReturnOut -> comSink.dataReturnIn

            deframer.dataOut               -> fprimeRouter.dataIn
            fprimeRouter.dataReturnOut     -> deframer.dataReturnIn

            fprimeRouter.commandOut        -> cmdDisp.seqCmdBuff
            cmdDisp.seqCmdStatus            -> fprimeRouter.cmdResponseIn
        }

        @ Downlink. framer's driver-side ports are wired to comSink instead of
        @ a real Samd21.UsartDriver, matching CuriosityReference's `Link`
        @ connections one-for-one against comDriver.
        connections Downlink {
            tlm.pktSendOut          -> framer.comPacketQueueIn
            framer.drvSendOut       -> comSink.$send
            comSink.sendReturnOut   -> framer.drvReturnIn
            comSink.ready           -> framer.drvConnected
        }
    }
}
