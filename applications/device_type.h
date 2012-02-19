#ifndef DEVICE_TYPE_H_
#define DEVICE_TYPE_H_

typedef enum {
   DEVICE_ITRV =  0x00,
   DEVICE_TEMP,
   DEVICE_MAX
}DEVICE_T;

/* ITRV */
#define STATUS_BYTE_ITRV   7


#define MSG_LEN_TEMP    (DATA_SIG + DATA_TEMP)     
#endif /*DEVICE_TYPE_H_*/
