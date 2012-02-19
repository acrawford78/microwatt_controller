#ifndef XPORT_H_
#define XPORT_H_
#include "../common/event_queue.h"

int16_t xport_init(EVENT_QUEUE_T* event_queue);
int16_t xport_send_msg(uint8_t* data, uint16_t num_bytes, uint8_t* resp_msg);
#endif /*XPORT_H_*/
