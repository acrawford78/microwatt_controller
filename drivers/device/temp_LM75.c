#include <msp430x54x.h>
#include "../chip/i2c_driver.h"
#include "temp_LM75.h"

#define LM75_RES  11

//------------------------------------------------------------------------------
// Places the LM75 into shutdwon mode uses typically 4uA
// 
// The LM75 will only unpdate temperature approx every 1.5 seconds.
// If shutdown is to be used this should be call ed approx 1.5 seconds before reading value.
// Otherwise you will read the last value before device was shutdown.
// IN:   uint8_t shutdown => 0x01 (shutdown) 0x00 (wake from shutdown)
// OUT:  < 1 fail 
//-----------------------------------------------------------------------------
int16_t temp_LM75_shutdown(uint8_t shutdown, uint8_t saddr)
{
   //shutdown the LM75 uses 4uA vs 250uA not shutdown
   uint8_t reg[2];
   reg[0] = LM75_CONFIG_REG;
   reg[1] = shutdown;
   if(i2c_write(reg, saddr, sizeof(reg)) == sizeof(reg))
   {
      //now reset pointer temperature register   
      reg[0] = LM75_TEMP_REG; 
      return(i2c_write(reg, saddr, 1));
   }
   return(-1);
}

//------------------------------------------------------------------------------
// Read the temperature from LM75. 
// IN: valid ptr to float or NULL if no floating point calc required, saddr 0b1001XXX XXX user defined on board
// OUT: 9-bit twos complement representation of temperature, 0.5 degree per bit
//      0xffff (-1) error
//-----------------------------------------------------------------------------
int16_t temp_LM75_read(float* temp_deg, uint8_t saddr)
{
   int16_t bytes_recv = 0;
   int16_t temp = 0xffff;
   //bytes_recv = i2c_read_polling(2, (uint8_t*)&temp);
   bytes_recv = i2c_read((uint8_t*)&temp, saddr, 2);
   if(bytes_recv == 2)
   {
      int16_t lsb = temp & (1<<15);
      temp <<= 1;
      if(lsb)
         temp |= 0x1;
      //possible D6-D0 from LM75 could be undefined to clear to be sure
      temp &= 0x1FF; 
      //calculate floating point if required
      if(temp_deg)
      {
         int16_t temp_16 = temp;
         //sign extend to 16bit number if negative
         if(temp & 0x100)
            temp_16 |= 0xFE;
         *temp_deg = (float)temp_16/2.0;
      }    
   }
   return(temp);
}

int16_t temp_LM75_read_int(int16_t* temp, uint8_t saddr)
{
   uint8_t bytes[2];
   int16_t bytes_recv = 0;
   bytes_recv = i2c_read(bytes, saddr, 2);
   if(bytes_recv == 2)
   {
      int16_t temp_int = ((uint16_t)bytes[0])<<8 | bytes[1];
      *temp = (temp_int>>(16 - LM75_RES));
   }
   return(bytes_recv);
}


