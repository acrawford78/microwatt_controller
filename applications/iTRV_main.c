#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
//#include "vlo_rand.h"
#include "device_type.h"
#include "../common/event_queue.h"
#include "../common/msg_common.h"
#include "rtc.h"
#include "i2c_driver.h"
#include "temp_LM75.h"
#include "uart.h"
#include "timer.h"
#include "buttons.h"

//event queue
#define MAX_EVENTS 8
EVENT_QUEUE_T event_queue;
ITRV_EVENT_MSG_T events[MAX_EVENTS];

//event_callback function
typedef enum{
   ITRV_NONE = 0,
   ITRV_TX_FAIL = -1,
   ITRV_RX_FAIL = -2,
   ITRV_TIMEOUT = -3,
   ITRV_GATEWAY_NACK = -4
}ITRV_ERROR_T;
static int16_t send_temp_update(uint8_t arg);
static int16_t button_handler(uint8_t button);
typedef int16_t (*ITRV_CALLBACK_T)(uint8_t arg);
static ITRV_CALLBACK_T event_callback[ITRV_MAX_EVENT];

//we sleep when waiting for response, this wakes us up
static uint8_t recv_frame_callback(linkID_t id);
static volatile uint16_t frame_recv = 0;
static volatile uint16_t timeout_count = 0;

//buttons callback
static void port1_button_cb(uint8_t button);

//request status update every UPDATE_REQ times
#define STATUS_UPDATE_REQ      2
#define TEMP_UPDATE_SEC        10 
static uint16_t update_count = STATUS_UPDATE_REQ;

//link to gateway
static linkID_t link_id = 0;
//store flash id in information segment C
uint8_t *flash_addr = (uint8_t *)0x1880;
static addr_t device_addr;

//for now use a big buffer to handle the massive profile updates
static uint8_t response_in[320];

static void set_device_address();
static smplStatus_t link_gateway();

void main(void)
{
   ITRV_EVENT_MSG_T msg = {ITRV_READ_TEMP, 0x03};
   TIME_T time = {2011, 2, 19, 6 /*sat*/, 23, 59, 50};
   
   event_callback[ITRV_READ_TEMP] = send_temp_update;
   event_callback[ITRV_BUTTON] = button_handler;
   
   
   event_queue_create(&event_queue, sizeof(ITRV_EVENT_MSG_T), (uint8_t*)events, MAX_EVENTS);
   
   //Initialize board-specific hardware
   BSP_Init();
   
   i2c_init(256);
   
   timer_init();
   
   //Read device from flash and tell simpliciTI stack
   BSP_ENABLE_INTERRUPTS();
   set_device_address();
   
   /* Unconditional link to AP which is listening due to successful join. */
   if(SMPL_SUCCESS != link_gateway())
   {
      BSP_TURN_ON_LED2();
      BSP_ASSERT(0);
   }
   
   //request profile + time 0x03
   //err = send_temp_update(0x03);
   //set temp update 
   rtc_set_timer(TEMP_UPDATE_SEC, TIMER_ONESHOT, msg);
   //set time
   rtc_set_time(&time, &event_queue);
   //main event processing loop
   {
      ITRV_EVENT_MSG_T msg;
      int16_t status;
      while(1)
      {
         //blocks waiting for event
         event_queue_recv(&event_queue, (uint8_t*)&msg, 1 /*sllep*/);
      
         status = event_callback[msg.event_id](msg.data);
         if(status < 0)
         {
            BSP_TOGGLE_LED2();
         }
      }
   }   
}

/***
   Read temperature sensor and send to gateway
   void* arg : ptr to uint8_t status req byte
   ret < 0 error
   ret = 0 ok but no msg response
   ret > 0 size of resp msg        
 */
static int16_t send_temp_update(uint8_t arg)
{
   #ifdef WANT_SIGINFO
   ioctlRadioSiginfo_t sig_info;
   #endif
   int16_t ret = ITRV_NONE;
   int16_t temp = 24;
   int16_t len;
   uint8_t msg[24];
   ITRV_EVENT_MSG_T next_event = {ITRV_READ_TEMP, 0x00};
   uint8_t *msg_ptr = msg;
   
   ret = temp_LM75_read_int(&temp, 0x4F);
   BSP_ASSERT(ret == 2);
   
   *msg_ptr++ = DEVICE_ITRV;
   
   //add status byte
   *msg_ptr++ = arg;
   //wake radio
   SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE, 0);
   
#ifdef WANT_SIGINFO
   sig_info.lid = link_id;
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
#endif
   //big-endian..for now
   //room temp 
   *msg_ptr++ = (uint8_t)(temp>>8);    
   *msg_ptr++ = (uint8_t)(temp & 0xFF);
   //pipe temp
   *msg_ptr++ = (uint8_t)(temp>>8);    
   *msg_ptr++ = (uint8_t)(temp & 0xFF);
   *msg_ptr++ = 0x11;  //valve position
   //calculate len
   len = (msg_ptr-msg);
   BSP_ASSERT((len) <= sizeof(msg));
   //Send msg to gateway
   ret = SMPL_SendOpt(link_id, msg, len, SMPL_TXOPTION_NONE);
   //if status set then wait for reply
   if((SMPL_SUCCESS == ret) && (msg[1] > 0))
   {
      TIMER_ID timer; 
      TIMER_ACTION_T action;
      static volatile uint16_t timeout = 0;
      int16_t resp_len = 0;
      //we always expect the two length bytes
      len = 0;
      //put radio in recv state
      ret = SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0);
      BSP_ASSERT(SMPL_SUCCESS == ret);
      //set timeout one shot
      action.event = &timeout;
      timer = set_timer(300 /* ticks == 10ms*/, action, TIMER_SET_EVENT);
      BSP_ASSERT(timer >= 0);
      //wait for profile update
      do{
         BSP_DISABLE_INTERRUPTS();
         if(frame_recv)
         {
            uint8_t recv_len;
            --frame_recv;
            BSP_ENABLE_INTERRUPTS();
            ret = SMPL_Receive(link_id, &response_in[len], &recv_len);
            
            if(SMPL_SUCCESS == ret)
            {         
               //extract length on first response
               if(len == 0)
               {
                  resp_len = (response_in[1]<<8) | response_in[0];                
                  if(resp_len < 0)
                  {
                     ret = ITRV_GATEWAY_NACK;
                     break;
                  }
                  //adjust length to account for the 2 len bytes
                  resp_len += 2;                  
               }    
               len += recv_len;
               if(len >= resp_len)
               {   
                  ret = len;
                  break;
               }    
            }
            else
            {
               ret = ITRV_RX_FAIL;
               break;   
            }
         }
         else
         {
            BSP_ENABLE_INTERRUPTS();
            //sleep with ENABLE_INTERRUPTS
            __bis_SR_register(LPM0_bits+GIE);
         }
      }while(1 && !timeout);
      
      if(timeout)
         ret = ITRV_TIMEOUT;
      else
         cancel_timer(timer);
   }
   
   /* Put radio back to sleep */
   SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);
   BSP_TOGGLE_LED1();
   
   if(0 == update_count)
   {
      //next time
      next_event.data = 0x01;
      update_count = STATUS_UPDATE_REQ;
   }
   else
   {
      update_count--;
   }
   
   //set temp update 
   rtc_set_timer(TEMP_UPDATE_SEC, TIMER_ONESHOT, next_event);
   
   return(ret);
}

static int16_t button_handler(uint8_t button)
{
   BSP_TOGGLE_LED2();
   return(0);   
}

static void set_device_address()
{
   uint16_t rand;
  
   /* If no address then create a random one */
   #if 0
   //TODO xxx, not save to flsh we want to test out button random code
   if(flash_addr[0] == 0xFF && flash_addr[1] == 0xFF &&
        flash_addr[2] == 0xFF && flash_addr[3] == 0xFF )
   {
      BSP_DISABLE_INTERRUPTS();                 // 5xx Workaround: Disable global
      FCTL3 = FWKEY;                            // Clear Lock bit
      FCTL1 = FWKEY+ERASE;                      // Set Erase bit
      *flash_addr = 0;                          // Dummy write to erase Flash seg D
      FCTL1 = FWKEY+WRT;                        // Set WRT bit for write operation, byte write
      flash_addr[0]= 0x55;
      flash_addr[1]=rand & 0xFF;
      flash_addr[2]=(rand >> 8) & 0xFF;
      flash_addr[3]= 0x55;
      FCTL1 = FWKEY;                            // Clear WRT bit
      FCTL3 = FWKEY+LOCK;                       // Set LOCK bit
      BSP_ENABLE_INTERRUPTS();
   }
   
   /* Read out address from flash */
   device_addr.addr[0] = flash_addr[0];
   device_addr.addr[1] = flash_addr[1];
   device_addr.addr[2] = flash_addr[2];
   device_addr.addr[3] = flash_addr[3];
   #endif
   
   //enable P1.1 button
   BSP_ENABLE_INTERRUPTS();
   {
      ITRV_EVENT_MSG_T msg;
      
      buttons_init(port1_button_cb, (1<<1));
      //set off timer B, use we use for random SMPL_ADDR 
      TBCCR0 = 0xffff;
      TBCTL = TBSSEL_1 + MC_1 + TBCLR;
   
      while(1)
      {
         //blocks waiting for event
         event_queue_recv(&event_queue, (uint8_t*)&msg, 1 /*sllep*/);
         BSP_ASSERT(msg.event_id == ITRV_BUTTON);
         if(msg.event_id == ITRV_BUTTON)
         {
            //todo testing
            event_callback[msg.event_id](msg.data);
            break;
         }
      }   
      
      //store value, disable timer
      rand = TB0R;
      TBCTL &= MC_0;
   }
   device_addr.addr[0] = 0xA5;
   device_addr.addr[1] = rand & 0xFF;;
   device_addr.addr[2] = (rand >> 8) & 0xFF;
   device_addr.addr[3] = 0xA5;
   
   /* Tell network stack the device address */
   SMPL_Ioctl(IOCTL_OBJ_ADDR, IOCTL_ACT_SET, &device_addr);
}

static smplStatus_t link_gateway()
{
   smplStatus_t status;
   uint8_t msg[5];
   TIMER_ID timer;
   TIMER_ACTION_T action;
   int16_t err;
   uint8_t level = IOCTL_LEVEL_2;
   
   BSP_TURN_ON_LED1();
   BSP_TURN_ON_LED2();
   //join
   action.event = &timeout_count;
   timer = set_timer(300 /* ticks == 10ms*/, action, TIMER_SET_EVENT|TIMER_CONTINUOUS);
   while(SMPL_SUCCESS != SMPL_Init(recv_frame_callback))
   {
      if(timeout_count)
      {
         BSP_TOGGLE_LED1();
         timeout_count = 0;
      }
      //Timer interrupt will wake CPU up every second to retry initializing
      __bis_SR_register(LPM0_bits+GIE);
      __no_operation();
   }
   
   //max power
   SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SETPWR, &level);
   
   //now link
   while (SMPL_SUCCESS != SMPL_Link(&link_id))
   {
      if(timeout_count)
      {
         BSP_TOGGLE_LED1();
         timeout_count = 0;
      }
      __bis_SR_register(LPM0_bits+GIE);
      __no_operation();
   }
   
   BSP_TURN_OFF_LED1();
   BSP_TURN_OFF_LED2();
   //disable timer
   err = cancel_timer(timer);
   BSP_ASSERT(err >= 0);
   
   //send device type and address to gateway
   msg[0] = DEVICE_ITRV;
   msg[1] = device_addr.addr[0];
   msg[2] = device_addr.addr[1];
   msg[3] = device_addr.addr[2];
   msg[4] = device_addr.addr[3];
   
   //send reply with device addr
   status = SMPL_SendOpt(link_id, msg, sizeof(msg), SMPL_TXOPTION_NONE);
   
   return(status);
}

static uint8_t recv_frame_callback(linkID_t id)
{
   if(id)
   {
      BSP_ASSERT(link_id == id);
      ++frame_recv;      
   }
   return(0);
}

static void port1_button_cb(uint8_t button)
{
   ITRV_EVENT_MSG_T event;
   event.event_id = ITRV_BUTTON;
   event.data = button;
   event_queue_send(&event_queue, (uint8_t*)&event);   
}


