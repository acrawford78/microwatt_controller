#ifndef USCI_LIB
#define USCI_LIB
#include <stdint.h>
//undef this to use UCB3
//#define UCB3
#define SDA_PIN 0x02                                  
#define SCL_PIN 0x04
void i2c_init(uint16_t prescale);
int16_t i2c_read(uint8_t* data, uint8_t saddr, int16_t num_bytes);
int16_t i2c_write(uint8_t* data, uint8_t saddr, int16_t num_bytes);
#ifdef UCB3
   #define I2C_PORT     P10SEL
   #define UCBCTL0      UCB3CTL0
   #define UCBCTL1      UCB3CTL1
   #define UCBBR0       UCB3BR0
   #define UCBBR1       UCB3BR1
   #define UCBI2CSA     UCB3I2CSA
   #define UCBCTL1      UCB3CTL1
   #define UCBI2CIE     UCB3I2CIE
   #define UCBSTAT      UCB3STAT
   //interrupts
   #define USCI_VECTOR  USCI_B3_VECTOR
   #define UCBIE        UCB3IE
   #define UCBIFG       UCB3IFG
   #define UCBIV        UCB3IV
   //data reg
   #define UCBRXBUF     UCB3RXBUF
   #define UCBTXBUF     UCB3TXBUF
#else
   #define I2C_PORT     P3SEL
   #define UCBCTL0      UCB0CTL0
   #define UCBCTL1      UCB0CTL1
   #define UCBBR0       UCB0BR0
   #define UCBBR1       UCB0BR1
   #define UCBI2CSA     UCB0I2CSA
   #define UCBCTL1      UCB0CTL1
   #define UCBI2CIE     UCB0I2CIE
   #define UCBIE        UCB0IE
   #define UCBSTAT      UCB0STAT
   //interrupts
   #define USCI_VECTOR  USCI_B0_VECTOR
   #define UCBIE        UCB0IE
   #define UCBIFG       UCB0IFG
   #define UCBIV        UCB0IV
   //data reg
   #define UCBRXBUF   UCB0RXBUF
   #define UCBTXBUF   UCB0TXBUF
#endif

#endif
