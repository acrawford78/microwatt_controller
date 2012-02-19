#ifndef RTC_H_
#define RTC_H_
#include <stdint.h>
#include "../../common/msg_common.h"
#include "../../common/event_queue.h"

#define RTC_AE 0x80

extern int SetRTCYEAR(int year);    
extern int SetRTCMON(int month);
extern int SetRTCDAY(int day);
extern int SetRTCDOW(int dow);
extern int SetRTCHOUR(int hour);
extern int SetRTCMIN(int min);
extern int SetRTCSEC(int sec);

typedef enum{
   TIMER_ONESHOT = 0,
   TIMER_REPEAT
}RTC_TIMER_FLAGS_T;

typedef struct{
   int16_t year;
   int16_t mon;
   int16_t day;
   int16_t day_of_week;
   int16_t hour;
   int16_t min;
   int16_t secs;   
}TIME_T;

void rtc_set_time(TIME_T* time, EVENT_QUEUE_T* event_queue);
void rtc_get_time(TIME_T* time);
void rtc_set_alarm(uint8_t day_of_week, uint8_t hour, uint8_t min, ITRV_EVENT_MSG_T event);
void rtc_set_timer(uint16_t timeout_sec, uint8_t flags, ITRV_EVENT_MSG_T event);
#endif /*RTC_H_*/
