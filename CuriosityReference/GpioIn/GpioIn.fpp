module CuriosityReference {
    @ Reads and reports the level of one GPIO input pin.
    @
    @ Exercises both halves of the Samd21.GpioDriver input path:
    @   1. the synchronous Drv.GpioRead port, on demand from a ground command;
    @   2. the EIC edge notification the driver emits on Drv.Gpio's
    @      gpioInterrupt output, which is a Svc.Cycle port.
    @
    @ Drv.GpioRead returns a Drv.GpioStatus and this component always checks it,
    @ so a read against a driver instance that was never configured (NOT_OPENED)
    @ or that was configured as an output (INVALID_MODE) reports a failure
    @ instead of faulting the board.
    @
    @ One instance owns exactly one pin -- readOut is always invoked on index 0 --
    @ so a second pin means a second GpioIn instance paired with a second
    @ Samd21.GpioDriver instance.
    @
    @ WARNING: transitionIn is invoked from the SAMD21 EIC interrupt handler.
    @ Everything reached from that handler, including event emission, runs in ISR
    @ context. See docs/sdd.md before adding work to it.
    passive component GpioIn {

        @ Port for reading the state of a GPIO pin from the driver.
        @ Wired to Samd21.GpioDriver.gpioRead.
        output port readOut: Drv.GpioRead

        @ Incoming message signaling we saw a GPIO edge.
        @ Driven by Samd21.GpioDriver.gpioInterrupt, which the driver emits from
        @ ISR context -- so this handler runs in ISR context too. Svc.Cycle is
        @ the port type the Drv.Gpio interface uses for the notification; the
        @ cycle start time it carries is not used here.
        sync input port transitionIn: Svc.Cycle

        @ Read the logic level of the GPIO pin and emit an event
        sync command READ opcode 0

        @ Level reported by a commanded READ
        event ReadLevel($state: Fw.Logic) \
            severity activity low \
            id 0 \
            format "Gpio Pin logic level: {}"

        @ The pin changed level. Reported from the driver's EIC interrupt, so the
        @ level in this event is sampled after the edge, not at the edge.
        event LevelTransitioned($state: Fw.Logic) \
            severity activity low \
            id 1 \
            format "GPIO level transitioned to: {}"

        @ The driver rejected a read. NOT_OPENED means the paired
        @ Samd21.GpioDriver instance was never configured; INVALID_MODE means it
        @ was configured as an output.
        event ReadFailed(
            status: Drv.GpioStatus @< Status returned by the driver
        ) \
            severity warning high \
            id 2 \
            format "GPIO read failed: {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

    }
}
