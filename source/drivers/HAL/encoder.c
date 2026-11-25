#include "include/encoder.h"
#include "../MCAL/include/gpio.h"
#include "include/board.h"
#include "include/messages.h"
#include "include/switch.h"
#include <os.h>

#define TASK_ENC_STK_SIZE 512u
#define MAX_ISR_ENC_EV_PENDING 8u

/* this will be the private queue POINTERS
 */
static DriverMsg_t enc_event_pool[ENC_MAX_PENDING_EVENTS];
static bool enc_event_in_use[ENC_MAX_PENDING_EVENTS];

static OS_Q *shared_event_queue; // supplied by client
static OS_SEM enc_isr_sem;
static OS_ERR enc_sem_err;
static OS_ERR enc_queue_err;

/* this will be the private queue
 */
static OS_Q free_enc_slots;

/* for isr + task  communication*/
static encoderDir queue[MAX_ISR_ENC_EV_PENDING];
static volatile uint8_t _head = 0; // newst the ISR writes here
static volatile uint8_t _tail = 0; // oldest , the ENC TASK will read from here -> 0 a MAX_PENDING -1

static encChannels channel_pins; // to keep them somewhere

/* OS TASK VARIABLES */
static OS_TCB TaskEncoderTCB;
static CPU_STK TaskEncoderStk[TASK_ENC_STK_SIZE];

static uint8_t previous;

static void _encoder_drv_push_event(encoderDir dir);
static void TaskEncoder(void *p_arg);
static size_t enc_drv_slot_index(const DriverMsg_t *msg);

pisrCallbackPtr_t encoder_drv_PISR(void);

bool encoder_drv_init() {
	channel_pins.channelA = PIN_ENC_CHNA;
	channel_pins.channelB = PIN_ENC_CHNB;
	gpio_drv_mode(channel_pins.channelA, INPUT_PULLUP); // theres already a pullup but oh well
	gpio_drv_mode(channel_pins.channelB, INPUT_PULLUP);
	_head = 0;
	_tail = 0;

	/* INITIALIZE SWITCH DRV FOR ENCODER SW*/
	switch_drv_init();
	switch_drv_register(PIN_SW_ENC, ACTIVE_ON_LOW, PULL_UP);
	return pisr_drv_register((pisrCallbackPtr_t) encoder_drv_PISR, ENC_PISR_PERIOD);

	// initialize private queue and post the free events
	OS_ERR err;
	OSSemCreate(&enc_isr_sem, "Internal encoder semaphore", 0, &err);
	if (err != OS_ERR_NONE) {
		return false;
	}
	OSQCreate(&free_enc_slots, "Private encoder queue", ENC_MAX_PENDING_EVENTS, &err);
	if (err != OS_ERR_NONE) {
		return false;
	}
	/* each post means an encoder event slot is free */
	for (size_t i = 0; i < ENC_MAX_PENDING_EVENTS; ++i) {
		OSQPost(&free_enc_slots, &enc_event_pool[i], sizeof(enc_event_pool[i]), OS_OPT_POST_FIFO, &err);
		if (err != OS_ERR_NONE) {
			return false;
		}
	}
	return true;
}

encoderDir encoder_drv_pop_event() {
	if (_head == _tail) {
		return ENC_NONE;
	}
	encoderDir dir = queue[_tail];
	_tail = (_tail + 1) & (MAX_ISR_ENC_EV_PENDING - 1); // add 1 and & only if _tail is the last possible one
	return dir;
}
/**
 * Idea:
 * Two channels A and B
 * We need to maintain the current vs previous values of both.
 *
 * AB -> one uitn8
 * 00 -> nothing
 * 01 -> A is inactive, B is active
 * 10 -> A is active, B is not
 * 11 -> both are active
 *
 *
 * Obs! 4x 4 puedo hacer un array de lookup para que que pasa en cada caso de previous vs actual  -> pero solo me
 * interesa cuando pasa de 0 a algo Obs! Depende de cuantos eventos quiero por "click"
 *
 * Por ejemplo:
 * 00 + 10 -> estaaban en 0 y ahora A tocó antes que B  -> es un mov cwise
 * 00 + 01 -> estaban en 0 y ahora B tocó antes que A -> counter clockwise
 * todos los otros casos no los toco ahora ... no debería ser necesario
 *
 */

pisrCallbackPtr_t encoder_drv_PISR(void) {
	uint8_t current = (gpio_drv_read(channel_pins.channelA) << 1) | gpio_drv_read(channel_pins.channelB);
	uint8_t index = (previous << 2) | current;

	switch (index) {
		case 0b1110:
			_encoder_drv_push_event(ENC_CW);
			OS_SemPost(&enc_isr_sem, OS_OPT_POST_ALL, NULL, &enc_sem_err);
			break;
		case 0b1101:
			_encoder_drv_push_event(ENC_CCW);
			OS_SemPost(&enc_isr_sem, OS_OPT_POST_ALL, NULL, &enc_sem_err);
			break;
		default:
			break;
	}

	previous = current;
}

static void _encoder_drv_push_event(encoderDir dir) {
	uint8_t next = (_head + 1u) & (MAX_ISR_ENC_EV_PENDING - 1u); // if full, then go to start

	if (next == _tail) // if we caught up to the tail, events were never consumed!!
		return;		   // drop events if queue is full

	queue[_head] = dir;
	_head = next;
}

OS_ERR enc_drv_task_create(OS_Q *event_queue, OS_PRIO prio) {
	OS_ERR err;
	if (event_queue == NULL) {
		return OS_ERR_OBJ_PTR_NULL;
	}
	shared_event_queue = event_queue; /* we post to this queue and application processes*/
	OSTaskCreate(&TaskEncoderTCB, "Encoder", TaskEncoder, NULL, prio, &TaskEncoderStk[0], (TASK_ENC_STK_SIZE / 10u),
				 TASK_ENC_STK_SIZE, 0u, 0u, NULL, OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR, &err);
	if (err != OS_ERR_NONE) {
		shared_event_queue = NULL;
	}
	return err;
}

static void TaskEncoder(void *p_arg) {
	(void) p_arg; /* avoid warning */
	DriverMsg_t *msg;
	Card_t discarded_card; // if we need to discard
	OS_ERR err;
	OS_MSG_SIZE free_slot_msg_size;
	CPU_SR_ALLOC();

	while (1) {
		OSSemPend(&enc_isr_sem, 0, OS_OPT_PEND_BLOCKING, NULL, &enc_sem_err);
		if (enc_sem_err != OS_ERR_NONE) {
			continue;
		}
		/* if here, theres an encoder event to process*/

		msg = OSQPend(&free_enc_slots, 0u, OS_OPT_PEND_NON_BLOCKING, &free_slot_msg_size, NULL, &enc_queue_err);
		if (enc_queue_err == OS_ERR_Q_EMPTY) {
			(void *) encoder_drv_pop_event();
			continue;
		}
		if (enc_queue_err != OS_ERR_NONE) {
			// something else went wrong ..
			continue;
		}

		size_t slot = enc_drv_slot_index(msg);
		if (slot == ENC_MAX_PENDING_EVENTS) {
			enc_queue_err = OS_ERR_PTR_INVALID;
			continue;
		}
		CPU_CRITICAL_ENTER();
		enc_event_in_use[slot] = true;
		CPU_CRITICAL_EXIT();

		msg->type = MSG_TYPE_ENCODER_STEP;
		msg->data.encoder = encoder_drv_pop_event();

		OSQPost(shared_event_queue, msg, sizeof(*msg), OS_OPT_POST_FIFO, &err);
		if (err != OS_ERR_NONE) {
			enc_drv_release_event(msg);
		}
	}
}

void enc_drv_release_event(DriverMsg_t *msg) {
	OS_ERR err;

	if (msg == NULL) {
		return;
	}

	size_t slot = enc_drv_card_slot_index(msg);
	if (slot == ENC_MAX_PENDING_EVENTS) {
		return;
	}

	/* both the application and */
	CPU_SR_ALLOC();
	CPU_CRITICAL_ENTER();
	if (!enc_event_in_use[slot]) {
		CPU_CRITICAL_EXIT();
		return;
	}
	enc_event_in_use[slot] = false;
	CPU_CRITICAL_EXIT();

	/* release it */
	OSQPost(&free_enc_slots, msg, sizeof(*msg), OS_OPT_POST_FIFO, &err);
	if (err != OS_ERR_NONE) {
		CPU_CRITICAL_ENTER();
		// couldnt release it
		enc_event_in_use[slot] = true;
		CPU_CRITICAL_EXIT();
	}
}

static size_t enc_drv_slot_index(const DriverMsg_t *msg) {
	for (size_t i = 0; i < ENC_MAX_PENDING_EVENTS; ++i) {
		if (msg == &enc_event_pool[i]) {
			return i;
		}
	}
	return ENC_MAX_PENDING_EVENTS;
}