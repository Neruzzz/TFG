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


void main(void)
{
  struct device *dev;
  dev = device_get_binding("GPIO_0");
  gpio_pin_configure(dev, 6, GPIO_INPUT | BUTTON_FLAG);
  gpio_pin_configure(dev, 7, GPIO_INPUT | BUTTON_FLAG);
  gpio_pin_configure(dev, 2, GPIO_OUTPUT_ACTIVE); //p0.02 == LED1 (EL ACTIVE LO PONE ENCENDIDO)
  gpio_pin_configure(dev, 3, GPIO_OUTPUT_ACTIVE); //p0.03 == LED2
  gpio_pin_configure(dev, 4, GPIO_OUTPUT_ACTIVE); //p0.04 == LED3
  gpio_pin_configure(dev, 5, GPIO_OUTPUT_ACTIVE); //p0.05 == LED4
 
  while(1){
    while(gpio_pin_get(dev, 6) != 1){//lee el botón
    }

    gpio_pin_set(dev, 2, 0);
    gpio_pin_set(dev, 3, 0);
    gpio_pin_set(dev, 4, 0);
    gpio_pin_set(dev, 5, 0);

    while(gpio_pin_get(dev, 7) != 1){//lee el botón
    }

    gpio_pin_set(dev, 2, 1);
    gpio_pin_set(dev, 3, 1);
    gpio_pin_set(dev, 4, 1);
    gpio_pin_set(dev, 5, 1);
  }
}