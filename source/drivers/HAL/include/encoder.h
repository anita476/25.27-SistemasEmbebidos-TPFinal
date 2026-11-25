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

#define ENC_PISR_PERIOD 5 // in ticks -> i leave it public so clients know the resolution they can expect
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
encoderDir encoder_drv_pop_event();

/**
 * @brief Creates the encoder task.
 * @param event_queue Pointer to the queue where the driver will publish MSG_TYPE_ENC msgs when encoder is moved.
 * @param prio Desired task priority
 * @returns OS_ERR_NONE if successfull, corresponding OS error code otherwise
 */
OS_ERR enc_drv_task_create(OS_Q *event_queue, OS_PRIO prio);

/**
 * @brief Return an encoder event message to the pool after processing it.
 * @param msg The pointer to the message received via event_queue
 * @note MUST be called after processing the message
 */
void reader_drv_release_event(DriverMsg_t *msg);

#endif /* DRIVERS_HAL_ENCODER_H_ */
