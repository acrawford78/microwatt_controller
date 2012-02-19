#ifndef TIMER_H_
#define TIMER_H_
#include <stdint.h>

//keep this small, only intended for a few..
#define MAX_TIMERS   4
typedef int16_t TIMER_ID;
#define TIMER_CONTINUOUS   (1<<0)
#define TIMER_CB           (1<<1)
#define TIMER_SET_EVENT    (1<<2)
typedef void (*TIMER_CALLBACK_T)(void);
typedef volatile uint16_t* TIMER_EVENT_T;

typedef union
{
   TIMER_EVENT_T event;
   TIMER_CALLBACK_T func_cb;   
}TIMER_ACTION_T;
typedef struct
{
   uint8_t flags;
   uint16_t timeout_ticks;
   uint16_t reset_ticks;
   TIMER_ACTION_T action;
}TIMER_VALUE_T;

typedef struct
{
   TIMER_VALUE_T value[MAX_TIMERS];
   TIMER_ID count;    
}TIMERS_T;

void timer_init();

TIMER_ID set_timer(uint16_t timeout_ticks, TIMER_ACTION_T expired_action, uint8_t flags);

int16_t cancel_timer(TIMER_ID timer_id);

#endif /*TIMER_H_*/
