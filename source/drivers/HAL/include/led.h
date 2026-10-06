#ifndef _LED_H_
#define _LED_H_
#include <stdbool.h>

/**
 * @brief Initialize the led driver. Internally uses a timer and no multiplexing so only one led can be blinking/on at a
 * time
 */
bool led_drv_init(void);

/**
 * @brief Turn on the success status indicator led
 */
void led_drv_on_success(void);

/**
 * @brief Turn off the success status indicator led
 */
void led_drv_off_success(void);

/**
 * @brief Start blinking the failure status indicator led at a steady rate
 */
void led_drv_blink_failure(void);

/**
 * @brieg Stop blinking the failure status indicator led at a steady rate
 */
void led_drv_stop_blink_failure(void);

#endif /*_LED_H_*/