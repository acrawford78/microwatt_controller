#ifndef UART_H_
#define UART_H_
#include <stdint.h>

typedef int16_t (*UART_CALLBACK_T)(uint8_t byte);
typedef enum{
   PORT_A = 0,
   PORT_B,
   PORT_MAX
}UART_PORT_T;

void uart_init(uint16_t prescale);
int16_t uart_write(uint8_t* data, uint8_t port, uint16_t num_bytes);
int16_t uart_set_recv_cb(UART_PORT_T port, UART_CALLBACK_T cb);
#endif /*UART_H_*/
