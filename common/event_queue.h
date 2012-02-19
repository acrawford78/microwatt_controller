#ifndef EVENT_QUEUE_H_
#define EVENT_QUEUE_H_

typedef struct{
   uint8_t* read_ptr;
   uint8_t* write_ptr;
   uint8_t* end_ptr;
   uint8_t* start;
   uint16_t event_size;
   volatile int16_t count;
   uint16_t total_slots;
}EVENT_QUEUE_T;

int16_t event_queue_create(EVENT_QUEUE_T* queue, uint16_t event_size,  uint8_t* queue_start, uint16_t queue_len);

int16_t event_queue_send(EVENT_QUEUE_T* queue, uint8_t* event_msg);

int16_t event_queue_recv(EVENT_QUEUE_T* queue, uint8_t* event_msg, uint16_t sleep);
#endif /*EVENT_QUEUE_H_*/
