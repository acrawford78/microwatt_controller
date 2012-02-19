#ifndef PROFILE_MANAGER_H_
#define PROFILE_MANAGER_H_
#include <stdint.h>

uint8_t* default_profile();
int16_t update_profile(uint8_t* profile);
int16_t handle_profile_alarm(uint16_t event);

#endif /*PROFILE_MANAGER_H_*/
