module Drv {
    module AsyncSpiCfg {
        @ Slots (chip selects) per asynchronous SPI driver. The F Prime default is 10; this
        @ deployment wires one device (the F-RAM on SERCOM1) and keeps a little headroom.
        @ Each slot costs two port objects on the driver plus one pending-reply record.
        constant SpiDriverPorts = 4
    }
}
