#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/uart.h>
#include "dusty_MQTT.h"


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


void dn_ipmg_reply_cb(uint8_t cmdId) 
{
   app_vars.replyCb();
}

void api_getSystemInfo_reply(void){
  fsm_cancelEvent();
  dn_ipmg_getSystemInfo_rpt* reply;
  reply = (dn_ipmg_getSystemInfo_rpt*)app_vars.replyBuf;
  if(reply->RC == 0){
     char MAC[24];
     sprintf(MAC,"%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", reply->macAddress[0], reply->macAddress[1], reply->macAddress[2], 
                  reply->macAddress[3], reply->macAddress[4], reply->macAddress[5], reply->macAddress[6], reply->macAddress[7]);
     LOG_INF("Hardware model: %i , Hardware Revision: %i , MAC Address: %s \n",reply->hwModel,reply->hwRev,log_strdup(MAC));
  }
}

void api_getSystemInfo(void){
  fsm_setCallback(api_getSystemInfo_reply);
  dn_ipmg_getSystemInfo((dn_ipmg_getSystemInfo_rpt*)(app_vars.replyBuf));
  fsm_scheduleEvent(SERIAL_RESPONSE_TIMEOUT, api_response_timeout);
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
            api_getSystemInfo
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


void main()
{
   memset(&app_vars, 0, sizeof(app_vars));
   // initialize the ipmg module
   dn_ipmg_init(
      NULL,                            // notifCb
      NULL,                            // notifBuf
      0,                               // notifBufLen
      dn_ipmg_reply_cb,                // replyCb
      dn_ipmg_status_cb                // statusCb
   );
   fsm_scheduleEvent(CMD_PERIOD, &api_initiateConnect);
}

