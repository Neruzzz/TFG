#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <inttypes.h>
#include <sys/printk.h>

#define BATT_SUPPLY 15
#define BATT_CHARGE 16
#define P_GOOD 19


void main(void){
  struct device *dev;
  dev = device_get_binding("GPIO_0");
  gpio_pin_configure(dev, BATT_SUPPLY, GPIO_INPUT);
  gpio_pin_configure(dev, BATT_CHARGE, GPIO_INPUT);
  gpio_pin_configure(dev, P_GOOD, GPIO_INPUT);

  while(1){
    printk("result: %d \n", gpio_pin_get(dev, BATT_SUPPLY));
    k_sleep(K_MSEC(500));
 }


/*
  printk("Se va a comprobar el funcionamiento de la señal BATT_SUPPLY \n");
  printk("Desconecta la bateria principal \n");
  k_sleep(K_MSEC(5000));
  while(gpio_pin_get(dev, BATT_SUPPLY) == 1){//fuente principal conectada
    printk("Desconecta la bateria principal!!!!!!! \n");
    k_sleep(K_MSEC(5000));
  }
  printk("CORRECTO, la bateria principal esta DESCONECTADA \n");
  printk("Ahora CONECTA la bateria principal \n");
  k_sleep(K_MSEC(5000));
  while(gpio_pin_get(dev, BATT_SUPPLY) == 0){// fuente principal desconectada
    printk("Ahora CONECTA la bateria principal!!!!!! \n");
    k_sleep(K_MSEC(5000));
  }
  printk("CORRECTO, la bateria principal esta CONECTADA \n\n");
  k_sleep(K_MSEC(5000));


  printk("A continuacion se va a comprobar el funcionamiento de la señal BATT_CHARGE \n");
  printk("Desinstala la bateria  \n");
  k_sleep(K_MSEC(5000));
  while(gpio_pin_get(dev, BATT_CHARGE) == 0){
    printk("Desinstala la bateria!!!!! \n");
  }
  printk("CORRECTO, la bateria de emergencia esta DESCARGADA/DESCONECTADA \n");
  printk("Instala la bateria  \n");
  k_sleep(K_MSEC(5000));
  while(gpio_pin_get(dev, BATT_CHARGE) == 0){
    printk("Instala la bateria!!!!  \n");
    k_sleep(K_MSEC(5000));
  }
  printk("CORRECTO, la bateria de emergencia esta CARGADA/INSTALADA \n\n");



  printk("Se va a comprobar el funcionamiento de la señal P_GOOD \n");
  printk("Desconecta la bateria principal \n");
  k_sleep(K_MSEC(5000));
  while(gpio_pin_get(dev, P_GOOD) == 1){
    printk("Desconecta la bateria principal!!!!!! \n");
    k_sleep(K_MSEC(5000));
  }
  printk("CORRECTO, la bateria principal da un voltaje inferior a 90% del voltaje nominal.\n");
  printk("Conecta la bateria principal \n");
  k_sleep(K_MSEC(5000));
  while(gpio_pin_get(dev, P_GOOD) == 1){
    printk("Conecta la bateria principal!!!!!! \n");
    k_sleep(K_MSEC(5000));
  }
  printk("CORRECTO, la bateria principal da un voltaje superior al 95% del voltaje nominal \n\n"); */
}
