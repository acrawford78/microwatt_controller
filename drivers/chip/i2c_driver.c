//#include <msp430x54x.h>
#include "i2c_driver.h"
#include "bsp.h"

static uint8_t* i2c_buf;
static volatile int16_t i2c_num_bytes = 0;
static volatile int16_t nack_count = 0;

void i2c_init(uint16_t prescale)
{
   I2C_PORT |= SDA_PIN + SCL_PIN;                 // Assign I2C pins to USCI
   UCBCTL1 = UCSWRST;                        // Enable SW reset
   UCBCTL0 = UCMST + UCMODE_3 + UCSYNC;       // I2C Master, synchronous mode
   UCBCTL1 = UCSSEL_2 + UCSWRST;              // Use SMCLK, keep SW reset
   UCBBR0 = (uint8_t)prescale;                         // set prescaler
   UCBBR1 = (uint8_t)(prescale>>8);
   UCBCTL1 &= ~UCSWRST;                       // Clear SW reset, resume operation

}

int16_t i2c_read(uint8_t* data, uint8_t saddr, int16_t num_bytes)
{
   int16_t bytes_recv = 0;
   if(num_bytes && data)
   {
      nack_count = 0;
      i2c_buf = data;
      i2c_num_bytes = num_bytes;
      UCBI2CSA = saddr;                  // set slave address
      UCBIE = (UCRXIE | UCNACKIE);         // Enable RX + NACK interrupt
      UCBCTL1 &= ~UCTR;                    //Clear TX flag
      UCBCTL1 |= UCTXSTT;                      
      if(num_bytes == 1)
         UCBCTL1 |= UCTXSTP;   
      //wait in low power mode waiting for all bytes to be received or a NACK
      while(1)
      {
         BSP_DISABLE_INTERRUPTS();
         if((i2c_num_bytes == 0) || nack_count)
         {
            //exit
            BSP_ENABLE_INTERRUPTS();
            break;
             
         }
         //sleep with interrupts enabled
         __bis_SR_register(LPM0_bits + GIE);
         __no_operation();
      }
      //wait for stop to be sent
      while(UCBCTL1 & UCTXSTP);
      if(nack_count)
         bytes_recv = -1;
      else
         bytes_recv = num_bytes - i2c_num_bytes;
   }
   return(bytes_recv);
}
   
int16_t i2c_write(uint8_t* data, uint8_t saddr, int16_t num_bytes)
{
   int16_t bytes_sent = 0;
   if(num_bytes && data)
   {
      nack_count = 0;
      i2c_buf = data;
      i2c_num_bytes = num_bytes;
      UCBI2CSA = saddr;                  // set slave address
      UCBIE = (UCTXIE | UCNACKIE);         // Enable TX + NACK interrupt
      UCBCTL1 |= UCTR + UCTXSTT;             // I2C TX, start condition
      //wait in low power mode waiting for TX to complete
      while(1)
      {
         BSP_DISABLE_INTERRUPTS();
         if((i2c_num_bytes == 0) || nack_count)
         {
            //exit
            BSP_ENABLE_INTERRUPTS();
            break;
             
         }
         //sleep with interrupts enabled
         __bis_SR_register(LPM0_bits + GIE);
         __no_operation();
      }
      //wait for stop to be sent
      while(UCBCTL1 & UCTXSTP);
      if(nack_count)
         bytes_sent = -1;
      else
         bytes_sent = num_bytes - i2c_num_bytes;
   }
   return(bytes_sent);
}

// USCI Data ISR
#pragma vector = USCI_VECTOR
__interrupt void USCI_ISR(void)
{
   switch(__even_in_range(UCBIV,12))
   {
   case  0: break;                           // Vector  0: No interrupts
   case  2: break;                           // Vector  2: ALIFG
   case  4:                                   // Vector  4: NACKIFG
      nack_count++;
      UCBCTL1 |= UCTXSTP;
      UCBIFG &= ~UCNACKIFG;
      __bic_SR_register_on_exit(LPM0_bits);    // Exit to active CPU
   break;                           
   case  6: break;                           // Vector  6: STTIFG
   case  8: break;                           // Vector  8: STPIFG
   case 10:                                  // Vector 10: RXIFG
      *i2c_buf++ = UCBRXBUF;
      i2c_num_bytes--;
      if(i2c_num_bytes == 1)
         UCBCTL1 |= UCTXSTP;
      else if(i2c_num_bytes == 0)
         __bic_SR_register_on_exit(LPM0_bits);   // Exit to active CPU
   break;
   case 12:                           // Vector 12: TXIFG
      if(i2c_num_bytes) {
         UCBTXBUF = *i2c_buf++;
         i2c_num_bytes--;
      }
      else {
         //send stop and clr flag as we are now finished
         UCBCTL1 |= UCTXSTP;
         UCBIFG &= ~UCTXIFG;
         //STP is set so now wake up CPU
         __bic_SR_register_on_exit(LPM0_bits);   // Exit to active CPU
      }
      break;                     
   default: break;
   }
}
