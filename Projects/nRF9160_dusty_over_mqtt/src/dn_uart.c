/*
Copyright (c) 2015, Dust Networks. All rights reserved.

Port of the uart module to the MSP430FR4133.

On the MSP430FR4133 LaunchPad, we connect the SmartMesh IP device's serial
API to UCA0 using:
- P1.0 (UCA0TXD)
- P1.1 (UCA0RXD)

\license See attached DN_LICENSE.txt.

*/


#include "dn_uart.h"




//=========================== variables =======================================

typedef struct {
   dn_uart_rxByte_cbt   ipmt_uart_rxByte_cb;
} dn_uart_vars_t;

dn_uart_vars_t dn_uart_vars;


uint8_t uart_rx_buf[1024];
uint8_t uart_tx_buf[1024];
uint8_t idx_tx_buf = 0;
struct device *uart_dev;
struct uart_config _uart_cfg;

/*
static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);
struct uart_data_t {
	void  *fifo_reserved;
	u8_t    data[1];
	u16_t   len;
};*/


//=========================== interrupt handlers ==============================
/*
void uart_cb(const struct device* x, struct uart_event* evt, void *user_data){
   switch(evt->type){
      case UART_TX_DONE:
         printk("data transmitted!");
         break;
      case UART_RX_BUF_REQUEST:
         printk("uart_rx_enabled!");
         uart_rx_buf_rsp(x,&uart_rx_buf,1);
         break;
      case UART_RX_RDY:
         printk("byte received!");
         dn_uart_vars.ipmt_uart_rxByte_cb(uart_rx_buf[0]);
         break;
      default:
         printk("uncaught event!");

   }

} */

void uart_cb(struct device *x)
{
        //mandatory call to start processing interrupts
	uart_irq_update(x);
	int data_length = 0;

        //Check if UART RX buffer has a received char.
	if (uart_irq_rx_ready(x)) {
                //Read data from UART FIFO. Returns num of bytes read.
		data_length = uart_fifo_read(x, &uart_rx_buf[0], sizeof(uart_rx_buf[0]));
                //printk("received byte! -> %x\n", uart_rx_buf[0]); 
                dn_uart_vars.ipmt_uart_rxByte_cb(uart_rx_buf[0]);

	}
        //printk("%c\n",uart_rx_buf[0]);

        if (uart_irq_tx_ready(x)) {

            //Fill FIFO with data. Returns the num of bytes sent.
            
           /****************VA BIEN ******************************/
            int written = uart_fifo_fill(x, &uart_tx_buf[0], idx_tx_buf);
            while (idx_tx_buf > written) {
		written += uart_fifo_fill(x, &uart_tx_buf[written], idx_tx_buf - written);
            }
            idx_tx_buf = 0;
           // printk("data transmitted!\n");
            uart_irq_tx_disable(x);
            /***************VA BIEN******************************/
           
        }
	
}

//=========================== prototypes ======================================

//=========================== public ==========================================

void dn_uart_init(dn_uart_rxByte_cbt rxByte_cb){
   

    dn_uart_vars.ipmt_uart_rxByte_cb = rxByte_cb;


    //tell to nrf9160dk which UART is used.
    uart_dev = device_get_binding("UART_1");

  /*  DEBUGGING UART CONFIG PARAMETERS
    int ret = uart_config_get(uart_dev, &_uart_cfg);
    if(ret == 0){
      printf("UART CONFIGURATION:\n\n");
      printf("BAUDRATE: %d\n", _uart_cfg.baudrate);
      printf("PARITY: %d\n", _uart_cfg.parity);
      printf("STOP BITS: %d\n", _uart_cfg.stop_bits);
      printf("DATA BITS: %d\n", _uart_cfg.data_bits);
      printf("FLOW CONTROL: %d\n", _uart_cfg.flow_ctrl);
    }
    */

   /* uart_callback_set(uart_dev,(uart_callback_t) uart_cb, NULL);
    uart_rx_enable(uart_dev, uart_rx_buf, sizeof(uart_rx_buf), SYS_FOREVER_MS);
    uint8_t byte = 'w';
    uart_tx(uart_dev, &byte, 1, SYS_FOREVER_MS);
     */
 
    //bind the uart port used with its callback
    uart_irq_callback_set(uart_dev, uart_cb);
    //The callback is called when rx event happens.
    uart_irq_rx_enable(uart_dev);
    
   

}

void dn_uart_txByte(uint8_t byte){

    //uart_tx(uart_dev, &byte, 1, SYS_FOREVER_MS);

    uart_tx_buf[idx_tx_buf] = byte;
    ++idx_tx_buf;

    /****** VA BIEN *******/
    //uart_tx_buf[0] = byte;
    //uart_irq_tx_enable(uart_dev);
    //uart_irq_tx_disable(uart_dev);
    /******* VA BIEN *****/
}

void dn_uart_txFlush(){
   uart_irq_tx_enable(uart_dev);
   // nothing to do since MSP430 driver is byte-oriented
}

//=========================== private =========================================

//=========================== helpers =========================================

