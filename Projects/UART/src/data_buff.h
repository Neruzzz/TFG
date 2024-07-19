/*
 * Wireless Networks Research Lab @ UOC
 * Author: Aaron Acosta
 * Date: 11/12/2020
 * License: BSD
 */

#ifndef data_buff
#define data_buff

#include <zephyr.h>
#include <string.h>
#include "dn_ipmg.h"

//=========================== defines =====================================

//Data buffer defines
#define MAX_DATA_BUF_ITEM_LENGTH 700
#define MAX_DATA_BUF_LENGTH 16

//Mutex definition
static K_MUTEX_DEFINE(my_mutex);

//=========================== typedefs =====================================

typedef struct {
  uint8_t used;
  size_t len;
  uint8_t data[MAX_DATA_BUF_ITEM_LENGTH];
} data_buf_item_t;

//=========================== variables =====================================

static data_buf_item_t data_buffer[MAX_DATA_BUF_LENGTH];

//=========================== prototypes =====================================
void init_data_buffer();
data_buf_item_t* store_to_data_buffer(uint8_t* data, size_t len);
void free_data_buffer_item(data_buf_item_t* item);

#endif