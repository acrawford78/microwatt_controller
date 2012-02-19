#include <msp430x54x.h>
#include "rtc.h"
static EVENT_QUEUE_T* rtc_events; 

typedef struct{
   uint16_t count;
   uint16_t timeout_sec;
   uint8_t flags;
   ITRV_EVENT_MSG_T event;   
}TIMER_T;

static TIMER_T timer = {0, 0, 0};

static ITRV_EVENT_MSG_T alarm_event;
/**
 * Sets the date in binary format
 *
 */
void rtc_set_time(TIME_T* time, EVENT_QUEUE_T* event_queue)
{
   
   if(event_queue)
   {
      rtc_events = event_queue;
      // Configure RTC_A
      RTCCTL01 |= RTCTEVIE + RTCRDYIE + RTCHOLD + RTCMODE;
                                            // RTC enable, RTC hold
                                            // enable RTC read ready interrupt
                                            // enable RTC time event interrupt    
      //workaround for 37 non A variant
      SetRTCYEAR(time->year);                       
      SetRTCMON(time->mon);                         
      SetRTCDOW(time->day_of_week);                 
      SetRTCDAY(time->day);
      SetRTCHOUR(time->hour);
      SetRTCMIN(time->min);
      SetRTCSEC(time->secs);
      RTCCTL01 &= ~(RTCHOLD);                   // Start RTC calendar mode  
   }
}

void rtc_get_time(TIME_T* time)
{
   while(1)
   {
      //can we read it safely
      if(RTCCTL01 & RTCRDYIFG)
      {
         time->year = RTCYEAR;                       
         time->mon = RTCDAY;                         
         time->day_of_week = RTCDOW;                 
         time->day = RTCDAY;
         time->hour = RTCHOUR;
         time->min = RTCMIN;
         time->secs = RTCSEC;
         //check still safe else re-read
         if(RTCCTL01 & RTCRDYIFG)
         {
            break;
         }
      }
   }
}

void rtc_set_alarm(uint8_t day_of_week, uint8_t hour, uint8_t min, ITRV_EVENT_MSG_T event)
{
   //disable interrupts
   RTCCTL01 &= ~(RTCAIE | RTCAIFG);             
   RTCAMIN = 0;
   RTCAHOUR = 0;
   RTCADAY = 0;
   RTCADOW = 0;
   
   //set alarm enabling interrupts
   RTCADOW = (RTC_AE | day_of_week);
   RTCAHOUR = (RTC_AE | hour);
   RTCAMIN = (RTC_AE | min);
   alarm_event = event; 
   RTCCTL01 |= RTCAIE;
}

void rtc_set_timer(uint16_t timeout_sec, uint8_t flags, ITRV_EVENT_MSG_T event)
{
   timer.flags = flags;
   timer.count = timer.timeout_sec = timeout_sec;
   timer.event = event;
}

#pragma vector=RTC_VECTOR
__interrupt void RTC_ISR(void)
{
   switch(__even_in_range(RTCIV,16))
   {
      case RTC_NONE:                          // No interrupts
      break;
      case RTC_RTCRDYIFG:                     // RTCRDYIFG, every second
         if(timer.count)
         {
            timer.count--;
            if(timer.count == 0)
            {            
               event_queue_send(rtc_events, (uint8_t*)&timer.event);
               if(timer.flags == TIMER_REPEAT)
               {
                  timer.count = timer.timeout_sec;
               }
               __bic_SR_register_on_exit(LPM0_bits);   // Exit to active CPU
               break;   
            }
         }
         
         #ifdef RTC_RDY_MSG
         msg.event_id = ITRV_RTC_RDY;
         msg.data = RTC_RDY;
         event_queue_send(rtc_events, (uint8_t*)&msg);
         __bic_SR_register_on_exit(LPM0_bits);   // Exit to active CPU
         #endif
      break;
      case RTC_RTCTEVIFG:                     // RTCEVIFG            
      break;
      case RTC_RTCAIFG:                       // RTCAIFG
         event_queue_send(rtc_events, (uint8_t*)&alarm_event);
         __bic_SR_register_on_exit(LPM0_bits);   // Exit to active CPU
      break;
      case RTC_RT0PSIFG:                      // RT0PSIFG
      break;
      case RTC_RT1PSIFG:                      // RT1PSIFG
      break;
      case 12: break;                         // Reserved
      case 14: break;                         // Reserved
      case 16: break;                         // Reserved
      default: break;
  }
}
