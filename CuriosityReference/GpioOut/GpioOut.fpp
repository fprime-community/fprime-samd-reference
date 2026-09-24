module CuriosityReference {
    @ Drives one GPIO output pin to a commanded logic level.
    @
    @ The write half of the Samd21.GpioDriver demonstration, and the shortest
    @ path in the deployment from a ground command to a hardware register: the
    @ command handler calls Drv.GpioWrite, which lands in the driver's
    @ gpioWrite handler, which writes the SAMD21 PORT peripheral -- all
    @ synchronously, on the command dispatcher's stack.
    @
    @ Drv.GpioWrite returns a Drv.GpioStatus and this component checks it, so a
    @ SET against a driver instance that was never configured (NOT_OPENED) or
    @ that was configured as an input (INVALID_MODE) fails the command instead
    @ of reporting a success that never reached a pin. That is the reason this
    @ component imports Fw.Event and takes a time port at all.
    @
    @ One instance owns exactly one pin -- write is always invoked on index 0 --
    @ so a second pin means a second GpioOut instance paired with a second
    @ Samd21.GpioDriver instance.
    passive component GpioOut {

        @ Port for writing a logic level to a GPIO pin.
        @ Wired to Samd21.GpioDriver.gpioWrite.
        output port write: Drv.GpioWrite

        @ Set the logic level of the GPIO
        sync command SET(
            $state: Fw.Logic @< Level to drive on the pin
        ) \
            opcode 0

        @ The driver rejected the write and the pin was not changed. NOT_OPENED
        @ means the paired Samd21.GpioDriver instance was never configured;
        @ INVALID_MODE means it was configured as an input.
        event WriteFailed(
            status: Drv.GpioStatus @< Status returned by the driver
            $state: Fw.Logic       @< Level that was requested
        ) \
            severity warning high \
            id 0 \
            format "GPIO write failed: {}, requested level {}"

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
