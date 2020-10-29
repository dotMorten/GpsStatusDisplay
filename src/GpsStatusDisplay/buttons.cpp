#include "buttons.h"
#include <Arduino.h>

void initButtons()
{
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_SELECT, INPUT_PULLUP);
};

int getButtonState()
{
  if ( digitalRead(PIN_BUTTON_LEFT) == LOW )
    return KEY_LEFT;
  else if ( digitalRead(PIN_BUTTON_RIGHT) == LOW )
    return KEY_RIGHT;
  else if ( digitalRead(PIN_BUTTON_UP) == LOW )
    return KEY_UP;
  else if ( digitalRead(PIN_BUTTON_DOWN) == LOW )
    return KEY_DOWN;
  else if ( digitalRead(PIN_BUTTON_SELECT) == LOW)
    return KEY_SELECT;
  else
    return KEY_NONE;
};
