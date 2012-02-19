#include <stdint.h>
#include <string.h>
#include "bsp.h"
#include "event_queue.h"

int16_t event_queue_create(EVENT_QUEUE_T* queue, uint16_t event_size,  uint8_t* queue_start, uint16_t queue_len)
{
   int16_t status = -1;
   memset((uint8_t*)queue, 0, sizeof(*queue));
   if(queue && queue_start && (event_size <= queue_len))
   {
      queue->count = 0;
      queue->event_size = event_size;
      queue->start = queue_start;
      queue->read_ptr = queue->write_ptr = queue_start;
      queue->total_slots = queue_len/event_size;
      queue->end_ptr = queue->start + (event_size*queue->total_slots);
      //return free slots
      status = queue->total_slots;
   }
   return(status);
}

/*
 * Always called within interrupt, no need to disable interrupts
 */
int16_t event_queue_send(EVENT_QUEUE_T* queue, uint8_t* event_msg)
{
   int16_t status = -1;
   if(queue->count < queue->total_slots)
   {
      //add the msg
      memcpy((void*)queue->write_ptr, (void*)event_msg, queue->event_size);
      queue->write_ptr += queue->event_size;
     
      //loop back to the start
      if(queue->write_ptr >= queue->end_ptr)
         queue->write_ptr = queue->start;
      
      queue->count++;
      status = queue->count;
   }
   return(status);
}

int16_t event_queue_recv(EVENT_QUEUE_T* queue, uint8_t* event_msg, uint16_t sleep)
{
   int16_t status = -1;
   if(queue && event_msg)
   {
      do{
         //we need to disable before checking queue->count to avoid going to sleep with count > 0
         BSP_DISABLE_INTERRUPTS();
         //check if we have a new event
         if(queue->count)
         {
            BSP_ENABLE_INTERRUPTS();
            //we can do this will interrupts off as queue->count protects us
            memcpy((void*)event_msg, (void*)queue->read_ptr, queue->event_size);
            queue->read_ptr += queue->event_size;
            //loop back to the start
            if(queue->read_ptr >= queue->end_ptr)
               queue->read_ptr = queue->start;
            
            //now update the count
            BSP_DISABLE_INTERRUPTS();
            queue->count--;
            status = queue->count;
            BSP_ENABLE_INTERRUPTS();
            break;     
         }
         else
         {
            if(sleep)
            {
               //wait in low power mode, GIE == ENABLE_INTERRUPTS
               __bis_SR_register(LPM0_bits + GIE);
               __no_operation();      
            }
            else
            {
               BSP_ENABLE_INTERRUPTS();  
            }
            
         }
      }while(1);
   }
   return(status);
}
