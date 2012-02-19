#ifndef XPORT_MSG_H_
#define XPORT_MSG_H_

#define XPORT_MAX_MSG   (320)   
//set if msg is a response to previous msg
#define RESPONSE        0x80
typedef enum{
   CONNECT_DEVICE = 0,
   PROFILE_UPDATE, 
   REQUEST_NEW_PROFILE,
   STATUS_UPDATE,
   SET_POWER,
   UN_LINK,
   END_DEVICE_JOIN,
   XPORT_MSG_MAX   
}XPORT_MSG_T;


#endif /*XPORT_MSG_H_*/
