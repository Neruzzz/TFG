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

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000


void main(void)
{
  struct device *dev;
  dev = device_get_binding("GPIO_0");
  /* Set LED pin as output */
  gpio_pin_configure(dev, 2, GPIO_OUTPUT_ACTIVE); //p0.02 == LED1
  gpio_pin_configure(dev, 3, GPIO_OUTPUT_ACTIVE); //p0.03 == LED2
  gpio_pin_configure(dev, 4, GPIO_OUTPUT_ACTIVE); //p0.04 == LED3
  gpio_pin_configure(dev, 5, GPIO_OUTPUT_ACTIVE); //p0.05 == LED4

  gpio_pin_set(dev, 2, 1);
  gpio_pin_set(dev, 3, 1);  
  gpio_pin_set(dev, 4, 1);
  gpio_pin_set(dev, 5, 1);

}