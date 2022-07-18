## nRF52840_sample_route_uart_to_GPIO

This sample is used to flash the nRF52840 chip on the nRF9160DK. So, make sure you **select the nRF52 on the PROG/DEBUG switch in the board before flashing the chip**.

The purpose of this sample is to route the UART2 from VCOM2 port to the external GPIO pins of the board. This will allow us to communicate
the Dusty manager module with the application through the following nRF9160DK GPIO pins:

* P0.15 -> RTS   _Not used_
* P0.14 -> CTS   _Not used_
* P0.00 -> TXD   info enters to the nRF9160 through this pin
* P0.01 -> RXD   
