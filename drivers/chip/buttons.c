//#include <msp430x54x.h>
#include <stdint.h>
#include "bsp.h"
#include "bsp_leds.h"
#include "buttons.h"
#include "timer.h"
#define MAX_CHECKS   4
static uint8_t wait_rising_edge = 0x00;
//history of port state
static uint8_t port_state[MAX_CHECKS] = {0};
static uint8_t last_port_state = 0xFF;
static uint8_t index = 0;
static uint8_t timeout = 0;
static uint8_t port1_buttons;
static TIMER_ID button_timer = -1;
static BUTTON_CALLBACK_T port1_callback;
static void button_debounce_cb(void);

void buttons_init(BUTTON_CALLBACK_T func, uint8_t buttons)
{
   if(func && buttons)
   {
      uint8_t i;
      //disable wiile changing
      P1IE &= ~port1_buttons;
      port1_buttons = buttons;
      port1_callback = func;
      P1DIR &= ~(buttons);
      P1REN |= port1_buttons;
      P1OUT |= port1_buttons;
      P1IES |= port1_buttons;
      P1IFG &= ~port1_buttons;
      P1IE  |= port1_buttons;
      
      for(i = 0; i < MAX_CHECKS; i++)
         port_state[i] = 0;
         
   }
}
/*
 * Called from timer.c interrupt
 */
static void button_debounce_cb(void)
{
   uint8_t i, debounced, input_changed;
   port_state[index] = P1IN;
   ++index;
   debounced = 0xFF;
   for(i = 0; i < MAX_CHECKS; i++)
   {
      debounced = (debounced & port_state[i]);
   }
   //loop back around
   if(index >= MAX_CHECKS)
      index = 0;

   //XOR to find chnaged inputs
   input_changed = debounced ^ last_port_state; 
   if(input_changed)
   {
      uint8_t bit, out_buttons;
      bit = 0x01;
      out_buttons = 0x00;
      for(i = 0; i < 8; i++)
      { 
         //if bit is button and changed
         if(port1_buttons & input_changed & bit)
         {
            //gone high
            if(debounced & bit)
            {
               wait_rising_edge &= ~bit;
               out_buttons |= bit;
               //re-enable interrupt for button
               P1IE |= bit;       
            }
            else
            {
               //now wait for rising edge
               wait_rising_edge |= bit;
            }
               
         }
         bit <<= 1;
      }
      //one callback for all buttons
      if(out_buttons && port1_callback)
         port1_callback(out_buttons);   
   }
   else if(!wait_rising_edge)
   {
      //if not waiting for button release, dec timout
      if(--timeout == 0)
      {
         cancel_timer(button_timer);
         button_timer = -1;
         //re-enable interrupts
         P1IE  |= port1_buttons;
      }   
   }
   last_port_state = debounced;
}

#pragma vector=PORT1_VECTOR
__interrupt void PORT_1_ISR(void)
{
   uint8_t falling_edge = 0;
   switch(__even_in_range(P1IV,4))
   {
      case 4:        //Vector 4: Port 1 bit 1
         falling_edge |= (1<<1);
         P1IE &= ~(1<<1);
      break;
   }
     
   if(falling_edge)
   {
      //allow atleast MAX_CHECKS for stable low
      timeout += MAX_CHECKS;  
      //enable timer if not already running
      if(button_timer == -1)
      {
         TIMER_ACTION_T action;
         action.func_cb = button_debounce_cb;   
         button_timer = set_timer(1, action, TIMER_CB|TIMER_CONTINUOUS);
         BSP_ASSERT(button_timer >= 0);
      }
   } 
}
