#ifndef BUTTONS_H_
#define BUTTONS_H_

typedef void (*BUTTON_CALLBACK_T)(uint8_t out_buttons);

void buttons_init(BUTTON_CALLBACK_T func, uint8_t port1_buttons);

#endif /*BUTTONS_H_*/
