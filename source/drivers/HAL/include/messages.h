/**
 * Events with their type:
 * both drivers and application need a common contract to know what events are in the queue
 */
#ifndef __MESSAGES_H_
#define __MESSAGES_H_
#include "encoder.h"
#include "reader.h"
#include <stdint.h>

typedef enum {
	MSG_TYPE_CARD_READ,
	MSG_TYPE_ENCODER_STEP,
	MSG_TYPE_COUNT
} MSG_T; /* only for DRIVER EVENTS, diff from application events! means theres a message*/

typedef struct DriverMsg {
	MSG_T type;
	union {
		Card_t card;
		encoderDir encoder;
	} data;
} DriverMsg_t;

#endif