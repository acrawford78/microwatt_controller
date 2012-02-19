#include <msp430x54x.h>
#include "uart.h"
#include "bsp.h"

#define NUM_PORTS    2

static uint8_t* uart_buf;
static volatile uint16_t uart_num_bytes = 0;

static UART_CALLBACK_T rx_callback[PORT_MAX];

void uart_init(uint16_t prescale)
{
   P5SEL = (BIT6 | BIT7);                    //Enable output pins after reset to avoid spurious transmit
   UCA1CTL1 |= UCSWRST;                      // **Put state machine in reset** 
   UCA1CTL1 |= UCSSEL_2;                     // SMCLK
   UCA1BR0 = (uint8_t)prescale;               // set prescaler
   UCA1BR1 = (uint8_t)(prescale>>8);
   UCA1MCTL |= UCBRS_6 + UCBRF_0;            // Modulation
   UCA1CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
}

int16_t uart_write(uint8_t* data, uint8_t port, uint16_t num_bytes)
{
   if(num_bytes && data)
   {
      uart_buf = data;
      uart_num_bytes = num_bytes;
      UCA1TXBUF = *uart_buf;                    //start the transmission
      UCA1IE |= UCTXIE;                         // Enable USCI_A1 TX interrupt
      //wait in low power mode waiting for TX to complete
      do{
         __bis_SR_register(LPM0_bits + GIE);     // Enter LPM0, enable interrupts
         __no_operation();                       // For debugger   
      }while(uart_num_bytes);
   }
   return(num_bytes-uart_num_bytes);
}

int16_t uart_set_recv_cb(UART_PORT_T port, UART_CALLBACK_T cb)
{
   int16_t err = -1;
   if((port < NUM_PORTS) && cb)
   {
      rx_callback[port] = cb;
      UCA1IE |= UCRXIE;
      err = 0;
   }
   return(err);
}

// Echo back RXed character, confirm TX buffer is ready first
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
  switch(__even_in_range(UCA1IV,4))
  {
  case 0:break;                             // Vector 0 - no interrupt
  case 2:                                   // Vector 2 - RXIFG
    rx_callback[PORT_A](UCA1RXBUF);  
    break;
  case 4:                                   // Vector 4 - TXIFG
    uart_num_bytes--;
    if(uart_num_bytes)
    {
      uart_buf++;
      UCA1TXBUF = *uart_buf;
    }
    else
    {
       UCA1IE &= ~UCTXIE;                      //turn off TX interrupt
       __bic_SR_register_on_exit(LPM0_bits);   // Exit to active CPU
    }
    break;                                  
  default: break;
  }
}
