#include "include/led.h"
#include "include/shift_register.h"
#include "include/timer.h"
#include <stdint.h>

#define SUCCESS_LED LED_SEL_FIRST
#define FAILURE_LED LED_SEL_THIRD
#define LED_BLINK_PERIOD 500 /* in ticks */

volatile bool blink_on = false;
uint8_t led_timer;

static void toggle_led(void);

bool led_drv_init(void) {
	led_timer = timer_drv_get_id();
	if (led_timer == TIMER_INVALID_ID) {
		return false;
	}
	shift_register_drv_sel_led(LED_SEL_NONE);
	return true;
}

void led_drv_on_success(void) {
	shift_register_drv_sel_led(SUCCESS_LED);
}

void led_drv_off_success(void) {
	shift_register_drv_sel_led(LED_SEL_NONE);
}

void led_drv_blink_failure(void) {
	timer_drv_start(led_timer, LED_BLINK_PERIOD, TIM_MODE_PERIODIC, toggle_led);
}

void led_drv_stop_blink_failure(void) {
	timer_drv_stop(led_timer);
}

static void toggle_led(void) {
	if (blink_on) {
		shift_register_drv_sel_led(LED_SEL_NONE);
	} else {
		shift_register_drv_sel_led(FAILURE_LED);
	}
	blink_on = !blink_on;
}