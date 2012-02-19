#include <msp430x54x.h>
#include "profile_manager.h"
#include "rtc.h"
#include "../common/msg_common.h"

#define PERIOD_LEN (2)
#define FLASH_D_ADDR 0x1800

typedef struct{
   uint8_t state;
   uint8_t next_day;
   uint8_t num_periods;
   //float curr_temp;
   uint8_t* next_ptr;
}PROFILE_MANAGER_T;

static uint8_t profile[32];
static PROFILE_MANAGER_T manager = {0x00, 0xFF, 0xFF, 0x00};

uint8_t* default_profile()
{
   profile[1] = 0; //profile id
   profile[2] = 6; //sat
   profile[3] = 2; //num on-off time period
   //on time
   profile[4] = 0; //minutes
   profile[5] = 19;  //hours
   //off time
   profile[6] = 15; //minutes
   profile[7] = 19;  //hours
   //on time
   profile[8] = 20; //minutes
   profile[9] = 19;  //hours
   //off time
   profile[10] = 30; //minutes
   profile[11] = 19;  //hours
   
   //fill in len
   profile[0] = 2 + (profile[2]*4);  
   
   return(profile);
}
int16_t update_profile(uint8_t* profile)
{
   int16_t ret = 0;
   uint8_t i;
   uint8_t msg_len = profile[0];
   uint8_t* flash_ptr_d = (uint8_t*)FLASH_D_ADDR;
   //info sections are 128
   if((msg_len < 128) && (flash_ptr_d[0] == 0xFF))
   {
      __disable_interrupt();                    // 5xx Workaround: Disable global
                                                // interrupt while erasing. Re-Enable
                                                // GIE if needed
      FCTL3 = FWKEY;                            // Clear Lock bit
      FCTL1 = FWKEY+ERASE;                      // Set Erase bit
      *flash_ptr_d = 0;                         // Dummy write to erase Flash seg D
      FCTL1 = FWKEY+WRT;                        // Set WRT bit for write operation, byte write
      for(i = 0; i < msg_len; i++)
      {
         *flash_ptr_d++ = *profile++;
      }
      //mark the end of profile
      *flash_ptr_d++ = 0xFF;    
      FCTL1 = FWKEY;                            // Clear WRT bit
      FCTL3 = FWKEY+LOCK;                       // Set LOCK bit
      
      //mark day as invalid, for new profile
      manager.next_day = 0xFF;
      __enable_interrupt();
      ret = i;
   }
   //now set alarm, turn on iTRV if required  
   return(ret);
}

int16_t handle_profile_alarm(uint16_t event)
{
   int16_t err = -1;
   uint8_t* flash_ptr_d;
   ITRV_EVENT_MSG_T alarm_msg = {ITRV_RTC_ALARM, 0};
   
   int16_t next_hour_min;
   //we assume safe because this is called after an alarm event
   uint8_t day_of_week = RTCDOW;
   int16_t curr_hour_min = (RTCHOUR<<8) | RTCMIN;
   
   //we have received a new profile
   if(manager.next_day == 0xFF)
   {   
      flash_ptr_d = (uint8_t*)FLASH_D_ADDR;
      //skip len, profile number
      flash_ptr_d += 2;
      next_hour_min = 0xFF;
      //store first period
      manager.next_ptr = flash_ptr_d;
      while(err == -1)
      {
         manager.next_day = *flash_ptr_d;
         flash_ptr_d++;
         if(manager.next_day > 6) //we mark end of profile with 0xff, try loop back to start
         {
            flash_ptr_d = (uint8_t*)(FLASH_D_ADDR + 2); //skip len, profile number
            manager.next_day = *flash_ptr_d;
         }
         
         if(manager.next_day >= day_of_week)
         {
            manager.num_periods = *flash_ptr_d;
            flash_ptr_d++;
            //search for time
            while(manager.num_periods)
            {
               uint8_t* alarm_time = (uint8_t*)&next_hour_min;
               //min
               alarm_time[0] = *flash_ptr_d++;
               //hour
               alarm_time[1] = *flash_ptr_d++; 
               //give a 2 minute buffer avoiding 59 sec case 
               if((next_hour_min - curr_hour_min) > 2)  
               {
                  //store direct ptr
                  manager.next_ptr = flash_ptr_d;
                  manager.num_periods--;
                  if((alarm_time[0] < 60) && (alarm_time[1] < 24) && (manager.next_day < 7))
                  {
                     rtc_set_alarm(manager.next_day, alarm_time[1], alarm_time[0], alarm_msg);
                     err = 0;
                  }
                  else
                  {
                     err = -2;   
                  }
                  break;
                  //manager.curr_temp = *((float*)&flash_ptr_d[2]);     
               }        
               flash_ptr_d += PERIOD_LEN;      
               manager.num_periods--;
            }      
         }
         else
         {
            //skip to next DAY
            flash_ptr_d += (manager.num_periods*PERIOD_LEN);   
         }            
      }
      
      //we failed to find timeslot for current day -> day 6, then store first day
      if(err != 0)
      {
         flash_ptr_d = (uint8_t*)(FLASH_D_ADDR + 2); //skip len, profile number
         manager.next_day = *flash_ptr_d++;
         manager.num_periods = *flash_ptr_d++;
         //first time value for when we wake next
         manager.next_ptr = flash_ptr_d;
         rtc_set_alarm( (RTC_AE | manager.next_day) , 0, 0, alarm_msg);           
      }
   }
   else if(event == ITRV_RTC_ALARM)
   {
      uint8_t next_alarm_min;
      uint8_t next_alarm_hour;
      
      if(manager.num_periods)
      {
         //assert(manager.next_day  == day_of_week )
         //get the off time
         next_alarm_min = *manager.next_ptr++; 
         next_alarm_hour = *manager.next_ptr++;
         if(manager.state > 0)
         {
            manager.num_periods--;
            manager.state = 0;
            P1OUT |= (BIT7);
         }
         else
         {
            //turn on
            manager.state = 1;
            P1OUT &= ~(BIT7);
         }
      }
      else
      {
         //new day
         if(*manager.next_ptr > 6 )//we mark end of profile with 0xff
         {
            //loop back to start
            manager.next_ptr = (uint8_t*)(FLASH_D_ADDR+2);  //skip len and profile id
         }
         manager.next_day = *manager.next_ptr++;
         manager.num_periods = *manager.next_ptr++;
         next_alarm_min = *manager.next_ptr++; 
         next_alarm_hour = *manager.next_ptr++;
         manager.state = 0;
      }
      if((next_alarm_min < 60) && (next_alarm_hour < 24) && (manager.next_day < 7))
      {
         rtc_set_alarm(manager.next_day, next_alarm_hour, next_alarm_min, alarm_msg);
         err = 0;
      }
      else
      {
         err = -2;
      }
         
   }
   else
   {
      P1OUT ^= (BIT6);   
   }
   return(err);
}
