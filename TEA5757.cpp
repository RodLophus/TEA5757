/*

This library implements the Philips TEA5757 Radio-on-chip communication protocol

Notes: 
 1. This is NOT compatible with TEA5767!
 2. Check TEA5757_BASE_CONFIG below to set I/O pins (P0 and P1) to suitable values
    (on Philips' modules, wrong values will prevent the MONO/STEREO indicator 
	from working properly)
 3. This works for 0.1MHz steps for FM and 10kHz steps for AM

               ------------------------------------

Copyright (c) 2013 Rodolfo Broco Manin.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include <inttypes.h>
#include <TEA5757.h>

// Default delay (us)
// This is also the base timing for clock pulses
#define TEA5757_DELAY 5

// All data sent to TEA5757 will be OR'ed with this value.
// Use this, for example, to set the outputs of I/O ports P0 and P1.  
// On Philips radio modules, the wrong setting may prevent the 
// stereo indicator from working properly.
// - Philips DVD receiver modules usually requires:
#define TEA5757_BASE_CONFIG 0b0000011000000000000000000
// - For Philips ECO6 and other stereo modules, try:
//#define TEA5757_BASE_CONFIG 0b0000000000000000000000000;



/******************************\
 *        Constructor         *
 *  Just set class variables  *
\******************************/
TEA5757::TEA5757(uint8_t mo_st_pin, uint8_t clock_pin, uint8_t data_pin, uint8_t wr_en_pin) {
	_mo_st_pin = mo_st_pin;
	_clock_pin = clock_pin;
	_data_pin  = data_pin;
	_wr_en_pin = wr_en_pin;
}


/******************************************\
 *                 init()                 *
 *  Set pin functions and initial states  *
\******************************************/
void TEA5757::init() {
  pinMode(_mo_st_pin, INPUT);
  pinMode(_clock_pin, OUTPUT);
  pinMode(_data_pin, INPUT);
  pinMode(_wr_en_pin, OUTPUT);
  
  // TEA5757 will be left with clock low and in read mode when idle
  digitalWrite(_wr_en_pin, LOW);
  digitalWrite(_clock_pin, LOW);
  
  digitalWrite(_data_pin, HIGH);
  
  // The MO_ST pin may require a stronger (external) pullup
  digitalWrite(_mo_st_pin, HIGH);
}


/************************\
 *       clock()        *
 * Toggle the clock pin *
\************************/
void TEA5757::clock() {
  digitalWrite(_clock_pin, HIGH);
  delayMicroseconds(TEA5757_DELAY);
  digitalWrite(_clock_pin, LOW);
}


/*************************************************\
 *                     write()                   *
 * Send (25 bits of) data to TEA5757 (MSB first) *
\*************************************************/
void TEA5757::write(uint32_t data) {
  digitalWrite(_wr_en_pin, HIGH);     // Enter write mode
  pinMode(_data_pin, OUTPUT);         // Data output

  for(uint8_t i = 0; i < 25; i++) {
    if(data & 0x01000000)
      digitalWrite(_data_pin, HIGH);
    else
      digitalWrite(_data_pin, LOW);
    clock();
    data = data << 1;
  }
  
  // Get back to read (idle) mode
  pinMode(_data_pin, INPUT);
  digitalWrite(_data_pin, HIGH);
  digitalWrite(_wr_en_pin, LOW);  
}


/****************************************************\
 *                     read()                       *
 * Reads (25 bits of) data from TEA5757 (MSB first) *
\****************************************************/
uint32_t TEA5757::read() {
  uint32_t data;
  // The TEA5757 is already in read mode (it is kept so while idle)
  
  // Toggle WR_EN to "reset" the radio shift register´s pointer
  digitalWrite(_wr_en_pin, HIGH);
  delayMicroseconds(TEA5757_DELAY);
  digitalWrite(_wr_en_pin, LOW);
  
  // 1st bit (MSB) is available as soon as WR_EN go low
  delayMicroseconds(TEA5757_DELAY);
  data = digitalRead(_data_pin);
  
  // Get the other (24) bits
  for(uint8_t i = 0; i < 24; i++) {
    data = data << 1;
    clock();
    if(digitalRead(_data_pin))
      data |= 0x1;
  }
  return data;
}


/*********************************************\
 *                  preset()                 *
 * Tune the specified band/frequency         *
 * Use:                                      *
 *  AM: frequency [kHz] / 10   (53 to 170)   *
 *  FM: frequency [kHz] / 100  (880 to 1080) *
\*********************************************/
void TEA5757::preset(uint16_t frequency, uint8_t band) {
  uint32_t data;
  switch(band) {
    case TEA5757_BAND_FM:
              //4321098765432109876543210
      data |= 0b0000000110000000000000000 | TEA5757_BASE_CONFIG;
      data += (frequency + 107) * 8; // (freq + FI) / 12,5
      break;
    case TEA5757_BAND_AM:
              //4321098765432109876543210
      data |= 0b0000100110000000000000000 | TEA5757_BASE_CONFIG;
      data += (frequency + 45) * 10 ; // freq + FI
      break;
    default:
      return;
  }
  write(data);
}


/****************************************************\
 *                     search()                     *
 * Go to the next or previous working radio station *
\****************************************************/
void TEA5757::search(uint8_t band, uint8_t level, uint8_t dir) {
                  //4321098765432109876543210
  uint32_t data = 0b1000000000000000000000000 | TEA5757_BASE_CONFIG; // Enter search mode

  if(band == TEA5757_BAND_AM)
            //4321098765432109876543210
    data |= 0b0000100000000000000000000; // AM mode
  
  // Select the signal level threshold
  switch(level) {
    case TEA5757_LVL_2:
	          //4321098765432109876543210
      data |= 0b0000000010000000000000000;
      break;
    case TEA5757_LVL_3:
	          //4321098765432109876543210
      data |= 0b0000000100000000000000000;
      break;
    case TEA5757_LVL_4:
	          //4321098765432109876543210
      data |= 0b0000000110000000000000000;
      break;
  }
  
  if(dir == TEA5757_SEARCH_UP)
            //4321098765432109876543210
    data |= 0b0100000000000000000000000;

  write(data);
}


/******************************************************\
 *                  getFrequency()                    *
 * Return the TEA5757 PLL frequency registry after a  *
 * search operation.                                  *
 *  AM: returns tuned_frequency + 450 [kHz]           *
 *  FM: returns (tuned_frequency * 12.5) + 10.7 [MHz] *
 * Returns 0 in preset mode or if no station is found *
\******************************************************/
uint16_t TEA5757::getPLLFrequency() {
	uint32_t data = read();
	data &= 0b0000000000111111111111111;
	return (uint16_t)data;
}

	
/********************************************************\
 *                     isStereo()                       *
 * Return 1 if the radio detects a stereo station pilot *
 * (works only on FM mode)                              *
\********************************************************/
uint8_t TEA5757::isStereo() {
  return digitalRead(_mo_st_pin) ? 0 : 1;
}
