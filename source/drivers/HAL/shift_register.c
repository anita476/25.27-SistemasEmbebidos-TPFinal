#include "include/shift_register.h"
#include "../MCAL/include/gpio.h"
#include "../MCAL/include/pisr.h"
#include "include/board.h"

#define OUT_NUM 16
#define SR_PISR_PERIOD 1 // in systicks
/* Word layout
 *
 * 15------------8------------4-----2-----0
 * | segment_bits| -EMPTY-    | leds| disp|
 * ----------------------------------------
 *
 */
#define DIGIT_MASK 0x0003 // bits 1..0
#define LED_MASK 0x000C	  // bits 3..2
#define SEG_MASK 0xFF00	  // bits 15..8

#define DIGIT_SHIFT 0
#define LED_SHIFT 2
#define SEG_SHIFT 8

static uint16_t current_output; /* last value shifted out        */
static uint16_t pending_output; /* value queued */
static bool dirty;				/* true = pending != current     */
static bool initialized;

void shift_register_drv_PISR();

static void _shift_data(uint16_t data) {
	gpio_drv_write(SR_LATCH, LOW);

	/* MSB first */
	for (int i = (OUT_NUM - 1); i >= 0; i--) {
		gpio_drv_write(SR_DATA, (data >> i) & 0x01);
		gpio_drv_write(SR_SCLK, HIGH);
		gpio_drv_write(SR_SCLK, LOW);
	}

	/* Push*/
	gpio_drv_write(SR_LATCH, HIGH);
	gpio_drv_write(SR_LATCH, LOW);
}

void shift_register_drv_init() {
	if (initialized)
		return;
	gpio_drv_mode(SR_DATA, OUTPUT);
	gpio_drv_mode(SR_LATCH, OUTPUT);
	gpio_drv_mode(SR_SCLK, OUTPUT);
	gpio_drv_mode(SR_DEC_OE, OUTPUT);
	gpio_drv_mode(SR_SEG_OE, OUTPUT);

	gpio_drv_write(SR_DATA, LOW);
	gpio_drv_write(SR_LATCH, LOW);
	gpio_drv_write(SR_SCLK, LOW);
	gpio_drv_write(SR_DEC_OE, LOW);
	gpio_drv_write(SR_SEG_OE, LOW);

	pisr_drv_register(shift_register_drv_PISR, SR_PISR_PERIOD);
	initialized = true;
}

void shift_register_drv_set_segments(uint8_t word) {
	if (!initialized)
		return;

	pending_output &= ~SEG_MASK;
	pending_output |= ((uint16_t) word << SEG_SHIFT);

	dirty = (pending_output != current_output);
}

void shift_register_drv_sel_digit(DIS_DIG_SELECTION sel) {
	if (!initialized)
		return;

	pending_output &= ~DIGIT_MASK;
	pending_output |= ((uint16_t) sel & 0x03) << DIGIT_SHIFT;

	dirty = (pending_output != current_output);
}

void shift_register_drv_sel_led(LED_SELECTION led) {
	if (!initialized)
		return;

	pending_output &= ~LED_MASK;
	pending_output |= ((uint16_t) led & 0x03) << LED_SHIFT;

	dirty = (pending_output != current_output);
}

void shift_register_drv_PISR() {
	if (!initialized || !dirty)
		return;
	_shift_data(pending_output);
	current_output = pending_output;
	dirty = false;
}

void shift_register_drv_set_digit_segments(uint8_t word, DIS_DIG_SELECTION sel) {
	if (!initialized)
		return;
	pending_output &= ~(SEG_MASK | DIGIT_MASK);
	pending_output |= ((uint16_t) word << SEG_SHIFT);
	pending_output |= ((uint16_t) (sel & 0x03) << DIGIT_SHIFT);
	dirty = (pending_output != current_output);
}

void shift_register_drv_seg_enable(bool on) {
	gpio_drv_write(SR_SEG_OE, on ? LOW : HIGH); // OE is active-low
}