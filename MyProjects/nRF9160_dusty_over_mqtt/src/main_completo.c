/*
 * Wireless Networks Research Lab @ UOC
 * Author: Aaron Acosta
 * Date: 11/12/2020
 * License: BSD
 */


#include "dusty_MQTT.h"
#include "certificates.h"

#if defined(CONFIG_MQTT_LIB_TLS)
static sec_tag_t sec_tag_list[] = { CONFIG_MQTT_TLS_SEC_TAG };
#endif /* defined(CONFIG_MQTT_LIB_TLS) */ 

#if defined(CONFIG_BSD_LIBRARY)

// Recoverable BSD library error.
void bsd_recoverable_error_handler(uint32_t err) {
  printk("bsdlib recoverable error: %u\n", (unsigned int)err);
}

#endif // defined(CONFIG_BSD_LIBRARY)

#if defined(CONFIG_MQTT_LIB_TLS)
static int certificates_provision(void)
{
  int err = 0;
  LOG_INF("Provisioning certificates");
#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
  err = modem_key_mgmt_write(CONFIG_MQTT_TLS_SEC_TAG,
                             MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
                             CA_CERTIFICATE,
                             strlen(CA_CERTIFICATE));
  if (err) {
          LOG_ERR("Failed to provision CA certificate: %d", err);
          return err;
  }
#elif defined(CONFIG_BOARD_QEMU_X86) && defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
  err = tls_credential_add(CONFIG_MQTT_TLS_SEC_TAG,
                           TLS_CREDENTIAL_CA_CERTIFICATE,
                           CA_CERTIFICATE,
                           sizeof(CA_CERTIFICATE));
  if (err) {
          LOG_ERR("Failed to register CA certificate: %d", err);
          return err;
  }
#endif
  return err;
}
#endif /* defined(CONFIG_MQTT_LIB_TLS) */

//*****************************************************************************************************************************************************************************************************
//**************************************************************** DUSTY_THREAD **********************************************************************************************************************
//*****************************************************************************************************************************************************************************************************

/*
 *The Dusty Thread is responsible for the management of the connection between the Dusty manager module and the application.
 *If you take a look at the code, you will see that the connection is lead by a finite state machine.
 *
 *Once connection has been established, if a data notification is received from the Dusty manager,
 *the thread immediately starts executing the notification callback function dn_ipmg_notif_cb(cmdID, subCmdId, notifData)
 *which pushes the received data to the upstream FIFO. Otherwise, the thread loops indefinitely reading the downstream FIFO.
*/

int Dusty_thread(void) 
{
   msgq_item_t rx_data;
   int8_t ret; 
   init_data_buffer();
   // reset local variables
   memset(&app_vars, 0, sizeof(app_vars));
   // initialize the ipmg module
   dn_ipmg_init(
      dn_ipmg_notif_cb,                // notifCb
      app_vars.notifBuf,               // notifBuf
      sizeof(app_vars.notifBuf),       // notifBufLen
      dn_ipmg_reply_cb,                // replyCb
      dn_ipmg_status_cb                // statusCb
   );
   fsm_scheduleEvent(CMD_PERIOD, &api_initiateConnect);
   while(1){   
      ret = k_msgq_get(&MQTT_to_Dusty_msg_q, &rx_data, K_MSEC(MSGQ_WAITING_PERIOD));
      if (ret == SUCCESS){ //success #define
        LOG_INF("[DUSTY_Thread] data read from FIFO successfully!");
#ifdef CONFIG_LOG
        char rx_data_arr[rx_data.data->len+1];
        rx_data_arr[rx_data.data->len] = ENDSTR;
        memcpy(rx_data_arr, rx_data.data->data, rx_data.data->len);
#endif
        LOG_INF("[DUSTY_Thread] Received from MQTT Thread: %s", log_strdup(rx_data_arr));
        free_data_buffer_item(rx_data.data);
     }
  }
  return SUCCESS;
}

void fsm_scheduleEvent(uint16_t delay, fsm_timer_callback cb)
{   
   // remember what function to call
   app_vars.fsmCb       = cb;
   // configure/start the timer
   k_timer_start(&fsm_timer, K_MSEC(delay), K_NO_WAIT);
}

void fsm_cancelEvent(void) 
{
   // stop the timer
   k_timer_stop(&fsm_timer);
   // clear function to call
   app_vars.fsmCb       = NULL;
}

void fsm_setCallback(fsm_reply_callback cb) 
{
   app_vars.replyCb     = cb;
}

void api_response_timeout(void) 
{
   LOG_ERR("[DUSTY_Thread] API response timeout! Reconnecting...");
   dn_ipmg_cancelTx();
   fsm_scheduleEvent(BACKOFF_AFTER_TIMEOUT,api_initiateConnect);  
}

void api_initiateConnect(void) 
{
   LOG_INF("[DUSTY_Thread] Initiating connection...");
   // issue command from Dusty API
   dn_ipmg_initiateConnect();
   // schedule timeout event
   fsm_scheduleEvent(
      SERIAL_RESPONSE_TIMEOUT,
      api_response_timeout
   );
}

void api_subscribe(void) 
{
   LOG_INF("[DUSTY_Thread] Subscribing to data notifications...");
   fsm_setCallback(api_subscribe_reply);
   dn_ipmg_subscribe(
      0xFFFFFFFF,                                     // filter
      0x00000000,                                     // unackFilter
      (dn_ipmg_subscribe_rpt*)(app_vars.replyBuf)     // reply
   );
   // schedule timeout event
   fsm_scheduleEvent(
      SERIAL_RESPONSE_TIMEOUT,
      api_response_timeout
   );
}

void api_subscribe_reply(void) 
{
   //cancel the api_response_timeout event once successfully subscribed
   fsm_cancelEvent();
   LOG_INF("[DUSTY_Thread] SUBSCRIBED to data notifications succesfully!\n");
}

void dn_ipmg_notif_cb(uint8_t cmdId, uint8_t subCmdId) 
{
  // k_mutex_lock(&my_mutex_2, K_FOREVER);
   dn_ipmg_notifData_nt* notifData_notif;
   int8_t err;
   msgq_item_t msgq_item;
   if (cmdId==DN_NOTIFID_NOTIFDATA) {

      //Build JSON (using cJSON library)
      //Convert to String (using cJSON library)
      //Store string in data_buffer (calling to store_to_data_buffer)
      //Get the address returned by the previous step and assign it to a msgq_item_t variable (into the data field)
      //Put this address into Dusty_to_MQTT_msg_q
      notifData_notif = (dn_ipmg_notifData_nt*)app_vars.notifBuf;

      char pkt_str[700];
      convert_dusty_pkt_to_string("notifData", notifData_notif, &pkt_str, sizeof(pkt_str));
     
     /*
      time_t rawtime;
      struct tm* ptm;
      time ( &rawtime );
      ptm = gmtime ( &rawtime );

      time_t now = time(NULL);
      printk("UTC time: %u:%u:%u", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);*/

      /*TIME*/
      int64_t unix_time_ms;

      /*Force the date_time library to get current time */
      int64_t uptime = k_uptime_get();
      date_time_uptime_to_unix_time_ms(&uptime);

      /*Read current time and put in container */
      int err = date_time_now(&unix_time_ms);

      /*Print the current time */
      printk("Date_time: %i %i\n", (uint32_t)(unix_time_ms/1000000), (uint32_t)(unix_time_ms%1000000));
      
      LOG_INF("[DUSTY_Thread] data notification received from the manager!");
      //If data buffer is full, the application prints a message on the serial output and 
      //passes the execution to the other thread in order to it consumes data buffer items and frees storage.
      msgq_item.data = store_to_data_buffer(&pkt_str, strlen(pkt_str)); 
      //data buffer is FULL
      if (msgq_item.data == NULL){
        int8_t err_code = -ENOBUFS;
        comm_DS_error_handling(k_current_get(), err_code);
        k_yield();
      } 
      else{
        //We are assuming that k_msgq_put is creating a copy of the struct. Must be checked.
        err = k_msgq_put(&Dusty_to_MQTT_msg_q, &msgq_item, K_NO_WAIT);
        if (err < SUCCESS){
          comm_DS_error_handling(k_current_get(), err);
          k_yield();
        }
        else {
          LOG_INF("[DUSTY_Thread] data succesfully written in the message queue!\n");
        }
      }
   }
   if(cmdId == DN_EVENTID_EVENTCOMMANDFINISHED){
      printk("Net id successfully set to 1229!\n");
   }
   if(cmdId == DN_NOTIFID_NOTIFEVENT){
      LOG_INF("[DUSTY_Thread] event notification received from the manager!\n\n");
   }
   k_mutex_unlock(&my_mutex_2);
}

void dn_ipmg_reply_cb(uint8_t cmdId) 
{
   app_vars.replyCb();
}

void dn_ipmg_status_cb(uint8_t newStatus) 
{
   switch (newStatus) {
      case DN_SERIAL_ST_CONNECTED:
         //cancel the api_response_timeout event once connected
         fsm_cancelEvent();
         LOG_INF("[DUSTY_Thread] CONNECTED succesfully!");
         // schedule next event
         fsm_scheduleEvent(
            INTER_FRAME_PERIOD,
            api_subscribe
         );
         break;
      case DN_SERIAL_ST_DISCONNECTED:
         LOG_ERR("[DUSTY_Thread] DISCONNECTED! Reconnecting...");
         // schedule first event to reconnect with the dusty manager
         fsm_scheduleEvent(
            INTER_FRAME_PERIOD,
            api_initiateConnect
         );
         break;
      default:
         // nothing to do
         break;
   }
}

void timer_interrupt(struct k_timer *dummy)
{
   //Call the callback function set in fsm_scheduleEvent()
   app_vars.fsmCb();
}


//*****************************************************************************************************************************************************************************************************
//**************************************************************** MQTT_THREAD **********************************************************************************************************************
//*****************************************************************************************************************************************************************************************************


/* The MQTT Thread is responsible for the management of the connection between the application and the MQTT broker.
 * It consists of an infinite loop which polls for an event in a file descriptor holding the connection parameters during a certain period. 
 * If the file descriptor is ready, it means the connection is up and we are able to publish data to a topic and receive data from a topic.
 * Whenever there are data to be received from a topic, the mqtt_event_handler(client, evt) function is called with the argument evt 
 * holding the type of event has occurred. 
 * If this event matches with an MQTT_PUBLISH_EVT, means that we have new data to be picked up from an MQTT_client.
 * Then, these data are pushed into the downstream FIFO in order to be managed by the Dusty Thread.
 * While there are no data to be received from a topic, the thread tries to read the upstream FIFO indefinitely 
 * to publish pending information received from the Dusty manager
 */
int data_publish(struct mqtt_client *c, enum mqtt_qos qos, uint8_t *data, size_t len) 
{
  struct mqtt_publish_param param;
  param.message.topic.qos = qos; //qos = 1
  param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
  param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
  param.message.payload.data = data;
  param.message.payload.len = len;
  param.message_id = sys_rand32_get();
  param.dup_flag = MQTT_PUBLISH_FLAG;
  param.retain_flag = MQTT_PUBLISH_FLAG; //retain = 0
  LOG_INF("[MQTT_Thread] to topic: %s len: %u\n", CONFIG_MQTT_PUB_TOPIC, (unsigned int)strlen(CONFIG_MQTT_PUB_TOPIC));
  return mqtt_publish(c, &param);
}

// Function to subscribe to the configured topic
int subscribe(void) 
{
  struct mqtt_topic subscribe_topic = {
      .topic = { .utf8 = CONFIG_MQTT_SUB_TOPIC,
                 .size = strlen(CONFIG_MQTT_SUB_TOPIC)},
                 .qos = MQTT_QOS_1_AT_LEAST_ONCE};
  const struct mqtt_subscription_list subscription_list = { .list = &subscribe_topic,
                                                            .list_count = MQTT_SUBSCRIPTION_COUNT,
                                                            .message_id = MQTT_SUBSCRIPTION_MSG_ID};
  LOG_INF("[MQTT_Thread] Subscribing to: %s len %u\n", CONFIG_MQTT_SUB_TOPIC, (unsigned int)strlen(CONFIG_MQTT_SUB_TOPIC));
  return mqtt_subscribe(&client, &subscription_list);
}

// Function to read the published payload.
int publish_get_payload(struct mqtt_client *c, size_t length) 
{
  int8_t err;
  uint8_t *buf = payload_buf;
  uint8_t *end = buf + length;
  if (length > sizeof(payload_buf)) {
    return -EMSGSIZE;
  }
  while (buf < end) {
    int ret = mqtt_read_publish_payload(c, buf, end - buf);
    if (ret < SUCCESS) {
      if (ret != -EAGAIN) {
        return ret;
      }
      LOG_ERR("[MQTT_Thread] mqtt_read_publish_payload: EAGAIN");
      err = poll(&fds, 1, CONFIG_MQTT_KEEPALIVE * MSEC_PER_SEC);
      if (err > SUCCESS && (fds.revents & POLLIN) == POLLIN) {
        continue;
      } else {
        return -EIO;
      }
    }
    if (ret == SUCCESS) {
      return -EIO;
    }
    buf += ret;
  }
  return SUCCESS;
}

// MQTT client event handler
void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt) 
{
  int8_t err;
  switch (evt->type) {
  //nRF9160 is connected
  case MQTT_EVT_CONNACK:
    if (evt->result != SUCCESS) {
      LOG_ERR("[MQTT_Thread] MQTT connect failed %d", evt->result);
      break;
    }
    connected = true;
    LOG_INF("[MQTT_Thread] MQTT client connected!");
    subscribe();
    break;

  //nRF9160 is disconnected
  case MQTT_EVT_DISCONNECT:
    LOG_ERR("[MQTT_Thread] MQTT client disconnected %d", evt->result);
    connected = false;
    break;

  //nRF9160 receives a message from one of the subscribed topics
  case MQTT_EVT_PUBLISH:{
    const struct mqtt_publish_param *p = &evt->param.publish;
    LOG_INF("[MQTT_Thread] MQTT PUBLISH result=%d len=%d", evt->result, p->message.payload.len);
    err = publish_get_payload(c, p->message.payload.len);
    if (err >= SUCCESS) {
      //push data to message queue 
      data_buf_item_t* ptr_item = store_to_data_buffer(payload_buf, (uint8_t)p->message.payload.len);
      if (ptr_item == NULL){
        int8_t err_code = -ENOBUFS;
        comm_DS_error_handling(k_current_get(), err_code);
        k_yield();
      } 
      else {
        msgq_item_t msgq_item = {ptr_item};
        int8_t ret = k_msgq_put(&MQTT_to_Dusty_msg_q, &msgq_item, K_NO_WAIT);
        if (ret == SUCCESS){
          LOG_INF("[DUSTY_Thread] data succesfully written in the message queue!");
        }
        else {
          LOG_ERR("[MQTT_Thread] ERROR: unable to write data to the message queue!");
        }
      } 
    } 
    else {
      LOG_ERR("[MQTT_Thread] mqtt_read_publish_payload: Failed! %d", err);
      LOG_ERR("[MQTT_Thread] Disconnecting MQTT client...");
      err = mqtt_disconnect(c);
      if (err) {
        LOG_ERR("[MQTT_Thread] Could not disconnect: %d", err);
      }
    }
    break;
   }

  //nRF9160 receives an ACK from MQTT Broker meaning the published message has been published succesfully.
  case MQTT_EVT_PUBACK:
    if (evt->result != SUCCESS) {
      LOG_ERR("[MQTT_Thread] MQTT PUBACK error %d", evt->result);
      break;
    }
    LOG_INF("[MQTT_Thread] PUBACK packet id: %u", evt->param.puback.message_id);
    break;

  //nRF9160 receives an ACK from MQTT Broker meaning subscription has been done succesfully.
  case MQTT_EVT_SUBACK:
    if (evt->result != SUCCESS) {
      LOG_ERR("[MQTT_Thread] MQTT SUBACK error %d", evt->result);
      break;
    }
    LOG_INF("[MQTT_Thread] SUBACK packet id: %u\n", evt->param.suback.message_id);
    break;

  default:
    LOG_INF("[MQTT_Thread] default: %d",evt->type);
    break;
  }
}

// Resolves the configured hostname and initializes the MQTT broker structure
void broker_init(void)
{
  int err;
  struct addrinfo *result;
  struct addrinfo *addr;
  struct addrinfo hints = {
          .ai_family = AF_INET,
          .ai_socktype = SOCK_STREAM
  };
  err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
  if (err) {
    LOG_ERR("getaddrinfo failed: %d", err);
    return -ECHILD;
  }
  addr = result;
  /* Look for address of the broker. */
  while (addr != NULL) {
    /* IPv4 Address. */
    if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
        struct sockaddr_in *broker4 =
                ((struct sockaddr_in *)&broker);
        char ipv4_addr[NET_IPV4_ADDR_LEN];

        broker4->sin_addr.s_addr =
                ((struct sockaddr_in *)addr->ai_addr)
                ->sin_addr.s_addr;
        broker4->sin_family = AF_INET;
        broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

        inet_ntop(AF_INET, &broker4->sin_addr.s_addr,
                  ipv4_addr, sizeof(ipv4_addr));
        LOG_INF("IPv4 Address found %s", log_strdup(ipv4_addr));

        break;
    } else {
        LOG_ERR("ai_addrlen = %u should be %u or %u",
                (unsigned int)addr->ai_addrlen,
                (unsigned int)sizeof(struct sockaddr_in),
                (unsigned int)sizeof(struct sockaddr_in6));
    }
    addr = addr->ai_next;
  }
  /* Free the address. */
  freeaddrinfo(result);
 
}

inline void set_client_username_pass(void)
{
  password.utf8 = (uint8_t*)CONFIG_MQTT_BROKER_PASSWORD;
  password.size = strlen(CONFIG_MQTT_BROKER_PASSWORD);
  user_name.utf8 = (uint8_t*)CONFIG_MQTT_BROKER_USERNAME;
  user_name.size = strlen(CONFIG_MQTT_BROKER_USERNAME);
}

// Initialize the MQTT client structure
void client_init(struct mqtt_client *client) 
{
  mqtt_client_init(client);
  broker_init();
  // MQTT client configuration 
  client->broker = &broker;
  client->evt_cb = mqtt_evt_handler;
  client->client_id.utf8 = (uint8_t *)CONFIG_MQTT_CLIENT_ID;
  client->client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);
  client->protocol_version = MQTT_VERSION_3_1_1;
  // MQTT buffers configuration
  client->rx_buf = rx_buffer;
  client->rx_buf_size = sizeof(rx_buffer);
  client->tx_buf = tx_buffer;
  client->tx_buf_size = sizeof(tx_buffer);
#if CONFIG_MQTT_BROKER_SET_USERNAME_AND_PASSWORD
  set_client_username_pass();
  client->user_name = &user_name;
  client->password = &password;
#else
  client->user_name = NULL;
  client->password = NULL;
#endif
  // MQTT transport configuration
#if defined(CONFIG_MQTT_LIB_TLS)

  struct mqtt_sec_config *tls_cfg = &(client->transport).tls.config;
  static sec_tag_t sec_tag_list[] = { CONFIG_MQTT_TLS_SEC_TAG };

  LOG_INF("TLS enabled");
  client->transport.type = MQTT_TRANSPORT_SECURE;

  tls_cfg->peer_verify = CONFIG_MQTT_TLS_PEER_VERIFY;
  tls_cfg->cipher_count = 0;
  tls_cfg->cipher_list = NULL;
  tls_cfg->sec_tag_count = ARRAY_SIZE(sec_tag_list);
  tls_cfg->sec_tag_list = sec_tag_list;
  tls_cfg->hostname = CONFIG_MQTT_BROKER_HOSTNAME;

#else
  client->transport.type = MQTT_TRANSPORT_NON_SECURE;
#endif
}

//Initialize the file descriptor structure used by poll.
int fds_init(struct mqtt_client *c) 
{
  if (c->transport.type == MQTT_TRANSPORT_NON_SECURE) {
    fds.fd = c->transport.tcp.sock;
  } else {
#if defined(CONFIG_MQTT_LIB_TLS)
    fds.fd = c->transport.tls.sock;
#else
    return -ENOTSUP;
#endif
  }
  fds.events = POLLIN;
  return SUCCESS;
}

//Configures modem to provide LTE link. Blocks until link is successfully established.
void modem_configure(void)
{

  /* Turn off LTE power saving features for a more responsive demo. Also,
   * request power saving features before network registration. Some
   * networks rejects timer updates after the device has registered to the
   * LTE network.
   */
  LOG_INF("Disabling PSM and eDRX");
  lte_lc_psm_req(false);
  lte_lc_edrx_req(false);

#if defined(CONFIG_LTE_LINK_CONTROL)
  if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
          /* Do nothing, modem is already turned on
           * and connected.
           */
  } else {
#if defined(CONFIG_LWM2M_CARRIER)
    /* Wait for the LWM2M_CARRIER to configure the modem and
     * start the connection.
     */
    LOG_INF("Waitng for carrier registration...");
    k_sem_take(&carrier_registered, K_FOREVER);
    LOG_INF("Registered!");
#else /* defined(CONFIG_LWM2M_CARRIER) */
    int err;

    LOG_INF("LTE Link Connecting...");
    err = lte_lc_init_and_connect();
    if (err) {
            LOG_INF("Failed to establish LTE connection: %d", err);
            return err;
    }
    LOG_INF("LTE Link Connected!");
#endif /* defined(CONFIG_LWM2M_CARRIER) */
  }
#endif /* defined(CONFIG_LTE_LINK_CONTROL) */

}

void app_mqtt_connect(void)
{
  int8_t err;
  int8_t n_attempts = 0;
  while (!connected){
    LOG_INF("[MQTT_Thread] Attempt #%d to connect MQTT",n_attempts);
    err = mqtt_connect(&client);
    if (err != SUCCESS) {
      LOG_ERR("[MQTT_Thread] ERROR: mqtt_connect %d", err);
    }
    err = fds_init(&client);
    if (err != SUCCESS) {
      LOG_ERR("[MQTT_Thread] ERROR: fds_init %d", err);
    }
    poll(&fds, 1, MQTT_POLL_TIMEOUT);
    mqtt_input(&client);
    if (!connected) {
       mqtt_abort(&client);
    }
    ++n_attempts;
  }
}

void MQTT_thread(void)
{
  int cert_err;
  LOG_INF("[MQTT_Thread] The MQTT simple sample started");

#if defined(CONFIG_MQTT_LIB_TLS)
  cert_err = certificates_provision();
  if (cert_err != 0) {
    LOG_ERR("Failed to provision certificates");
    return;
  }
#endif /* defined(CONFIG_MQTT_LIB_TLS) */

  //Configures modem to provide LTE link. Blocks until link is successfully established.
  //IF SIGNAL COVERAGE IS LOST, DATA BUFFER WILL ONLY HOLD 16 PACKETS.
  //THE REST OF PENDING DATA TO BE ALLOCATED WILL BE LOST.
  modem_configure();
  //Initialize the MQTT client structure
  client_init(&client);
  app_mqtt_connect();
  int8_t err;
  int8_t retries;
  while (1) {
    err = poll(&fds, 1, MQTT_SENDING_PERIOD);
    if (err < 0) {
      LOG_ERR("[MQTT_Thread] ERROR: poll %d\n", errno);
    }
    err = mqtt_live(&client);
    if ((err != 0) && (err != -EAGAIN)) {
      LOG_ERR("[MQTT_Thread] ERROR: mqtt_live %d\n", err);
    }
    err = mqtt_input(&client);
    if (err < 0) {
      LOG_ERR("[MQTT_Thread] ERROR: mqtt_input %d\n", err);
      app_mqtt_connect();
    }
    else{
      msgq_item_t rx_data;  
      err = k_msgq_get(&Dusty_to_MQTT_msg_q, &rx_data, K_MSEC(MSGQ_WAITING_PERIOD));
      if (err < SUCCESS){
         //error_handling(k_current_get(), ret);
      }
      else{
        LOG_INF("[MQTT_Thread] data read from FIFO successfully!");
#ifdef CONFIG_LOG
        char rx_data_arr[rx_data.data->len+1];
        rx_data_arr[ rx_data.data->len] = ENDSTR;
        memcpy(rx_data_arr, rx_data.data->data, rx_data.data->len);
#endif
        printk("[MQTT_Thread] Receive from Dusty Thread: %s", rx_data_arr);
        //start sending the data buffer through MQTT
        retries = 0;
        while ((err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE, rx_data.data->data, rx_data.data->len)) < SUCCESS && retries < MQTT_DATA_PUBLISHING_MAX_RETRIES){
          ++retries;
        }
        if (retries == MQTT_DATA_PUBLISHING_MAX_RETRIES){
          // Unable to publish the data
          LOG_ERR("[MQTT_Thread] ERROR publishing data!");
          //Disconnects the mqtt client and try to reconnect the next iteration
          mqtt_disconnect(&client);
        }
        free_data_buffer_item(rx_data.data);
      }
    }
  }       
}

//***********************************************************************************************//
//************************************* HELPERS *************************************************//
//***********************************************************************************************//

//Function to print the errors occurred when dealing with inter-thread communication data structures 
void comm_DS_error_handling(k_tid_t thread_id, int err_code)
{
  char thread_name[THREAD_NAME_LEN];
  k_thread_name_copy(thread_id, thread_name, sizeof(thread_name));
  switch(err_code){
    case -ENOBUFS:
      LOG_ERR("[%s] data buffer is full!", log_strdup(thread_name));
      break;
    case -ENOMSG:
      LOG_ERR("[%s] FIFO is empty or has been purged!",log_strdup(thread_name));
      break;
    case -EAGAIN:
      LOG_ERR("[%s] FIFO Waiting period timed out!",log_strdup(thread_name));
      break;
    default:
      LOG_ERR("[%s] error_code: %d", log_strdup(thread_name),err_code);
  }
}

//Function to convert a notification data from Dusty to string.
void convert_dusty_pkt_to_string(char* name, dn_ipmg_notifData_nt* notifData, char* pkt_str, size_t pkt_str_len) {
  
   cJSON *root, *fields, *data_array, *timestamp;

   root = cJSON_CreateObject();
   fields = cJSON_CreateObject();
   data_array = cJSON_CreateArray();

   char macAddr_str[MAC_ADDR_SIZE];
   uint8_t* macAddr_ptr = notifData->macAddress;
   sprintf(macAddr_str, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", macAddr_ptr[0], macAddr_ptr[1], 
                                                                   macAddr_ptr[2], macAddr_ptr[3],
                                                                   macAddr_ptr[4], macAddr_ptr[5], 
                                                                   macAddr_ptr[6], macAddr_ptr[7]);

   uint32_t timestamp_doub = 0;
   int8_t i;
   for (i = 7; i >= 0; --i){
     const uint32_t byte = (uint32_t)notifData->utcSecs[i];
     if (byte != 0){
        timestamp_doub |= (byte << (8*(7-i)));
     }
   }
   char timestamp_str[TIMESTAMP_STR_SIZE];
   itoa(timestamp_doub, timestamp_str, 10);

   cJSON_AddItemToObject(root, "name", cJSON_CreateString(name));
   cJSON_AddItemToObject(root, "fields", fields);
   cJSON_AddItemToObject(fields, "macAddress", cJSON_CreateString(macAddr_str));
   cJSON_AddItemToObject(root, "gateway",  cJSON_CreateString(GATEWAY_NAME));
   cJSON_AddItemToObject(root, "timestamp", cJSON_CreateString(timestamp_str));
   
   char* uncompleted_json = NULL;
   uncompleted_json = cJSON_Print(root);

   memset(pkt_str, 0, pkt_str_len);

   if (uncompleted_json == NULL){
      printk("uncompleted_json = NULL!\n");
   }
   else {
      printk("%c\n",uncompleted_json[77]);
   }

   cJSON_Delete(root);

   //Convert the payload to string
   int data_len = notifData->dataLen;
   char data_str[7 + data_len * 4]; 
   uint8_t idx;
   int num_wr_chars = 0;
   for (idx = 0; idx < data_len; ++idx){
      if (idx == 0){
        num_wr_chars += sprintf(&data_str[num_wr_chars], "[%u", notifData->data[idx]);
      }
      else if (idx > 0 && idx < data_len-1) {
        num_wr_chars += sprintf(&data_str[num_wr_chars], ",%u", notifData->data[idx]);
      }
      else {
        num_wr_chars += sprintf(&data_str[num_wr_chars], ",%u]", notifData->data[idx]);
      }
   }
   strncpy(pkt_str, uncompleted_json, 78);
   strcat(pkt_str, ",\n\"data\":");
   strcat(pkt_str, data_str);
   strcat(pkt_str, &uncompleted_json[78]);  
}
K_THREAD_DEFINE(DUSTY_Thread, STACKSIZE, Dusty_thread, NULL, NULL, NULL,
    PRIORITY_DUSTY, 0, DUSTY_THREAD_DELAY);

K_THREAD_DEFINE(MQTT_Thread, STACKSIZE, MQTT_thread, NULL, NULL, NULL,
    PRIORITY_MQTT, 0, MQTT_THREAD_DELAY); 

