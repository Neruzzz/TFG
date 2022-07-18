/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <inttypes.h>
#include <sys/printk.h>

//#define SW0_NODE    DT_ALIAS(sw0)
//#define SW0_GPIO_LABEL    DT_GPIO_LABEL(SW0_NODE, gpios)      //esto es la string "GPIO_0"
//#define SW0_GPIO_PIN    DT_GPIO_PIN(SW0_NODE, gpios)          //esto vale el número de pin del device que vayas a usar
#define BUTTON_FLAG    0x11

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   10


struct device *dev;

static struct gpio_callback button_cb_data;

int ledison = 0;

int contador = 0;

int start_time;

int stop_time;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins){
  stop_time = k_uptime_get_32();
  if(stop_time - start_time > 200){
    printk("boton pulsado %d \n", contador);
    if (ledison == 0){
      gpio_pin_set(dev, 2, 1);
      gpio_pin_set(dev, 3, 1);
      gpio_pin_set(dev, 4, 1);
      gpio_pin_set(dev, 5, 1);
      ledison = 1;
    }
    else{
      gpio_pin_set(dev, 2, 0);
      gpio_pin_set(dev, 3, 0);
      gpio_pin_set(dev, 4, 0);
      gpio_pin_set(dev, 5, 0);
      ledison = 0;
    }
    contador++;
  }
  start_time = k_uptime_get_32();
}


void main(void){
 
  dev = device_get_binding("GPIO_0");

  gpio_pin_configure(dev, 6, GPIO_INPUT | BUTTON_FLAG);
  gpio_pin_configure(dev, 7, GPIO_INPUT | BUTTON_FLAG);
  gpio_pin_configure(dev, 2, GPIO_OUTPUT); //p0.02 == LED1 (EL ACTIVE LO PONE ENCENDIDO)
  gpio_pin_configure(dev, 3, GPIO_OUTPUT); //p0.03 == LED2
  gpio_pin_configure(dev, 4, GPIO_OUTPUT); //p0.04 == LED3
  gpio_pin_configure(dev, 5, GPIO_OUTPUT); //p0.05 == LED4

  gpio_pin_interrupt_configure(dev, 7, GPIO_INT_EDGE_TO_ACTIVE);
  
  gpio_init_callback(&button_cb_data, button_pressed, BIT(7));
  gpio_add_callback(dev, &button_cb_data);
  start_time = k_uptime_get_32();
}
