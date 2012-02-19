#include <string.h>
#include "timer.h"
#include "bsp.h"
#include "bsp_leds.h"
#define TEST_TIMER
#define TICK_MS   10
static TIMERS_T timers;

void timer_init()
{
   memset((uint8_t*)&timers, 0, sizeof(timers));
}

#ifdef TEST_TIMER
uint16_t count = 0;
static void timer_cb_1(void)
{
   count++;
   if(count == 100)
   {
      BSP_TOGGLE_LED1();
      count = 0;
   }
}

static void timer_cb_2(void)
{
   count++;
   if(count == 100)
   {
      BSP_TOGGLE_LED2();
      count = 0;
   }
}

void timer_test()
{
   TIMER_ACTION_T action;
   action.func_cb = timer_cb_1; 
   set_timer(1, action, TIMER_CB|TIMER_CONTINUOUS);
   action.func_cb = timer_cb_2;
   set_timer(2, action, TIMER_CB|TIMER_CONTINUOUS);  
}
#endif

/**
 *    Min value 1 tick == 10ms. We set timer at tick at 5ms.
 *    We can make this more accurate however that will increase number of interrupts
 *    
 */
TIMER_ID set_timer(uint16_t timeout_ticks, TIMER_ACTION_T expired_action, uint8_t flags)
{
   int16_t status = -1;
   BSP_ASSERT(timeout_ticks <= (65535/2));
   if(timeout_ticks && expired_action.func_cb)
   {
      TIMER_ID timer_id = 0;
      while(timer_id < MAX_TIMERS)
      {
         //timer is free
         if(timers.value[timer_id].timeout_ticks == 0)
         {
            bspIState_t s;
            BSP_ENTER_CRITICAL_SECTION(s);
            //actual interrupt is 5ms so x 2
            timers.value[timer_id].timeout_ticks = (timeout_ticks*2);
            if(flags & TIMER_CONTINUOUS)
               timers.value[timer_id].reset_ticks = timers.value[timer_id].timeout_ticks;
            else
               timers.value[timer_id].reset_ticks = 0;        
            timers.value[timer_id].flags = flags;       
            timers.value[timer_id].action = expired_action;
            status = timer_id;
            ++timers.count;
            BSP_EXIT_CRITICAL_SECTION(s);
            //first timer
            if(timers.count == 1)
            {
               //5000us x CLOCK
               TA1CCR0 = (uint16_t)5000*BSP_CONFIG_CLOCK_MHZ_SELECT;
               TA1CTL = TASSEL_2 + MC_1 + TACLR;       // SMCLK, upmode, clear TAR
               TA1CCTL0 = CCIE;                        // CCR0 interrupt enabled
            }        
            break;
         }
         ++timer_id;  
      }
   }  
   return(status);
}

int16_t cancel_timer(TIMER_ID timer_id)
{
   int16_t status = -1;
   if((timer_id >= 0) && (timer_id < MAX_TIMERS))
   {
      bspIState_t s;
      BSP_ENTER_CRITICAL_SECTION(s);
      timers.value[timer_id].timeout_ticks = 0;
      timers.value[timer_id].action.func_cb = 0;
      timers.value[timer_id].reset_ticks = 0;
      --timers.count;
      if(timers.count == 0)
      {
         //disable
         TA1CCTL0 = 0;
         TA1CTL &= 0xFF00;
      }
      status = timers.count;
      BSP_EXIT_CRITICAL_SECTION(s);
      
   }
   return(status);
}

// Timer A0 interrupt service routine
#pragma vector=TIMER1_A0_VECTOR
__interrupt void TIMER1_A0_ISR(void)
{
   TIMER_ID timer_id = 0;
   while((timer_id < MAX_TIMERS) && timers.count)
   {
      if(timers.value[timer_id].timeout_ticks)
      {
         --timers.value[timer_id].timeout_ticks;
         if(timers.value[timer_id].timeout_ticks == 0)
         {

            if(timers.value[timer_id].flags & TIMER_CB)
               timers.value[timer_id].action.func_cb();
            else if(timers.value[timer_id].flags & TIMER_SET_EVENT)
               *timers.value[timer_id].action.event = ~(*timers.value[timer_id].action.event);
            else
               BSP_ASSERT(timers.value[timer_id].flags & (TIMER_SET_EVENT|TIMER_CB));
            //repeat if required   
            if(timers.value[timer_id].flags & TIMER_CONTINUOUS)
               timers.value[timer_id].timeout_ticks = timers.value[timer_id].reset_ticks;
            else
               --timers.count;
            __bic_SR_register_on_exit(LPM0_bits);
         }
      }
      ++timer_id;
   }
   if(timers.count == 0)
   {
      //disable
      TA1CCTL0 = 0;
      TA1CTL &= 0xFF00;
   }
}
