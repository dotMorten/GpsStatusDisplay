
#ifndef __buttons_h__
#define __buttons_h__

// Defines which pins the buttons are connected to
#define PIN_BUTTON_UP 11
#define PIN_BUTTON_SELECT 10
#define PIN_BUTTON_LEFT 9
#define PIN_BUTTON_DOWN 6
#define PIN_BUTTON_RIGHT 5

// Values returned by getButtonState()
#define KEY_NONE 0
#define KEY_LEFT 1
#define KEY_RIGHT 2
#define KEY_UP 3
#define KEY_DOWN 4
#define KEY_SELECT 5

void initButtons();
int getButtonState();

#endif
