#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <zephyr.h>
#include <errno.h>
#include <time.h>
#include <date_time.h>

#include <modem/lte_lc.h>
#include <net/mqtt.h>
#include <net/socket.h>

//Configures modem to provide LTE link. Blocks until link is successfully established.
void modem_configure(void)
{

  /* Turn off LTE power saving features for a more responsive demo. Also,
   * request power saving features before network registration. Some
   * networks rejects timer updates after the device has registered to the
   * LTE network.
   */
  printk("Disabling PSM and eDRX");
  lte_lc_psm_req(false);
  lte_lc_edrx_req(false);


  if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
          /* Do nothing, modem is already turned on
           * and connected.
           */
  } else {
    int err;

    printk("LTE Link Connecting...");
    err = lte_lc_init_and_connect();
    if (err) {
            printk("Failed to establish LTE connection: %d", err);
            return err;
    }
    printk("LTE Link Connected!");
  }
}

/* Semaphore used to block the main thread until the link controller has
 * established an LTE connection.
 */
K_SEM_DEFINE(lte_connected, 0, 1);

uint8_t connection_status = 0;

static void lte_handler(const struct lte_lc_evt *const evt)
{
     switch (evt->type) {
     case LTE_LC_EVT_NW_REG_STATUS:
             if (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING) {
                      connection_status = 5;//variable del estado conectado a true
                      printk("Estado de conexion cambiado a CONECTADO \n");
             }
             else if(evt->nw_reg_status == LTE_LC_NW_REG_NOT_REGISTERED){
                      connection_status = 0;//variable del estado conectado a true
                      printk("Estado de conexion SIN CONEXION \n");
             }
             k_sem_give(&lte_connected);
             break;
     
     default:
             break;
     }
}


void main(){
    //modem_configure(); 
    struct device *dev;
    dev = device_get_binding("GPIO_0");
    gpio_pin_configure(dev, 21, GPIO_INPUT);

    while(gpio_pin_get(dev, 21) == 1){
     printk("Hay tarjeta SIM, quita la tarjeta SIM \n");
     k_sleep(K_MSEC(3000));
    }
    
    printk("Se ha dejado de detectar la tarjeta SIM \n");

      
    while(gpio_pin_get(dev, 21) != 1){
      printk("No hay tarjeta SIM, pon la tarjeta SIM \n");
      k_sleep(K_MSEC(3000));

    }
    printk("Se ha detectado una tarjeta SIM \n");

    int err;

    printk("Connecting to LTE network. This may take a few minutes...\n");

    err = lte_lc_init_and_connect_async(lte_handler);
    if (err) {
            printk("lte_lc_init_and_connect_async, error: %d\n", err);
            return;
    }

    k_sem_take(&lte_connected, K_FOREVER);

   
}