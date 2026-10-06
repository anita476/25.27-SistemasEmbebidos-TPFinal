#ifndef _SHIFT_REGISTER_H_
#define _SHIFT_REGISTER_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// MC74HC139.h will be absorbed by this drv, since it has the mask

typedef enum { LED_SEL_NONE = 0, LED_SEL_FIRST = 1, LED_SEL_SECOND = 2, LED_SEL_THIRD = 3 } LED_SELECTION;

typedef enum { DIS_DIG_FIRST = 0, DIS_DIG_SECOND = 1, DIS_DIG_THIRD = 2, LED_DIG_FOURTH = 3 } DIS_DIG_SELECTION;

/**
 * @brief Initialize the shift register driver
 */
void shift_register_drv_init();

/**
 * @brief Sets the segment values for display via shift registers
 * @param word 8 bits to set sequentially (A to DP)
 */
void shift_register_drv_set_segments(uint8_t word);

/**
 * @brief Sets the digit selected for display
 * @param sel The digit to select, from left to right.
 *
 */
void shift_register_drv_sel_digit(DIS_DIG_SELECTION sel);

/**
 * @brief Sets the digit selected for status leds
 * @param led The led to select, from left to right, or none.
 */
void shift_register_drv_sel_led(LED_SELECTION led);

/**
 * @brief Sets the digit selected for display and also the segments it should show.
 * @param word 8 bits to set sequentially (A to DP)
 * @param sel The digit to write to (left to right)
 */
void shift_register_drv_set_digit_segments(uint8_t word, DIS_DIG_SELECTION sel);

/**
 * @brief Set the output enable register for the SEGMENTS
 * @param on If on is true, output is enabled. If not, no segments show
 *
 */
void shift_register_drv_seg_enable(bool on);

#endif /*_SHIFT_REGISTER_H_*/