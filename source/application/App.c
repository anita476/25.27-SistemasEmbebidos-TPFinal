/***************************************************************************/ /**
   @file     App.c
   @brief    Application functions
   @author   Nicolás Magliola
  ******************************************************************************/

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/
#include "include/App.h"
#include "../drivers/HAL/include/messages.h"
#include "os.h"
#include <string.h>
/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

/**
 * global variable, will be used by fsm
 * since we are working sequentially and interrupts dont access it or use it, it should be safe
 * */
AppContext_t g_app_ctx = {.current_state = NULL, // set after initing tabl
						  .menu_selected = 0,	 /* first by default*/
						  .retry_count = 3,
						  .timer_timeout_block = TIMER_INVALID_ID,
						  .timer_misc = TIMER_INVALID_ID,
						  .display_intensity = 5,
						  .operation_result = false,
						  .card_len = 0,
						  .pin_num = 0,
						  .card_input_ctr = 0,
						  .card_input_len = ID_LENGTH,
						  .manual = false};

static OS_Q event_queue;
/*******************************************************************************
 *******************************************************************************
						GLOBAL FUNCTION DEFINITIONS
*******************************************************************************/

static EVENT App_CaptureEvent();
static OS_ERR err;
/********************************************************************************
******************************************************************************/
/* interrupts are disabled at this point*/
void App_Init(void) {
	timer_drv_init();
	encoder_drv_init();
	display_drv_init();
	reader_drv_init();
	led_drv_init();
	FSM_InitTable();

	// initial state
	g_app_ctx.current_state = FSM_GetInitState();

	OSQCreate(&event_queue, "Event queue", 100, &err);

	if (err == OS_ERR_NONE) {
		// @todo revise prios
		reader_drv_task_create(&event_queue, 4);
	}
}

/* Función que se llama constantemente en un ciclo infinito */
void App_Run(void) {
	while (1) {
		timer_drv_update(); /* must be called every iteration */

		// Capture ONE event from all input sources
		EVENT curr_event = App_CaptureEvent();

		// Feed event to FSM if theres something
		if (curr_event != EV_NONE) {
			g_app_ctx.current_state = fsm(g_app_ctx.current_state, curr_event);
		}
	}
}

static EVENT App_CaptureEvent() {
	if (timer_drv_expired(g_app_ctx.timer_misc)) {
		return EV_TIMEOUT_MISC;
	}

	if (timer_drv_expired(g_app_ctx.timer_timeout_block)) {
		return EV_TIMEOUT_BLOCK_CTR;
	}

	if (timer_drv_expired(g_app_ctx.timer_misc_err)) {
		return EV_TIMEOUT_MISC_ERROR;
	}
	/*
	if (reader_drv_event()) {
		reader_drv_card(g_app_ctx.card_buff, &g_app_ctx.card_len);
		if (auth_id_exists((uint8_t *) g_app_ctx.card_buff)) {
			return EV_RCV_CARD_S;
		}
		return EV_RCV_CARD_F;
	}
	*/
	DriverMsg_t *msg = OSQPend(&event_queue, 10u, OS_OPT_PEND_BLOCKING, NULL, NULL, &err);
	if (err != OS_ERR_NONE || msg == NULL) {
		return EV_NONE;
	}
	switch (msg->type) {
		case MSG_TYPE_CARD_READ:
			g_app_ctx.card_len = msg->data.card.length;
			memcpy(g_app_ctx.card_buff, msg->data.card.id, g_app_ctx.card_len);

			EVENT event = auth_id_exists(g_app_ctx.card_buff) ? EV_RCV_CARD_S : EV_RCV_CARD_F;
			/* return the slot to the reader drv */
			reader_drv_release_event(msg);
			return event;
			break;

		default:
			break;
	}
	swEvent sw_ev = switch_drv_pop_event();
	if (sw_ev.event_type == SW_EVENT_CLICK) {
		return EV_CLICK;
	}
	if (sw_ev.event_type == SW_EVENT_DOUBLE_CLICK) {
		return EV_DOUBLE_CLICK;
	}
	if (sw_ev.event_type == SW_EVENT_LONG_CLICK) {
		return EV_LONG_CLICK;
	}

	encoderDir enc_ev = encoder_drv_pop_event();
	if (enc_ev == ENC_CW) {
		return EV_ENC_CW;
	}
	if (enc_ev == ENC_CCW) {
		return EV_ENC_CCW;
	}

	// No event
	return EV_NONE;
}
