module CuriosityReference {
    # ----------------------------------------------------------------------
    # Symbolic constants for port numbers
    # ----------------------------------------------------------------------

    enum Ports_ComPacketQueue : U8 {
        EVENTS
        TELEMETRY
        FATAL
    }

    enum Ports_RateGroups {
        rateGroup1
        rateGroup2
        rateGroup3
    }

    @ Every DMA channel used by the deployment. The SAMD21 DMAC has 12 channels and
    @ Samd21.DmaDriver sizes its port arrays from this enum, so a channel that is not
    @ listed here cannot be used -- and two peripherals must never be given the same
    @ index.
    enum DmaChannel : U8 {
        USART0_UPL_DWN_TX
        USART0_UPL_DWN_RX
        SERCOM4_I2C_WRITE
        SERCOM4_I2C_READ
    }

    deployment topology Top {
        # ----------------------------------------------------------------------
        # Instances used in the topology
        # ----------------------------------------------------------------------

        instance fatalHandler
        instance rateDriver
        instance rg8Hz
        instance rg1Hz
        instance rg10s
        instance cycler
        instance rateGroupDriver
        instance timeHandler
        instance cmdDisp
        instance fwHealth

        instance comDriver
        instance i2cDriver

        instance downlink
        instance framer

        instance comStub
        instance deframer
        instance fprimeRouter
        instance commsBufferManager
        instance frameAccumulator
        instance dmaDriver
        instance fatalFramer
        instance i2cTester

        instance pinIn
        instance inPA23
        instance pinOut
        instance outPA25

        instance tlm

        # ----------------------------------------------------------------------
        # Telemetry packets
        #
        # Samd21.StaticTlmPacketizer builds its channel -> packet table at compile
        # time from this block, so EVERY telemetry channel of EVERY instance above
        # must appear either in a packet or in the `omit` list. An unaccounted
        # channel is a hard FPP error, not a warning -- which is the point: adding a
        # channel to a component forces a decision about whether it is downlinked.
        #
        # Packet ids are positional (Error = 0, Tester = 1, Health = 2). The group
        # number selects the downlink group the packetizer assigns the packet to.
        #
        # NOTE: nothing is wired to `tlm.pktSendIn`, so packets are sent only on
        # ground command (TLM.SEND_PKT). To downlink a packet periodically instead,
        # connect a rate group to `tlm.pktSendIn[<packet id>]` and raise
        # Samd21.NUM_TLM_PACKETS (default 1) above the largest port-driven packet id.
        # ----------------------------------------------------------------------

        telemetry packets Main {

            packet Error group 1 {
                i2cDriver.BusErrorCount
                framer.DroppedPackets
                # Per-failure-mode I2C counters. Which one is climbing identifies
                # the fault; see CuriosityReference/I2cTester/docs/sdd.md §3.6.
                i2cTester.BusyRejects
                i2cTester.AddressErrors
                i2cTester.WriteErrors
                i2cTester.ReadErrors
                i2cTester.OpenErrors
                i2cTester.OtherErrors
                i2cTester.ShortReads
            }

            packet Tester group 0 {
                i2cTester.ReadData0
                i2cTester.ReadData1
                i2cTester.ReadLength
                i2cTester.TxnCount
                i2cTester.TargetAddress
                i2cTester.RegisterOffset
            }

            packet Health group 0 {
                rateDriver.CycleOverrun
                fwHealth.Counter
                rg8Hz.CycleCount
                rateGroupDriver.MaxCycleTime
                rg8Hz.MaxCycleTime
                rg1Hz.MaxCycleTime
                rg10s.MaxCycleTime
                comDriver.rxOverflows
                comDriver.rxBytes
                comDriver.txBytes
                i2cDriver.BusState
                i2cDriver.ClockHold
                i2cDriver.ReceiveNotAcknowledged
                i2cDriver.DeviceOnBus
                i2cDriver.StallRecoveryCount
            }

        } omit {
            # Instantaneous and per-port cycle times. The high-water marks
            # (MaxCycleTime) are downlinked in the Health packet instead: they are
            # what a 16 KiB, 128 KiB-flash board actually needs, and PortCycleTime /
            # PortCycleTimeHWM are arrays of PassiveRateGroupOutputPorts entries,
            # which is more than a 128-byte packet can carry.
            rg8Hz.CycleTime
            rg8Hz.PortCycleTime
            rg8Hz.PortCycleTimeHWM

            rg1Hz.CycleTime
            rg1Hz.PortCycleTime
            rg1Hz.PortCycleTimeHWM

            rg10s.CycleTime
            rg10s.PortCycleTime
            rg10s.PortCycleTimeHWM

            rateGroupDriver.CycleTime

            # rg8Hz.CycleCount is downlinked; the slower groups are derivable from
            # it (divisors 8 and 80) and would just consume packet space.
            rg1Hz.CycleCount
            rg10s.CycleCount
        }

        # ----------------------------------------------------------------------
        # Pattern graph specifiers
        # ----------------------------------------------------------------------

        command connections instance cmdDisp

        event connections instance downlink

        telemetry connections instance tlm

        time connections instance timeHandler

        # ----------------------------------------------------------------------
        # Direct graph specifiers
        # ----------------------------------------------------------------------

        @ Components that need to be serviced from the main context rather than from
        @ an ISR. Each one latches work in its interrupt handler and completes it
        @ here, on cycler.cycle() in Main.cpp. Dropping one of these connections does
        @ not fail the build -- it silently stops that driver from ever delivering a
        @ completion.
        connections ActiveRateGroups {
            cycler.cycleOut -> comDriver.activeIn
            cycler.cycleOut -> rateDriver.activeIn
            cycler.cycleOut -> i2cDriver.activeIn
        }

        connections RateGroups {
            # Block driver
            rateDriver.CycleOut -> rateGroupDriver.CycleIn

            # A PassiveRateGroup calls its members in ascending port-index order,
            # synchronously, on the rate driver's stack. The indices below are
            # therefore an execution order, not just wiring, and every one of them is
            # written out explicitly -- an unindexed connection takes the next free
            # slot, which means inserting a member silently renumbers the ones after
            # it. Unconnected slots in between are skipped at no cost.

            # Rate group 1 -- 8 Hz
            rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1] -> rg8Hz.CycleIn
            rg8Hz.RateGroupMemberOut[0]                           -> comDriver.schedIn

            # Rate group 2 -- 1 Hz
            rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup2] -> rg1Hz.CycleIn
            rg1Hz.RateGroupMemberOut[0]                           -> i2cDriver.reportTelemetryIn
            rg1Hz.RateGroupMemberOut[1]                           -> i2cTester.schedIn
            # framer.schedIn sits in the LAST slot on purpose: it drains the com
            # packet queue, so it must run after every member that may have pushed
            # telemetry or an event into that queue this cycle.
            rg1Hz.RateGroupMemberOut[PassiveRateGroupOutputPorts - 1] -> framer.schedIn

            # Rate group 3 -- 10 s period
            rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup3] -> rg10s.CycleIn
            rg10s.RateGroupMemberOut[0]                           -> fwHealth.schedIn
        }

        @ FATAL path. fatalFramer writes straight out of comDriver.sendSync, bypassing
        @ the queued downlink pipeline, because a FATAL report has to reach the ground
        @ from inside the assert hook -- with the rest of the system already declared
        @ untrustworthy.
        connections FaultProtection {
            downlink.FatalAnnounce -> fatalHandler.FatalReceive
            fatalFramer.drvSendOut -> comDriver.sendSync
        }

        connections Link {
            comDriver.ready -> framer.drvConnected

            # Telemetry downlink pipeline
            downlink.PktSend        -> framer.comPacketQueueIn
            tlm.pktSendOut          -> framer.comPacketQueueIn
            framer.drvSendOut       -> comDriver.$send
            comDriver.sendReturnOut -> framer.drvReturnIn

            # TX
            comDriver.dmaQueueOut[Samd21.UsartDriver.DmaChannel.TX]   -> dmaDriver.sendTransactionIn[DmaChannel.USART0_UPL_DWN_TX]
            dmaDriver.transactionIsrOut[DmaChannel.USART0_UPL_DWN_TX] -> comDriver.dmaReplyIn[Samd21.UsartDriver.DmaChannel.TX]

            # RX
            comDriver.dmaQueueOut[Samd21.UsartDriver.DmaChannel.RX]   -> dmaDriver.sendTransactionIn[DmaChannel.USART0_UPL_DWN_RX]
            dmaDriver.transactionIsrOut[DmaChannel.USART0_UPL_DWN_RX] -> comDriver.dmaReplyIn[Samd21.UsartDriver.DmaChannel.RX]
            comDriver.dmaRxCircular                                   -> dmaDriver.linkToFrontIn[DmaChannel.USART0_UPL_DWN_RX]
            comDriver.dmaRxRead                                       -> dmaDriver.readWritebackIn[DmaChannel.USART0_UPL_DWN_RX]
        }

        connections Uplink {
            # ComDriver <-> ComStub (Uplink)
            comDriver.$recv             -> comStub.drvReceiveIn
            comStub.drvReceiveReturnOut -> comDriver.recvReturnIn

            # ComStub <-> FrameAccumulator (Uplink)
            comStub.dataOut                -> frameAccumulator.dataIn
            frameAccumulator.dataReturnOut -> comStub.dataReturnIn

            # (Incoming) ComInterface <-> FrameAccumulator connections shall be established by the user
            # FrameAccumulator buffer allocations
            frameAccumulator.bufferDeallocate -> commsBufferManager.bufferDeallocate[0]
            frameAccumulator.bufferAllocate   -> commsBufferManager.bufferAllocate[0]
            # FrameAccumulator <-> Deframer
            frameAccumulator.dataOut -> deframer.dataIn
            deframer.dataReturnOut   -> frameAccumulator.dataReturnIn
            # Deframer <-> Router
            deframer.dataOut           -> fprimeRouter.dataIn
            fprimeRouter.dataReturnOut -> deframer.dataReturnIn

            # Router <-> CmdDispatcher
            fprimeRouter.commandOut -> cmdDisp.seqCmdBuff
            cmdDisp.seqCmdStatus    -> fprimeRouter.cmdResponseIn
        }

        # DMA Engine drives the I2C transactions
        connections I2c {
            i2cDriver.dmaTransactionOut[Samd21.I2cDriver.DmaChannel.WRITE]      -> dmaDriver.sendTransactionIn[DmaChannel.SERCOM4_I2C_WRITE]
            i2cDriver.dmaTransactionAbortOut[Samd21.I2cDriver.DmaChannel.WRITE] -> dmaDriver.abortTransactionIn[DmaChannel.SERCOM4_I2C_WRITE]
            dmaDriver.transactionIsrOut[DmaChannel.SERCOM4_I2C_WRITE]           -> i2cDriver.dmaReplyIn[Samd21.I2cDriver.DmaChannel.WRITE]

            i2cDriver.dmaTransactionOut[Samd21.I2cDriver.DmaChannel.READ]      -> dmaDriver.sendTransactionIn[DmaChannel.SERCOM4_I2C_READ]
            i2cDriver.dmaTransactionAbortOut[Samd21.I2cDriver.DmaChannel.READ] -> dmaDriver.abortTransactionIn[DmaChannel.SERCOM4_I2C_READ]
            dmaDriver.transactionIsrOut[DmaChannel.SERCOM4_I2C_READ]           -> i2cDriver.dmaReplyIn[Samd21.I2cDriver.DmaChannel.READ]
        }

        @ Attach the I2C client to the bus driver. Both request/callback pairs are
        @ left unindexed: I2cDriver.fpp declares `match writeComplete with write` and
        @ `match writeReadComplete with writeRead`, so FPP guarantees a request and
        @ its callback land on the same client port index. Both callbacks MUST be
        @ connected -- the driver guards each with isConnected_* and otherwise drops
        @ the completion silently, wedging the client mid-transaction forever.
        connections I2cTester {
            i2cTester.i2cWriteReadOut   -> i2cDriver.writeRead
            i2cDriver.writeReadComplete -> i2cTester.writeReadCompleteIn
            i2cTester.i2cWriteOut       -> i2cDriver.write
            i2cDriver.writeComplete     -> i2cTester.writeCompleteIn
        }

        connections Pins {
            # Input. gpioInterrupt is emitted from the EIC ISR -- see
            # CuriosityReference/GpioIn/docs/sdd.md.
            pinIn.readOut        -> inPA23.gpioRead
            inPA23.gpioInterrupt -> pinIn.transitionIn

            # Output
            pinOut.write -> outPA25.gpioWrite
        }
    }
}
