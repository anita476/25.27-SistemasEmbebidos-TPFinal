#include "include/display.h"
#include "../MCAL/include/gpio.h"
#include "../MCAL/include/pisr.h"
#include "include/board.h"
#include "include/shift_register.h"

#define PISR_TICKS 1	// 1 ms bc thats the smallest int possible
#define DISPLAY_TICKS 5 // actual display logic will be every 5ms

#define SCROLL_TICKS 30

// padding is so that we start blank, then the scroll, and then theres 4 blank digits until restarting the scroll, it
// looks better
static uint8_t word_buf[DIG_NUM + MAX_WORD_LEN + DIG_NUM];
static volatile uint8_t display_counter;
static uint8_t word_len;
static uint8_t scroll_offset; // current start index
static uint8_t scroll_total;
static uint8_t scroll_tick; // counts PISR calls between scroll steps, max is SCROLL_TICKS
static bool scrolling;

/****** for intensity  */
static volatile uint8_t intensity;
static volatile uint8_t curr_int_counter;
static volatile uint8_t pwm_counter;

static uint8_t visible[DIG_NUM]; //  bytes currently on screen -> pisr multiplexes over these
static uint8_t curr;

void display_drv_PISR();
static void rebuild_visible();

bool display_drv_init() {
	curr = 0;
	scroll_offset = 0;
	scroll_tick = 0;
	scrolling = false;
	intensity = MAX_INTENSITY;
	word_len = 0;

	for (int i = 0; i < DIG_NUM; i++)
		visible[i] = 0x00;

	pisr_drv_register(display_drv_PISR, PISR_TICKS);
	shift_register_drv_init();
	return true;
}

void display_drv_write_word(uint8_t *new_word, uint8_t size) {
	if (size > MAX_WORD_LEN)
		size = MAX_WORD_LEN;
	word_len = size;

	// build the buffer
	for (int i = 0; i < DIG_NUM; i++)
		word_buf[i] = 0x00;
	for (int i = 0; i < size; i++)
		word_buf[DIG_NUM + i] = new_word[i];
	for (int i = 0; i < DIG_NUM; i++)
		word_buf[DIG_NUM + size + i] = 0x00;

	scroll_offset = 0;
	scroll_tick = 0;

	if (size > DIG_NUM) {
		scroll_total = DIG_NUM + size; // number of steps until word fully loads
		scrolling = true;
	} else {
		scroll_offset = DIG_NUM;
		scrolling = false;
		scroll_total = 0;
	}

	rebuild_visible();
}

static void rebuild_visible() {
	for (int i = 0; i < DIG_NUM; i++)
		visible[i] = word_buf[scroll_offset + i];
}

void display_drv_write_to_digit(uint8_t dig, uint8_t code) {
	if (dig >= DIG_NUM)
		return;
	shift_register_drv_set_digit_segments(code, dig);
}

void display_drv_PISR() {
	pwm_counter++;
	if (pwm_counter >= DISPLAY_TICKS)
		pwm_counter = 0;

	if (pwm_counter == 0) {
		shift_register_drv_seg_enable(true); //  segments on
	} else if (pwm_counter == intensity && intensity < DISPLAY_TICKS) {
		shift_register_drv_seg_enable(false);
	}

	display_counter++;
	if (display_counter < DISPLAY_TICKS)
		return;
	display_counter = 0;

	curr = (curr + 1u) % DIG_NUM;
	display_drv_write_to_digit(curr, visible[curr]);

	if (scrolling) { // every 5ms we scroll (display ticks)
		scroll_tick++;
		if (scroll_tick >= SCROLL_TICKS) {
			scroll_tick = 0;
			scroll_offset++;
			if (scroll_offset > scroll_total)
				scroll_offset = 0;
			rebuild_visible();
		}
	}
}
// @todo ask if irq disabling is fine within an irq
void display_drv_set_intensity(uint8_t intn) {
	if ((MIN_INTENSITY <= intn) && (intn <= MAX_INTENSITY)) {
		intensity = intn;
		pwm_counter = 0; // restart so it doesnt look too janky, it still doesnt fix all issues
	}
}