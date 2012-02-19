#include <string.h>
#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "nwk_frame.h"
#include "nwk.h"
//#include "virtual_com_cmds.h"
#include "device_type.h"
//device drivers
#include "i2c_driver.h"
#include "temp_LM75.h"
#include "rtc.h"
#include "profile_manager.h"
#include "../common/event_queue.h"
#include "../common/msg_common.h"
#include "../interface/xport.h"
#include "../interface/xport_msg.h"

// Active connection information
typedef struct{
   linkID_t link_id;
   uint8_t device_type;
   uint8_t device_addr[4];
}CONNECTION_T;
static CONNECTION_T connections[NUM_CONNECTIONS] = {0};
static uint16_t num_connections = 0;
//end device msg processing function
static int16_t device_itrv_cb(uint8_t* msg, uint8_t len, uint8_t* resp_msg);
static int16_t device_temp_cb(uint8_t* msg, uint8_t len, uint8_t* resp_msg);
typedef int16_t (*CONNECTION_CALLBACK_T)(uint8_t* msg, uint8_t len, uint8_t* resp_msg);
static CONNECTION_CALLBACK_T connection_callback[DEVICE_MAX] = {device_itrv_cb, device_temp_cb};
static CONNECTION_T* get_connection(linkID_t link_id);

//used for event processing
static uint8_t frame_callback(linkID_t id);

//event queue
#define MAX_EVENTS 8
EVENT_QUEUE_T event_queue;
GATEWAY_EVENT_MSG_T events[MAX_EVENTS];


//msg msp430 -> XPORT
static uint8_t msg_out[MAX_APP_PAYLOAD+4];
//msg XPORT -> msp430
static uint8_t msg_in[XPORT_MAX_MSG];

//debug stats
static uint16_t ack_fails = 0;

//Status callbacks received from end device
static int16_t device_itrv_cb(uint8_t* msg, uint8_t len, uint8_t* resp_msg)
{
   int16_t status;
   //STATUS, request profile
   status = xport_send_msg(msg, len, resp_msg);
   return(status);
}

static int16_t device_temp_cb(uint8_t* msg, uint8_t len, uint8_t* resp_msg)
{
   //TODO queue msg_out to be sent to XPORT
   return(0); 
}

static CONNECTION_T* get_connection(linkID_t link_id)
{
   CONNECTION_T* connection = 0;
   uint16_t connection_id = link_id - 1;
   
   //we use link_id to reference connection
   if(connections[connection_id].link_id == link_id)
   {
      connection = &connections[connection_id];   
   }
   
   return(connection); 
}

static int16_t gateway_event_loop()
{
   int16_t err;
   GATEWAY_EVENT_MSG_T event_msg;
   uint8_t len;
   uint8_t* msg_ptr;
   CONNECTION_T* connection;
   while(1)
   {
      err = event_queue_recv(&event_queue, (uint8_t*)&event_msg, 0 /*no sleep*/);
      msg_ptr = msg_out;
      BSP_ASSERT(err >= 0);
      switch(event_msg.event_id)
      {
         case SMPL_MSG_RECV: 
            connection = get_connection((linkID_t)event_msg.data);
            if(connection)
            {
               //only for debug
               BSP_TOGGLE_LED_PORT1((connection->link_id -1));
               if(connection->device_type < DEVICE_MAX)
               {
                  uint8_t device_type;
                  //prepare msg for xport
                  *msg_ptr++ = STATUS_UPDATE;
                  //we will fill in length later
                  msg_ptr++;
                  memcpy(msg_ptr, connection->device_addr, 4);
                  msg_ptr += 4;
                  
                  if(SMPL_SUCCESS == SMPL_Receive((linkID_t)connection->link_id, msg_ptr, &len))
                  {
                     ioctlRadioSiginfo_t sig_info;
                     device_type = msg_out[6];         
                     sig_info.lid = (linkID_t)connection->link_id;
                     msg_ptr += len;
                     //rssi + LQI
                     if(SMPL_SUCCESS == SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SIGINFO, &sig_info))
                     {
                        *msg_ptr++ = sig_info.sigInfo.rssi;
                        *msg_ptr++ = sig_info.sigInfo.lqi;;      
                     }
                     //hop count
                     *msg_ptr++ = 1;
                     //length of entire msg
                     len = msg_ptr - msg_out;                                  
                     //we expect response
                     if(msg_out[7] > 0)
                     {
                        int16_t resp_len;
                        resp_len = connection_callback[device_type](msg_out, len, &msg_in[2]/*leave room for len*/);
                        //send response
                        msg_in[0] = resp_len & 0xFF;
                        msg_in[1] = (resp_len>>8) & 0xFF; 
                        if(resp_len < 0)
                        {
                           //msg_in[0-1] contains error
                           resp_len = 2;            
                        }
                        else
                        {
                           //total transmit length
                           resp_len += 2;
                        }
                        msg_ptr = msg_in;
                        //break into smaller packets
                        do{
                           uint8_t smpl_len = (uint8_t)(resp_len > MAX_APP_PAYLOAD) ? MAX_APP_PAYLOAD : resp_len;
                           //request ack
                           err = SMPL_SendOpt((linkID_t)connection->link_id, msg_ptr, smpl_len, SMPL_TXOPTION_ACKREQ);                   
                           BSP_ASSERT(SMPL_SUCCESS == err || err == SMPL_NO_ACK);
                           if(SMPL_SUCCESS == err)
                           {
                              msg_ptr += smpl_len;
                              resp_len -= smpl_len;
                           }
                           else if(SMPL_NO_ACK == err)
                           {
                              ack_fails++;
                           }
                        }while(resp_len > 0);
                     }
                     else
                     {
                        connection_callback[device_type](msg_out, len, 0 /*no resp*/);   
                     }
                  }
               }
               else
               {
                  //this is the follow up from SMPL_JOIN_REQ
                  *msg_ptr++ = END_DEVICE_JOIN;
                  //we will fill in length later
                  msg_ptr++;                 
                  //success
                  *msg_ptr++ = 0x00;
                  //subtract 1 to get internal link id
                  *msg_ptr++ = event_msg.data - 1;
                  
                  if (SMPL_SUCCESS == SMPL_Receive((linkID_t)connection->link_id, msg_ptr, &len))
                  {
                     if(*msg_ptr < DEVICE_MAX)
                     {
                        smplStatus_t ret;
                        ioctlRadioSiginfo_t sig_info;
                        connection->device_type = *msg_ptr;
                        msg_ptr++;
                        memcpy(connection->device_addr, msg_ptr, 4);
                        msg_ptr += 4;
                        sig_info.lid = (linkID_t)connection->link_id;
                        //rssi + LQI
                        ret = SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SIGINFO, &sig_info);
                        if(ret == SMPL_SUCCESS)
                        {
                           *msg_ptr++ = sig_info.sigInfo.rssi;
                           *msg_ptr++ = sig_info.sigInfo.lqi;      
                        }
                        else
                        {
                           *msg_ptr++ = 0xFF;
                           *msg_ptr++ = 0xFF;
                        }
                        //hop count
                        *msg_ptr++ = 1;
                        //send -> xport
                        err = xport_send_msg(msg_out, msg_ptr - msg_out, msg_in);
                     }
                     else
                     {
                        //error
                     }
                  }     
               }
            }
            else
            {
               //error
               BSP_ASSERT(0);
            }
         break;
         
         case SMPL_JOIN_REQ:
            if(SMPL_SUCCESS == SMPL_LinkListen(&connections[num_connections].link_id))
            {
               //flag as not valid for now
               connections[num_connections].device_type = DEVICE_MAX;
               num_connections++;
            }
         break;
         
         case XPORT_CONNECT_MSG:
            BSP_TURN_OFF_LED1();
         break;
        
      }
   }   
}

void main (void)
{ 
   int16_t ret;
   //set to mx power
   uint8_t level = IOCTL_LEVEL_2;
   BSP_Init();
   
   ret = event_queue_create(&event_queue, sizeof(GATEWAY_EVENT_MSG_T), (uint8_t*)events, MAX_EVENTS);
   BSP_ASSERT(ret >= 0);
   
   ret = xport_init(&event_queue);
   BSP_ASSERT(ret >= 0);
   SMPL_Init(frame_callback);
   
   //max power
   SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SETPWR, &level);
   
   BSP_ASSERT(ret >= 0);
   
   gateway_event_loop();
   //error if we get here
   BSP_TURN_ON_LED1();
   BSP_TURN_ON_LED2();
   while(1);
}

static uint8_t frame_callback(linkID_t id)
{
   GATEWAY_EVENT_MSG_T event_msg;
   if(id)
   {
      event_msg.event_id = SMPL_MSG_RECV;
      event_msg.data = id;
      event_queue_send(&event_queue, (uint8_t*)&event_msg);
   }
   else
   {
      BSP_TURN_ON_LED1();
      event_msg.event_id = SMPL_JOIN_REQ;
      event_msg.data = 0;
      event_queue_send(&event_queue, (uint8_t*)&event_msg);
   }
   //reurn zero, we process in main thread
   return(0);
}
