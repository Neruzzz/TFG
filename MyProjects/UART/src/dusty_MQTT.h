/*
 * Wireless Networks Research Lab @ UOC
 * Author: Aaron Acosta
 * Date: 11/12/2020
 * License: BSD
 */

#ifndef dusty_MQTT
#define dusty_MQTT

#include <drivers/uart.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <date_time.h>
#include <random/rand32.h>

#include "dn_ipmg.h"
#include "dn_serial_mg.h"
#include "dn_uart.h"
#include "data_buff.h"
#include <modem/lte_lc.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <logging/log.h>
#include <cJSON.h>

//=========================== defines =========================================

#define STACKSIZE 2048
#define SUCCESS 0
#define ENDSTR 0
#define THREAD_NAME_LEN 32

//Message Queue defines
#define MAX_MSGQ_LEN MAX_DATA_BUF_LENGTH
#define MSGQ_WAITING_PERIOD 10 //ms

//threads priorities
#define PRIORITY_MQTT -3 //the lower the number is, the higher priority is
#define PRIORITY_DUSTY -3

//threads delays (in ms)
#define MQTT_THREAD_DELAY  5000
#define DUSTY_THREAD_DELAY 0

//DUSTY THREAD
#define CMD_PERIOD 1000                  
#define BACKOFF_AFTER_TIMEOUT 0   
#define SERIAL_RESPONSE_TIMEOUT 40 
#define INTER_FRAME_PERIOD 0

//subscription
#define SUBSC_FILTER_DATA 0x10
#define UNSUBSC_ALL 0x00

//MQTT THREAD
#define MQTT_PUBLISH_FLAG 0
#define MQTT_SUBSCRIPTION_COUNT 1
#define MQTT_SUBSCRIPTION_MSG_ID 1
#define MQTT_DATA_PUBLISHING_MAX_RETRIES 3
#define MQTT_POLL_TIMEOUT 5000 //ms
#define MQTT_SENDING_PERIOD 100 //ms
#define MQTT_USERNAME "wine"
#define MQTT_PASS "wine2020"

//PAYLOAD DATA DEFINE's
#define MAC_ADDR_SIZE 24
#define TIMESTAMP_STR_SIZE 16
#define GATEWAY_NAME "uoc-1"


LOG_MODULE_REGISTER(app);

static K_MUTEX_DEFINE(my_mutex_2);
//=========================== typedef =========================================

//=== callback signature
typedef void (*fsm_timer_callback)(void);
typedef void (*fsm_reply_callback)(void);

// msg queue item
typedef struct { 
  data_buf_item_t* data;
} msgq_item_t; //size must be power of 2

typedef struct {
  // fsm
  fsm_timer_callback fsmCb;
  // reply
  fsm_reply_callback replyCb;
  // api
  uint8_t replyBuf[MAX_FRAME_LENGTH]; // holds notifications from ipmg
  uint8_t notifBuf[MAX_FRAME_LENGTH]; // notifications buffer internal to ipmg
} app_vars_t;


//data struct to let an MQTT client know the packet sending and receiving statistics 
struct mqtt_stats{
  int n_pkt_tx; //num of packets transmitted from the MQTT Client to the dusty
  int n_pkt_rx; //num of packets received from the dusty to MQTT Client
};


//====================== complex data structures ==============================

//msg_queue_name, msg_q_item_size, num_max_items, align
K_MSGQ_DEFINE(Dusty_to_MQTT_msg_q, sizeof(msgq_item_t), MAX_MSGQ_LEN, sizeof(msgq_item_t));
K_MSGQ_DEFINE(MQTT_to_Dusty_msg_q, sizeof(msgq_item_t), MAX_MSGQ_LEN, sizeof(msgq_item_t));

//=========================== variables =======================================


// Buffers for MQTT client.
static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];
// The mqtt client struct
static struct mqtt_client client;
// MQTT Broker details.
static struct sockaddr_storage broker;
// Connected flag 
static bool connected;
// File descriptor 
static struct pollfd fds;
static struct mqtt_stats stats;
// Broker user name and password 
static struct mqtt_utf8 password;
static struct mqtt_utf8 user_name;

//Dusty thread app vars
app_vars_t app_vars;


//=========================== prototypes ======================================


//Dusty thread prototypes
int   Dusty_thread            (void);
void  dn_ipmg_notif_cb        (uint8_t cmdId, 
                               uint8_t subCmdId);
void  dn_ipmg_reply_cb        (uint8_t cmdId);
void  dn_ipmg_status_cb       (uint8_t newStatus);
void  fsm_scheduleEvent       (uint16_t delay, 
                               fsm_timer_callback cb);
void  fsm_cancelEvent         (void);
void  fsm_setCallback         (fsm_reply_callback cb);
void  api_response_timeout    (void);
void  api_initiateConnect     (void);
void  api_subscribe           (void);
void  api_subscribe_reply     (void);
void  timer_interrupt         (struct k_timer *dummy);

//MQTT thread prototypes
void  MQTT_thread             (void);
void  broker_init             (void);
void  client_init             (struct mqtt_client *client);
int   fds_init                (struct mqtt_client *c);
int   data_publish            (struct mqtt_client *c, 
                               enum mqtt_qos qos,
                               uint8_t *data,
                               size_t len);
int   subscribe               (void);
int   publish_get_payload     (struct mqtt_client *c,
                               size_t length);
void  mqtt_evt_handler        (struct mqtt_client *const c,
                               const struct mqtt_evt *evt);
void  modem_configure         (void);
void  app_mqtt_connect        (void);
inline void  set_client_username_pass(void);

//Helper fuctions
void  comm_DS_error_handling  (k_tid_t thread_id,
                               int err_code);
void convert_dusty_pkt_to_string(char* name, dn_ipmg_notifData_nt* notifData, char* pkt_str, size_t pkt_str_len);
//timer for fsm
K_TIMER_DEFINE(fsm_timer, timer_interrupt, NULL);

#endif
