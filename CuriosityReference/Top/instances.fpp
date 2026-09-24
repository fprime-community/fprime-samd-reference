module CuriosityReference {
    # ----------------------------------------------------------------------
    # Queued component instances
    # ----------------------------------------------------------------------

    # None. This deployment is bare-metal with no RTOS, so there are no queued
    # and no active component instances -- every instance below is passive and
    # runs on the caller's stack. See Main.cpp for the single main loop.

    # ----------------------------------------------------------------------
    # Passive component instances
    # ----------------------------------------------------------------------

    enum FixedBaseIds {
        @ Common base id to use for components that have no dictionary items
        NO_DICTIONARY = 0xFFFF
    }

    instance dmaDriver: Samd21.DmaDriver base id 0x3000
    instance comDriver: Samd21.UsartDriver base id 0x3020 {
        phase Fpp.ToCpp.Phases.configComponents """
        // Configure GPIO pins for SERCOM0
        Samd21::PinMux::configure(PINMUX_PA08C_SERCOM0_PAD0);  // PA8 -> SERCOM0 PAD[0] (TX)
        Samd21::PinMux::configure(PINMUX_PA09C_SERCOM0_PAD1);  // PA9 -> SERCOM0 PAD[1] (RX)

        comDriver.configure(
        Samd21::SercomKind::SERCOM_0,
        Samd21::UsartDriver::RxPinOut::PAD1,
        Samd21::UsartDriver::TxPinOut::PAD0,
        Samd21::UsartDriver::ClockMode::INTERNAL,
        Samd21::UsartDriver::CommunicationMode::ASYNC,
        Samd21::UsartDriver::BaudRate::BAUD_115200,
        Samd21::UsartDriver::DataOrder::LSB_FIRST,
        Samd21::UsartDriver::DataBits::BITS_8,
        Samd21::UsartDriver::StopBits::ONE,
        Samd21::UsartDriver::Parity::NONE
        );
        """
    }

    instance i2cDriver: Samd21.I2cDriver base id 0x3030 {
        phase Fpp.ToCpp.Phases.configComponents """
        // Configure GPIO pins for SERCOM4
        Samd21::PinMux::configure(PINMUX_PA12D_SERCOM4_PAD0);  // PA12 -> SERCOM4 PAD[0] (SDA)
        Samd21::PinMux::configure(PINMUX_PA13D_SERCOM4_PAD1);  // PA13 -> SERCOM4 PAD[1] (SCL)

        i2cDriver.configure(
        Samd21::SercomKind::SERCOM_4,
        Samd21::I2cDriver::SclLowTimeout::DISABLED,
        Samd21::I2cDriver::InactiveTimeout::DISABLED,
        Samd21::I2cDriver::ClockStretchMode::ALWAYS,
        Samd21::I2cDriver::Frequency::FAST_400KHZ,
        // All four SMBus time-outs disabled for bringup: they are driven by
        // GCLK_SERCOM_SLOW and set BUSERR when they trip during a frame (e.g. the
        // host SCL-extend time-out fires while the host holds SCL low waiting on the
        // RX DMA). Disabling them isolates a genuine protocol/electrical BUSERR from
        // a time-out-induced one -- re-enable once reads work.
        Samd21::I2cDriver::ClientSclLowTimeout::DISABLED,
        Samd21::I2cDriver::HostSclLowTimeout::DISABLED,
        Samd21::I2cDriver::SdaHold::HOLD_75_NS,
        Samd21::I2cDriver::PinUsage::TWO_WIRE,
        Samd21::I2cDriver::RunInStandby::ENABLED
        );
        """
    }

    instance comStub: Svc.ComStub base id FixedBaseIds.NO_DICTIONARY
    instance fatalFramer: Samd21.SyncFramer base id FixedBaseIds.NO_DICTIONARY
    instance downlink: Samd21.PassiveDownlink base id FixedBaseIds.NO_DICTIONARY
    instance cycler: Svc.PassiveCycler base id FixedBaseIds.NO_DICTIONARY

    @ Realtime clock driven rate driver, puts the CPU to sleep in-between cycles
    instance rateDriver: Samd21.RtcDriver base id 0x1000 {
        @ Cycle the clock @ 8Hz
        phase Fpp.ToCpp.Phases.configComponents """
        rateDriver.configure(
        Samd21::RtcDriver::ClockSource::UltraLowPowerOscillator,
        Samd21::RtcDriver::TickRate::TICK_8_HZ
        );
        """

        @ Enable the RTC peripheral and the unmask the NVIC
        phase Fpp.ToCpp.Phases.startTasks """
        rateDriver.enable();
        """
    }

    instance fatalHandler: Samd21.FatalHandler base id 0
    instance rg8Hz: Svc.PassiveRateGroup base id 0x1020  @< 8Hz frequency
    instance rg1Hz: Svc.PassiveRateGroup base id 0x1030  @< 1Hz frequency
    instance rg10s: Svc.PassiveRateGroup base id 0x1040  @< 10s period

    instance timeHandler: Samd21.Samd21Time base id 0x2000
    instance rateGroupDriver: Samd21.PassiveRateGroupDriver base id 0x2010
    instance cmdDisp: Samd21.StaticCmdDispatcher base id 0x2020
    instance tlm: Samd21.StaticTlmPacketizer base id 0x2030

    # ----------------------------------------------------------------------
    # Uplink Components
    # ----------------------------------------------------------------------

    instance frameAccumulator: Svc.FrameAccumulator base id 0xAA00 {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::FrameDetectors::FprimeFrameDetector frameDetector;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        frameAccumulator.configure(
            ConfigObjects::CuriosityReference_frameAccumulator::frameDetector,
            1,
            Allocation::frameAccumulatorAllocator,
            /* frame buffer size */ 128
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        frameAccumulator.cleanup();
        """
    }

    instance commsBufferManager: Svc.StaticMemory base id 0xAA10

    instance deframer: Svc.FprimeDeframer base id 0xAA20
    instance fprimeRouter: Samd21.FprimeRouter base id 0xAA30
    instance framer: Samd21.Framer base id 0xAA40
    instance fwHealth: CuriosityReference.FrameworkHealth base id 0xAA50

    # ----------------------------------------------------------------------
    # GPIO Components
    #
    # pinIn/inPA23 and pinOut/outPA25 are pairs: the Curiosity* component owns
    # the commands and events, the Samd21.GpioDriver instance owns the pin. The
    # base ids are deliberately tight (4 / 1 / 4 ids of headroom) -- adding a
    # command or event to GpioIn/GpioOut may require re-spacing them.
    # ----------------------------------------------------------------------

    instance pinIn: CuriosityReference.GpioIn base id 0xBB00

    instance inPA23: Samd21.GpioDriver base id 0xBB04 {
        phase Fpp.ToCpp.Phases.startTasks """
        inPA23.configureInput(
            Samd21::GpioDriver::Group::PA,
            Samd21::GpioDriver::Pin::PIN_23,
            Samd21::GpioDriver::InputPullMode::PULL_UP,
            Samd21::GpioDriver::ExternalInterruptMode::BOTH
        );
        """
    }

    instance pinOut: CuriosityReference.GpioOut base id 0xBB05

    instance outPA25: Samd21.GpioDriver base id 0xBB09 {
        phase Fpp.ToCpp.Phases.startTasks """
        outPA25.configureOutput(
            Samd21::GpioDriver::Group::PA,
            Samd21::GpioDriver::Pin::PIN_25
        );
        """
    }

    # ----------------------------------------------------------------------
    # I2C Components
    # ----------------------------------------------------------------------

    @ Exercises the I2C bus. See CuriosityReference/I2cTester/docs/sdd.md.
    instance i2cTester: CuriosityReference.I2cTester base id 0xCC00 {
        @ startTasks, not configComponents: configure() logs InvalidConfig on a
        @ bad argument, which needs the event and time ports already connected.
        @
        @ 0x50 is the 24Cxx serial-EEPROM address -- those parts are byte
        @ addressed, so "write offset, then read" is a valid EEPROM read and an
        @ EEPROM breakout is the cheapest target to hang on the bus. Nothing has
        @ to be attached: with an empty bus the tester still drives the full
        @ driver + DMA path and reports the NACK as I2cTester.WriteErrors.
        @
        @ Polling is OFF after configure(). Send I2CTESTER.SET_POLLING(true) to
        @ start the 1 Hz read; a freshly reset board does not drive the bus.
        phase Fpp.ToCpp.Phases.startTasks """
        i2cTester.configure(
        /* i2c address */ 0x50,
        /* reg offset  */ 0x00,
        /* read length */ 4
        );
        """
    }
}
