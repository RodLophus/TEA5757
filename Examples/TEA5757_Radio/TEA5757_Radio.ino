/*

Simple AM/FM radio with Philips TEA5757 Radio-on-chip IC

This sketch should also work with most TEA5757-based audio systems 
tuner modules (found in old Philips sound systems).

TEA5757 to Arduino connections:
Arduino              TEA5757
A5                  MO/ST (24)
A4                  Clock (27)
A3                  Data (28)
A2                  WR_EN (29)
GND                 GND

Note:   This example uses an analog keypad with the following schematics:

                 A0
                 |
          2k2    |   330R        620R         1k          3k3        
VCC -----\/\/\---+---\/\/\---+---\/\/\---+---\/\/\---+---\/\/\-----+----- GND
                 |           |           |           |             |
                 X SCAN_UP   X UP        X DOWN      X SCAN_DOWN   X BAND
                 |           |           |           |             |
                GND         GND         GND         GND           GND

X = keys (N.O.)

*/

#include <inttypes.h>
#include <LiquidCrystal.h>
#include <TEA5757.h>

// This example uses an analog 5-key keypad tied to A0
#define KEYPAD_PIN A0

// Keypad keys
#define KEY_BAND      5
#define KEY_SCAN_DOWN 4
#define KEY_DOWN      3
#define KEY_UP        2
#define KEY_SCAN_UP   1
#define KEY_NONE      0

// Initial frequencies
uint16_t FMFrequency = 885;  // kHz / 100
uint16_t AMFrequency = 53;   // kHZ / 10
uint16_t frequency = FMFrequency;

// Initial band
uint8_t band = TEA5757_BAND_FM;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

TEA5757 Radio(A5, A4, A3, A2);  // MO/ST, Clock, Data, WR_EN


/*******************************************************\
 *                       getKey()                      *
 * Read the (analog) keypad.                           *
 * If you are using an digital (one key per input pin) *
 * keypad, just this function to return the correct    *
 * values                                              *
\*******************************************************/
uint8_t getKey(uint8_t keypadPin) {
  uint16_t keypad;
  uint8_t key = KEY_NONE;
  
  keypad = analogRead(keypadPin);
  
  if(keypad < 870) key = KEY_BAND;  
  if(keypad < 600) key = KEY_SCAN_DOWN;  
  if(keypad < 390) key = KEY_DOWN;  
  if(keypad < 220) key = KEY_UP;  
  if(keypad < 60)  key = KEY_SCAN_UP;
  
  return key;
}



/*******************\
 * Arduino setup() *
\*******************/
void setup() {
  lcd.begin(16, 2);

  Radio.init();

  Radio.preset(frequency, band);
}


/******************\
 * Arduino loop() *
\******************/
void loop() {
  uint16_t previousFrequency;
  uint8_t key = getKey(KEYPAD_PIN);
  uint8_t searchDirection = 0;
  int8_t delta = 0;
  uint8_t i;
  uint8_t n = 0;

  lcd.setCursor(0, 0);
  
  // Frequency
  switch(band) {
    case TEA5757_BAND_FM:
      lcd.print("  FM ");
      if(frequency < 999) lcd.print(' ');
      lcd.print(frequency / 10);
      lcd.print('.');
      lcd.print(frequency % 10);
      lcd.print(" MHz  ");
      break;
    case TEA5757_BAND_AM:
      lcd.print("  AM ");
      if(frequency < 99) lcd.print(' ');
      lcd.print(frequency * 10);
      lcd.print(" kHz   ");
      break;
  }
  
  // Status (Tuned / Stereo)
  lcd.setCursor(0, 1);

  if(Radio.isTuned())
    lcd.print("[TUNED]");
  else
    lcd.print("[     ]");

  if(Radio.isStereo())
    lcd.print(" [STEREO]");
  else
    lcd.print(" [      ]");

  if(key != KEY_NONE) { // Some key has been pressed
    delay(200);
    switch(key) {
      case KEY_UP:        searchDirection = TEA5757_SEARCH_NONE; delta = +1; break;
      case KEY_DOWN:      searchDirection = TEA5757_SEARCH_NONE; delta = -1; break;
      case KEY_SCAN_UP:   searchDirection = TEA5757_SEARCH_UP;   delta = +1; break;
      case KEY_SCAN_DOWN: searchDirection = TEA5757_SEARCH_DOWN; delta = -1; break;
      case KEY_BAND:
        switch(band) {
          case TEA5757_BAND_FM:
            FMFrequency = frequency;
            frequency = AMFrequency;
            band = TEA5757_BAND_AM;
            break;
          case TEA5757_BAND_AM:
            AMFrequency = frequency;
            frequency = FMFrequency;
            band = TEA5757_BAND_FM;
            break;
        }
        Radio.preset(frequency, band);
        return;
    }
    
    if(searchDirection == TEA5757_SEARCH_NONE) { // No search mode = manual tuning

      frequency += delta;
      switch(band) {
        case TEA5757_BAND_FM:
          if(frequency > 1080) frequency = 885;
          if(frequency < 885)  frequency = 1080;
          break;
        case TEA5757_BAND_AM:
          if(frequency > 170) frequency = 53;
          if(frequency < 53)  frequency = 170;
          break;
      }  
      Radio.preset(frequency, band);

    } else { // Automatic tuning (search)

      previousFrequency = frequency;
      // Keep reading the TEA5757 registry until get a non-zero frequency.
      // According to the datasheet, the registry´s MSB should be "0" when the
      // TEA5757 get tuned, but apparently it is always "1"
      search:
      Radio.search(band, TEA5757_LVL_4, searchDirection);
      for(i = 0; i < 50; i++) {
        delay(100);
        frequency = Radio.getPLLFrequency();
        if(frequency) { // Tuned!
          i = 50; // exit the loop
          switch(band) {
            // Convert PLL frequency to "actual" frequency
            case TEA5757_BAND_FM: frequency = (frequency / 8) - 107; break;
            case TEA5757_BAND_AM: frequency = (frequency / 10) - 45; break;
          }
        }
      }
      
      if((frequency == previousFrequency) || ((frequency == 0) && ++n)) {
        if(n > 3) {
          // No station found (antenna problem?): quit searching
          frequency = previousFrequency;
          return;
        }
        // Issue a pre-step and try again
        Radio.preset(frequency + (5 * delta), TEA5757_BAND_FM);
        delay(100);
        goto search;
      }

      switch(band) {
        case TEA5757_BAND_FM:
          if((frequency > 1080) || (frequency < 885)) {
            // Got out of bounds or found no station: go to the other end of the scale
             if(searchDirection == TEA5757_SEARCH_UP)
                Radio.preset(885, TEA5757_BAND_FM);
              else
                Radio.preset(1080, TEA5757_BAND_FM);
              delay(100);
              goto search;
          }
          break;
        case TEA5757_BAND_AM:
          if((frequency > 170) || (frequency < 53)) {
            if(searchDirection == TEA5757_SEARCH_UP)
              Radio.preset(53, TEA5757_BAND_AM);
            else
              Radio.preset(170, TEA5757_BAND_AM);
            delay(100);
            goto search;
          }
          break;
      } // switch(band)

    } // else (automatic tunning)
  } // if (key pressed)
}



