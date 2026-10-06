#include "include/reader.h"
#include "../MCAL/include/gpio.h"
#include "include/board.h"
#include <string.h>

#define CARD_NUM_DIGITS 8

void card_Enable_ISR(void);
void card_Clock_ISR(void);

volatile uint8_t buffer[ID_LENGHT];
volatile int bit_count = 0;
volatile int char_count = 0;
volatile uint8_t temp_char = 0;
volatile bool reading_active = false;
volatile bool card_ready = false;

/********************** READER FUNCTIONS */
bool reader_drv_init(void) {
	// FIRST initializze gpios
	gpio_drv_mode(PIN_CARD_DATA, INPUT); // @todo change all to input pullup if all are high on idle
	gpio_drv_mode(PIN_CARD_CLOCK, INPUT);
	gpio_drv_mode(PIN_CARD_ENABLE, INPUT);

	bool int1 = gpio_drv_IRQ(PIN_CARD_ENABLE, GPIO_IRQ_MODE_FALLING_EDGE, card_Enable_ISR);
	// FALLING EDGE !
	bool int2 = gpio_drv_IRQ(PIN_CARD_CLOCK, GPIO_IRQ_MODE_FALLING_EDGE, card_Clock_ISR);

	if (int1 == false || int2 == false) {
		return false;
	}
	return true;
}
/*
void process_raw(void) {
	printf("char_count=%d\n", char_count);
	for (int i = 0; i < char_count; i++) {
		printf("[%d] raw=0x%02X dec=%d char=%c\n", i, buffer[i], buffer[i], buffer[i]);
	}
}
	*/
static void reader_drv_reset(void) {
	bit_count = 0;
	char_count = 0;
	temp_char = 0;
	reading_active = false;
	card_ready = false;
}

bool reader_drv_event(void) {
	return card_ready;
}

void reader_drv_card(uint8_t *out_buf, uint8_t *out_len) {
	if (!card_ready) {
		return;
	}
	// process_raw();
	*out_len = CARD_NUM_DIGITS; /* char count will be 19*/
	memcpy(out_buf, (void *) buffer, CARD_NUM_DIGITS);
	reader_drv_reset();
}

void card_Enable_ISR(void) {
	reader_drv_reset();
}

void card_Clock_ISR(void) {
	bool bit = !gpio_drv_read(PIN_CARD_DATA);

	if (!reading_active) {
		temp_char = (temp_char >> 1) | (bit << 4); // LSB first
		if (temp_char == CODED_B) {
			reading_active = true;
			bit_count = 0;
			temp_char = 0;
		}
	} else {
		temp_char |= (bit << bit_count); // LSB FIRST !!!
		bit_count++;
		if (bit_count == 5) {
			if (temp_char == CODED_D) { // it still keeps reading until FS, only that it doesnt put it in the buffer
				buffer[char_count] = '\0';
				card_ready = true;
				reading_active = false;
			} else if (char_count < 8) {
				buffer[char_count++] = '0' + (temp_char & 0x0F); // & 0f to get rid of parity bitt
			}
			bit_count = 0;
			temp_char = 0;
		}
	}
}