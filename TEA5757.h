#ifndef TEA5757_h
#define TEA5757_h

#include <inttypes.h>
#include <Arduino.h>

// Radio bands
#define TEA5757_BAND_FM 1
#define TEA5757_BAND_AM 2

// Search directions
#define TEA5757_SEARCH_NONE 0
#define TEA5757_SEARCH_UP   1
#define TEA5757_SEARCH_DOWN 2

// Signal detection level for search operations
#define TEA5757_LVL_1 1
#define TEA5757_LVL_2 2
#define TEA5757_LVL_3 3
#define TEA5757_LVL_4 4


class TEA5757 {
	public:
		TEA5757(uint8_t, uint8_t, uint8_t, uint8_t);
		void init();
		void clock();
		void preset(uint16_t, uint8_t);
		void search(uint8_t, uint8_t, uint8_t);
		uint16_t getPLLFrequency();
		uint8_t isStereo();

	private:
		void write(uint32_t);
		uint32_t read();
		uint8_t _mo_st_pin;
		uint8_t _clock_pin;
		uint8_t _data_pin;
		uint8_t _wr_en_pin;
};

#endif

