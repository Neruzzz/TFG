/*
 * Wireless Networks Research Lab @ UOC
 * Author: Aaron Acosta
 * Date: 11/12/2020
 * License: BSD
 */

#include "data_buff.h"

void init_data_buffer(){
  k_mutex_lock(&my_mutex, K_FOREVER);
  int i;
  for (i=0; i < MAX_DATA_BUF_LENGTH; ++i){
    data_buffer[i].used = 0;
  }
  k_mutex_unlock(&my_mutex);
}

data_buf_item_t* store_to_data_buffer(uint8_t* src_data, size_t len){
  //lock mutex
  k_mutex_lock(&my_mutex, K_FOREVER);
  uint8_t i = 0;
  uint8_t is_free = 0;
  while(i < MAX_DATA_BUF_LENGTH && is_free == 0){
    if (data_buffer[i].used == 0){
      is_free = 1;
    } 
    else {
      ++i;
    }
  }
  if (i == MAX_DATA_BUF_LENGTH){
    //Data buffer is FULL
    k_mutex_unlock(&my_mutex);
    return (data_buf_item_t*) NULL;
  }
  else {
    data_buffer[i].used = 1;
    memcpy(data_buffer[i].data, src_data, len);
    data_buffer[i].len = len;
    k_mutex_unlock(&my_mutex);
    return &data_buffer[i];
  }
}

void free_data_buffer_item(data_buf_item_t* item){
   //lock mutex
   k_mutex_lock(&my_mutex, K_FOREVER);
   item->used = 0;
   memset(item->data, 0, item->len);
   k_mutex_unlock(&my_mutex);
   //unlock mutex
}
