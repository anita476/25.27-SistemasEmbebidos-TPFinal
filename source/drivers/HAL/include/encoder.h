/**
 * encoder.h
 *
 *  Created on: Mar 26, 2026
 *      Author: anegre
 **/

#ifndef DRIVERS_HAL_ENCODER_H_
#define DRIVERS_HAL_ENCODER_H_
#include "../..//MCAL/include/gpio.h"
#include "../../MCAL/include/pisr.h"
#include "board.h"

#define ENC_PISR_PERIOD 5 // in ticks

#define ENC_MAX_PENDING_EVENTS 32

typedef enum : int8_t {
	ENC_CCW = -1,
	ENC_NONE = 0,
	ENC_CW = +1,
} encoderDir;

typedef struct {
	pin_t channelA; // Obs! absolute mcu pins
	pin_t channelB;
} encChannels;

typedef int8_t encStep_t; // step is -1 or 1, or 0 if empty

/**
 * @brief Initialize the with the corresponding channel pins
 * @return True if successful, false if not
 **/
bool encoder_drv_init();

/**
 * @brief Pop an event from the encoder event queue. Queue is circular with max ENC_MAX_PENDING_EVENTS, otherwise
 * overwrites
 * @returns 1 for a clockwise step, -1 for counter clockwise. Returns 0 if queue is empty.
 */
encStep_t encoder_drv_pop_event();

#endif /* DRIVERS_HAL_ENCODER_H_ */
