#ifndef MSG_COMMON_H_
#define MSG_COMMON_H_

typedef struct{
   uint8_t event_id;
   uint8_t data;   
}GATEWAY_EVENT_MSG_T;

typedef enum{
   SMPL_MSG_RECV = 0,
   SMPL_JOIN_REQ,
   XPORT_CONNECT_MSG
}GATEWAY_EVENT_TYPE_T;

/**
 * ITRV
 */
typedef struct{
   uint8_t event_id;
   uint8_t data;   
}ITRV_EVENT_MSG_T;

typedef enum{
   ITRV_READ_TEMP = 0,
   ITRV_BUTTON,
   ITRV_RTC_ALARM,
   ITRV_MAX_EVENT
}ITRV_EVENT_T;

#endif /*MSG_COMMON_H_*/
