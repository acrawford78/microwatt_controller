#include <stdint.h>
#include <msp430x54x.h>
#include "../interface/xport_msg.h"
#include "../common/msg_common.h"
#include "../common/event_queue.h"
#include "../drivers/chip/uart.h"
#include "../drivers/chip/timer.h"

typedef enum{
   WAITING_MSG = 0,
   ACK_BYTE,
   CRC_LSB,
   CRC_MSB,
   MSG_LEN_LSB,
   MSG_LEN_MSB,
   MSG_DATA_RESP,    //response from msg
   MSG_DATA_CMD      //new command from xport  
}RX_STATE_T;

#define MAX_RETRIES    1
static volatile int16_t resp_waiting;
static volatile uint16_t resp_bytes;
typedef enum{
   RESP_MSG_GOOD = 0,
   RESP_MSG_CORRUPT = -1,
   RESP_CRC_FAIL = -2,
   RESP_TIMEOUT = -3
}RESP_CODES_T;

static uint8_t curr_state = WAITING_MSG;
static uint16_t bytes_left;
static uint8_t* msg_ptr = 0;
static uint16_t crc_recv;
static GATEWAY_EVENT_MSG_T cmd_msg;
static EVENT_QUEUE_T* xport_events;

static int16_t xport_msg_callback(uint8_t byte);

int16_t xport_init(EVENT_QUEUE_T* event_queue)
{
   int16_t err = -1;
   if(event_queue)
   {
      xport_events = event_queue;
      //115K
      uart_init(69);
      err = uart_set_recv_cb(PORT_A, xport_msg_callback);
   }
   return(err);
}
/*
 * Block until msg sent, resp_msg = 0 if not response
 * If response is expected then we block until: timeout, response or error
 * Return: num_bytes received - (header + crc) or < 0 error
 */
int16_t xport_send_msg(uint8_t* data, uint16_t num_bytes, uint8_t* resp_msg)
{
   int16_t status = -1;
   
   //NEW MSP->XPORT
   if((data[0] & RESPONSE) == 0)
   {
      uint16_t i;
      //we add 2 to account for CRC bytes before performing CRC
      data[1] = (uint8_t)((num_bytes + 2) & 0xFF);
      //we share CRC block with uart RX so wait until no msg before using
      while(curr_state != WAITING_MSG);
      //disable RX int
      UCA1IE &= ~UCRXIE;
      CRCINIRES = 0xFFFF; 
      for(i = 0; i < num_bytes; i++)
      {
         CRCDI_L = data[i];      
      }
      //big-endian    
      data[num_bytes] = (uint8_t)(CRCINIRES>>8);    
      num_bytes++;
      data[num_bytes] = (uint8_t)(CRCINIRES & 0xFF);
      num_bytes++;
      UCA1IE |= UCRXIE;
   }
   
   //we want to check for profile update
   if(resp_msg)
   {
      TIMER_ID timer;
      TIMER_ACTION_T action;
      static volatile uint16_t timeout = 0;
      uint16_t retry = 0;
      //top bit used to indicate resonse we will wait for
      resp_waiting = RESPONSE | data[0];
      //set up response buffer
      msg_ptr = resp_msg;
      //set timeout one shot
      action.event = &timeout;
      uart_write(data, PORT_A, num_bytes);
      timer = set_timer(100 /* ticks == 10ms*/, action, TIMER_SET_EVENT);
      do
      {
         if(resp_waiting == RESP_MSG_GOOD)
         {
            //return length if msg has data
            if(resp_bytes)
               status = resp_bytes - 2; //-2 to remove CRC bytes
            else
               status = RESP_MSG_GOOD;
            break;
         }
         else if((resp_waiting == RESP_MSG_CORRUPT) || (resp_waiting == RESP_CRC_FAIL))
         {
            //original msp->xport corrupt, or xport->msp corrurpt resend
            if(retry < MAX_RETRIES)
            {
               resp_waiting = RESPONSE | data[0];
               uart_write(data, PORT_A, num_bytes);
               retry++;
            }
            else
            {
               status = resp_waiting;
               break;
            }      
         }
        
      }while(!timeout);
      
      if(timeout)
         status = RESP_TIMEOUT;
      else
         cancel_timer(timer);
   }
   else
   {
      resp_waiting = 0;
      status = uart_write(data, PORT_A, num_bytes);   
   }
   
   return(status);
}

static uint16_t bytes_recv = 0;
static int16_t xport_msg_callback(uint8_t byte)
{
   bytes_recv++;
   if(curr_state == WAITING_MSG)
   {
      //response, check we are waiting for this
      if((byte >= 0x80) && (resp_waiting == byte))
      {
         //we expect at least ACK byte
         curr_state = ACK_BYTE;
      }
      else if(byte < XPORT_MSG_MAX)
      {
         curr_state = MSG_DATA_CMD;
         //set bytes to parse based on msg type
         if(byte == CONNECT_DEVICE)
         {
            cmd_msg.event_id = XPORT_CONNECT_MSG;
            bytes_left = 1; 
         }
      }
      else
      {
         curr_state = WAITING_MSG;
         return(-1);
      }
      
      //init crc
      CRCINIRES = 0xFFFF;
      CRCDI_L = byte;
      
   }
   else if(curr_state == ACK_BYTE)
   {
      if(byte == 0x55)
      {
         //ACK with data
         curr_state = MSG_LEN_LSB;
      }
      else if(byte == 0xA5)
      {
         //ACK with no data
         resp_waiting = RESP_MSG_GOOD;
         curr_state = WAITING_MSG;
         msg_ptr = 0;
         resp_bytes = 0;   
      }
      else
      {
         //NACK
         curr_state = WAITING_MSG;
         resp_waiting = RESP_MSG_CORRUPT;  
         return(-1);
      }
      CRCDI_L = byte;     
   }
   else if(curr_state == MSG_LEN_LSB)
   {
      bytes_left = byte;
      if(bytes_left >= 3)
      {
         resp_bytes = bytes_left;
         curr_state = MSG_LEN_MSB;
         CRCDI_L = byte;
      }
      else
      {
         //bad error we always expect atleast STATUS_RESP, CRC_LSB, CRC_MSB, fail otherwise
         curr_state = WAITING_MSG;
         resp_waiting = RESP_MSG_CORRUPT;   
      }
   }
   else if(curr_state == MSG_LEN_MSB)
   {
      bytes_left |= (byte<<8);
      if(bytes_left <= XPORT_MAX_MSG)
      {
         resp_bytes = bytes_left;
         curr_state = MSG_DATA_RESP;
         CRCDI_L = byte;  
      }
      else
      {
         //bad error
         curr_state = WAITING_MSG;
         resp_waiting = RESP_MSG_CORRUPT;
      }
   }
   else if(curr_state == MSG_DATA_RESP)
   {
      *msg_ptr++ = byte;
      CRCDI_L = byte;
      bytes_left--;
      if(bytes_left == 2)
      {
         curr_state = CRC_LSB;   
      }
        
   }
   else if(curr_state == MSG_DATA_CMD)
   {
      cmd_msg.data = byte;
      CRCDI_L = byte;
      curr_state = CRC_LSB;   
   }
   else if(curr_state == CRC_LSB)
   {
      //msb first
      crc_recv = byte;
      curr_state = CRC_MSB;
   }
   else if(curr_state == CRC_MSB)
   {
      crc_recv |= ((uint16_t)byte)<<8;
      if(crc_recv == CRCINIRES)
      {
         if(resp_waiting)
         {
            //free waiting       
            resp_waiting = RESP_MSG_GOOD;
         }
         else
         {
            //queue for main()
            event_queue_send(xport_events, (uint8_t*)&cmd_msg);
         }     
      }
      else
      {
         if(resp_waiting)
         {
            //indicate CRC failure
            resp_waiting = -1;   
         }
         else
         {
            //queue
         }
      }
      
      curr_state = WAITING_MSG;
      msg_ptr = 0; 
   }
   
   return(0);
}
