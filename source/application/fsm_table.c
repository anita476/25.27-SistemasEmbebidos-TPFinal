#include "include/fsm_table.h"
#include "../drivers/HAL/include/display.h"
#include "../drivers/HAL/include/led.h"
#include "../drivers/HAL/include/reader.h"
#include "../drivers/HAL/include/timer.h"
#include "include/App_commons.h"
#include <string.h>

/****************************CONST DECLARATION OF DISPLAY ARRAYS ********************************************/
const uint8_t GREETING[] = {SEG7_H, SEG7_E, SEG7_L, SEG7_L, SEG7_O, SEG7_EXCL};
const uint8_t GREETING_NUM = 6; // if i do strlen here, bc its constant its defined at compiletime ?

const uint8_t OPEN[] = {SEG7_O, SEG7_P, SEG7_E, SEG7_N};
const uint8_t OPEN_NUM = 4;

const uint8_t FAIL[] = {SEG7_F, SEG7_A, SEG7_I, SEG7_L};
const uint8_t FAIL_NUM = 4;

/* 1_ CARD */
const uint8_t MENU_CARD[] = {SEG7_1, SEG7_DP, SEG7_BLANK, SEG7_C, SEG7_A, SEG7_R, SEG7_D};
const uint8_t MENU_CARD_NUM = 7;
/* 2_ INPUT*/
const uint8_t MENU_MANUAL[] = {SEG7_2, SEG7_DP, SEG7_BLANK, SEG7_I, SEG7_N, SEG7_P, SEG7_U, SEG7_T};
const uint8_t MENU_MANUAL_NUM = 8;
/* 3_ INT.*/
const uint8_t MENU_INTENSITY[] = {SEG7_3, SEG7_DP, SEG7_BLANK, SEG7_I, SEG7_N, SEG7_T, SEG7_DP};
const uint8_t MENU_INTENSITY_NUM = 7;

const uint8_t ID_AUTH_ERR[] = {SEG7_A, SEG7_U, SEG7_T, SEG7_H, SEG7_BLANK, SEG7_E, SEG7_R, SEG7_R};
const uint8_t ID_AUTH_ERR_NUM = 8;

/* .......   ........ */
const uint8_t WAITING[] = {SEG7_DP,	   SEG7_DP, SEG7_DP, SEG7_DP, SEG7_DP, SEG7_DP, SEG7_BLANK,
						   SEG7_BLANK, SEG7_DP, SEG7_DP, SEG7_DP, SEG7_DP, SEG7_DP, SEG7_DP};
const uint8_t WAITING_NUM = 14;

/*************** NON CONSTANT ARRAYS FOR DISPLAYS */
uint8_t intensity_display[] = {SEG7_BLANK, SEG7_BLANK, SEG7_BLANK, SEG7_BLANK};
uint8_t intensity_display_num = 4;

uint8_t pin_display[] = {SEG7_BLANK, SEG7_DP, SEG7_BLANK, SEG7_BLANK};
uint8_t pin_display_num = 4;

uint8_t card_display[] = {SEG7_BLANK, SEG7_BLANK, SEG7_BLANK, SEG7_BLANK};
uint8_t card_display_num = 4;

/*  Forward declarations of state arrays */
FSMState_t state_init[];
FSMState_t state_menu_main[];
FSMState_t state_op_read_card[];
FSMState_t state_op_input_card[];
FSMState_t state_op_set_intensity[];
FSMState_t state_input_pin[];
FSMState_t state_success[];
FSMState_t state_failure[];

typedef struct {
	uint8_t *item;
	uint8_t item_length;
	FSMState_t *next_state;
	void (*preset_fun)(void);
} MenuItem_t;

/* ── Forward declarations of actions*/
static void action_show_greeting();
static void action_show_menu(void);
static void action_do_nothing(void);
static void action_menu_next(void);
static void action_menu_prev(void);
static void action_menu_select(void);
static void action_show_card(void);
static void action_show_error_card(void);
static void action_intensity_increase(void);
static void action_intensity_decrease(void);
static void action_process_dig(void);
static void action_rollback_card_num(void);
static void action_stop_misc_timer_and_menu(void);
static void action_set_init_pin(void);
static void action_set_init_pin_manual(void);
static void action_input_pin_increment_dig(void);
static void action_input_pin_decrement_dig(void);
static void action_rollback_pin_digit(void);

static void preset_menu_card(void);
static void preset_menu_manual(void);
static void preset_menu_intensity(void);
static void preset_failure_state(void);
static void preset_success_state(void);

static void action_input_card_increment(void);
static void action_input_card_decrement(void);
static void action_process_card_dig(void);
static void action_show_card_manual(void);

static void _helper_refresh_card_display(bool show_cursor);
const MenuItem_t menu[MENU_ITEMS] = {{.item = (uint8_t *) MENU_CARD,
									  .item_length = MENU_CARD_NUM,
									  .next_state = state_op_read_card,
									  .preset_fun = preset_menu_card},
									 {.item = (uint8_t *) MENU_MANUAL,
									  .item_length = MENU_MANUAL_NUM,
									  .next_state = state_op_input_card,
									  .preset_fun = preset_menu_manual},
									 {.item = (uint8_t *) MENU_INTENSITY,
									  .item_length = MENU_INTENSITY_NUM,
									  .next_state = state_op_set_intensity,
									  .preset_fun = preset_menu_intensity}};

/*************************************************************************************************************************/
/***********************************************  TRANSITIONS. **************************************************/
/*************************************************************************************************************************/
FSMState_t state_init[] = {
	{EV_CLICK, NULL, action_stop_misc_timer_and_menu}, /* if someone clicks or smth, exit to main menu*/
	{EV_DOUBLE_CLICK, NULL, action_stop_misc_timer_and_menu},
	{EV_LONG_CLICK, NULL, action_stop_misc_timer_and_menu},
	{EV_TIMEOUT_MISC, NULL, action_show_menu},
	{TABLE_END, NULL, action_do_nothing}};

FSMState_t state_menu_main[] = {{EV_ENC_CW, NULL, action_menu_next},
								{EV_ENC_CCW, NULL, action_menu_prev},
								{EV_CLICK, NULL, action_menu_select},
								{EV_LONG_CLICK, NULL, action_do_nothing},
								{TABLE_END, NULL, action_do_nothing}};

/* FIRST OP ON MENU*/
FSMState_t state_op_read_card[] = {{EV_RCV_CARD_S, NULL, action_show_card},
								   {EV_RCV_CARD_F, NULL, action_show_error_card},
								   {EV_LONG_CLICK, NULL, action_stop_misc_timer_and_menu},
								   {EV_TIMEOUT_MISC, NULL, action_set_init_pin},
								   {EV_TIMEOUT_MISC_ERROR, NULL, action_show_menu},
								   {TABLE_END, NULL, action_do_nothing}};

FSMState_t state_op_input_card[] = {{EV_ENC_CW, NULL, action_input_card_increment},
									{EV_ENC_CCW, NULL, action_input_card_decrement},
									{EV_CLICK, NULL, action_process_card_dig},
									{EV_DOUBLE_CLICK, NULL, action_rollback_card_num},
									{EV_TIMEOUT_MISC, NULL, action_set_init_pin_manual},
									{EV_TIMEOUT_MISC_ERROR, NULL, action_show_menu},
									{EV_LONG_CLICK, NULL, action_show_menu},
									{TABLE_END, NULL, action_do_nothing}};

FSMState_t state_op_set_intensity[] = {{EV_ENC_CW, NULL, action_intensity_increase},
									   {EV_ENC_CCW, NULL, action_intensity_decrease},
									   {EV_CLICK, NULL, action_show_menu},
									   {EV_LONG_CLICK, NULL, action_show_menu},
									   {TABLE_END, NULL, action_do_nothing}};

FSMState_t state_input_pin[] = {{EV_ENC_CW, NULL, action_input_pin_increment_dig},
								{EV_ENC_CCW, NULL, action_input_pin_decrement_dig},
								{EV_DOUBLE_CLICK, NULL, action_rollback_pin_digit},
								{EV_CLICK, NULL, action_process_dig}, /* going to success or not is set frm here*/
								{EV_TIMEOUT_MISC_ERROR, NULL, action_show_menu},
								{EV_LONG_CLICK, NULL, action_show_menu},
								{TABLE_END, NULL, action_do_nothing}};

FSMState_t state_success[] = {{EV_TIMEOUT_MISC, NULL, action_show_greeting}, {TABLE_END, NULL, action_do_nothing}};

FSMState_t state_failure[] = {{EV_TIMEOUT_BLOCK_CTR, NULL, action_show_greeting}, {TABLE_END, NULL, action_do_nothing}};

void FSM_InitTable(void) {
	/* TIMERS BEFORE INITING TABLES !*/
	g_app_ctx.timer_timeout_block = timer_drv_get_id();
	g_app_ctx.timer_misc = timer_drv_get_id();
	g_app_ctx.timer_misc_err = timer_drv_get_id();

	// @todo: fix later if theres time

	state_init[0].next_state = state_menu_main;
	state_init[1].next_state = state_menu_main;
	state_init[2].next_state = state_menu_main;
	state_init[3].next_state = state_menu_main;
	state_init[4].next_state = state_init; // if nothing happened, stay here

	state_menu_main[0].next_state = state_menu_main; // EV_ENC_CW
	state_menu_main[1].next_state = state_menu_main; // EV_ENC_CCW
	state_menu_main[2].next_state = NULL;			 // EV_CLICK (action decides the next state)
	state_menu_main[3].next_state = state_menu_main; // EV_LONG_CLICK
	state_menu_main[4].next_state = state_menu_main; // TABLE_END

	state_op_read_card[0].next_state = state_op_read_card; // EV_RCV_CARD_S
	state_op_read_card[1].next_state = state_op_read_card; // EV_RCV_CARD_F
	state_op_read_card[2].next_state = state_menu_main;	   // EV_LONG_CLICK
	state_op_read_card[3].next_state = state_input_pin;	   // EV_TIMEOUT_MISC -> for success!
	state_op_input_card[4].next_state = state_menu_main;   // EV_TIMEOUT_MISC_ERROR -> for error card (unauth)
	state_op_read_card[5].next_state = state_op_read_card; // TABLE_END

	state_op_input_card[0].next_state = state_op_input_card; // EV_ENC_CW
	state_op_input_card[1].next_state = state_op_input_card; // EV_ENC_CCW
	state_op_input_card[2].next_state = NULL;				 // EV_CLICK
	state_op_input_card[3].next_state = state_op_input_card; // EV_DOUBLE_CLICK
	state_op_input_card[4].next_state = state_input_pin;	 // EV_TIMEOUT_MISC
	state_op_input_card[5].next_state = state_menu_main;	 // EV_TIMEOUT_MISC_ERROR
	state_op_input_card[6].next_state = state_menu_main;	 // EV_LONG_CLICK
	state_op_input_card[7].next_state = state_op_input_card; // TABLE_END

	state_op_set_intensity[0].next_state = state_op_set_intensity; // EV_ENC_CW
	state_op_set_intensity[1].next_state = state_op_set_intensity; // EV_ENC_CCW
	state_op_set_intensity[2].next_state = state_menu_main;		   // EV_CLICK
	state_op_set_intensity[3].next_state = state_menu_main;		   // EV_LONG_CLICK
	state_op_set_intensity[4].next_state = state_op_set_intensity; // TABLE_END

	state_input_pin[0].next_state = state_input_pin; // EV_ENC_CW
	state_input_pin[1].next_state = state_input_pin; // EV_ENC_CCW
	state_input_pin[2].next_state = state_input_pin; // EV_DOUBLE_CLICK
	state_input_pin[3].next_state = NULL;			 // EV_CLICK
	state_input_pin[4].next_state = state_menu_main; // EV_TIMEOUT_MISC_ERR
	state_input_pin[5].next_state = state_menu_main; /// EV_LONG_CLICK
	state_input_pin[6].next_state = state_input_pin; // TABLE_END

	state_success[0].next_state = state_init;	 // EV_TIMEOUT_MISC
	state_success[1].next_state = state_success; // TABLE_END

	state_failure[0].next_state = state_init;	 // EV_TIMEOUT_BLOCK_CTR
	state_failure[1].next_state = state_failure; // TABLE_END

	/* Leave things in initial state*/
	action_show_greeting();
}

FSMState_t *FSM_GetInitState(void) {
	return state_init;
}

/*************************************************************************************************************************/
/***********************************************  ACTIONS *******************************************************/
/*************************************************************************************************************************/
static void action_show_greeting(void) {
	// Reset everything:
	g_app_ctx.card_curr_dig = 0;
	g_app_ctx.card_input_ctr = 0;
	g_app_ctx.card_input_len = ID_LENGHT;
	g_app_ctx.pin_ctr = 0;
	g_app_ctx.curr_dig = 0;
	g_app_ctx.menu_selected = 0;
	memset(g_app_ctx.card_input, 0, ID_LENGTH);
	memset(g_app_ctx.pin, 0, MAX_PIN_LENGTH);
	for (int i = 0; i < card_display_num; i++) {
		card_display[i] = SEG7_BLANK;
	}

	// start timer
	timer_drv_start(g_app_ctx.timer_misc, 3000, TIM_MODE_SINGLESHOT, NULL);
	// show display
	display_drv_write_word((uint8_t *) GREETING, GREETING_NUM);
	// exit
}

static void action_show_menu(void) {
	/* reset pins also */
	g_app_ctx.pin_ctr = 0;
	g_app_ctx.card_input_ctr = 0;
	g_app_ctx.curr_dig = 0;
	g_app_ctx.card_curr_dig = 0;
	display_drv_write_word(menu[g_app_ctx.menu_selected].item, menu[g_app_ctx.menu_selected].item_length);
}

static void action_do_nothing(void) {
	/*/*/
}
static void action_menu_next(void) {
	g_app_ctx.menu_selected = (g_app_ctx.menu_selected + 1) % MENU_ITEMS;
	display_drv_write_word(menu[g_app_ctx.menu_selected].item, menu[g_app_ctx.menu_selected].item_length);
}
static void action_menu_prev(void) {
	if (g_app_ctx.menu_selected == 0)
		g_app_ctx.menu_selected = MENU_ITEMS - 1;
	else
		g_app_ctx.menu_selected--;
	display_drv_write_word(menu[g_app_ctx.menu_selected].item, menu[g_app_ctx.menu_selected].item_length);
}
static void action_menu_select(void) {
	g_app_ctx.current_state = menu[g_app_ctx.menu_selected].next_state;
	(menu[g_app_ctx.menu_selected].preset_fun)();
}
static void action_show_card(void) {
	uint8_t seg_buf[ID_LENGHT];
	/* need to translate chars to segment codes*/
	for (int i = 0; i < g_app_ctx.card_len; i++) {
		uint8_t digit = g_app_ctx.card_buff[i] - '0';
		seg_buf[i] = SEG7_DIGIT(digit);
	}
	timer_drv_start(g_app_ctx.timer_misc, 5000, TIM_MODE_SINGLESHOT, NULL);
	display_drv_write_word(seg_buf, g_app_ctx.card_len);
}
static void action_show_error_card(void) {
	timer_drv_start(g_app_ctx.timer_misc_err, 7000, TIM_MODE_SINGLESHOT, NULL);
	display_drv_write_word((uint8_t *) ID_AUTH_ERR, ID_AUTH_ERR_NUM);
}
static void action_intensity_increase(void) {
	if (g_app_ctx.display_intensity < MAX_INTENSITY)
		g_app_ctx.display_intensity++;
	display_drv_set_intensity(g_app_ctx.display_intensity);
	intensity_display[intensity_display_num - 1] = SEG7_DIGIT(g_app_ctx.display_intensity);
	display_drv_write_word(intensity_display, intensity_display_num);
}

static void action_intensity_decrease(void) {
	if (g_app_ctx.display_intensity > MIN_INTENSITY)
		g_app_ctx.display_intensity--;
	display_drv_set_intensity(g_app_ctx.display_intensity);
	intensity_display[intensity_display_num - 1] = SEG7_DIGIT(g_app_ctx.display_intensity);
	display_drv_write_word(intensity_display, intensity_display_num);
}

static void action_input_pin_increment_dig(void) {
	g_app_ctx.curr_dig = (g_app_ctx.curr_dig + 1) % 10;
	pin_display[pin_display_num - 1] = SEG7_DIGIT(g_app_ctx.curr_dig);
	display_drv_write_word(pin_display, pin_display_num);
}
static void action_input_pin_decrement_dig(void) {
	if (g_app_ctx.curr_dig == 0)
		g_app_ctx.curr_dig = 9;
	else
		g_app_ctx.curr_dig--;
	pin_display[pin_display_num - 1] = SEG7_DIGIT(g_app_ctx.curr_dig);
	display_drv_write_word(pin_display, pin_display_num);
}
static void action_process_dig(void) {
	g_app_ctx.pin[g_app_ctx.pin_ctr++] = g_app_ctx.curr_dig;
	g_app_ctx.curr_dig = 0;
	if (g_app_ctx.pin_ctr == g_app_ctx.pin_num) {
		// compare, and set the new state to success or retry !
		bool res;
		if (g_app_ctx.manual) {
			res = auth_id_pin_match(g_app_ctx.card_input, g_app_ctx.pin, g_app_ctx.pin_num);
		} else {
			res = auth_id_pin_match(g_app_ctx.card_buff, g_app_ctx.pin, g_app_ctx.pin_num);
		}
		if (res) {
			timer_drv_stop(g_app_ctx.timer_misc_err);
			preset_success_state();
			return;
		} else {
			g_app_ctx.retry_count--;
			/* restart timeout for pin input*/
			timer_drv_start(g_app_ctx.timer_misc_err, 30000, TIM_MODE_SINGLESHOT, NULL);

			if (g_app_ctx.retry_count == 0) {
				preset_failure_state();
				return;
			}
			g_app_ctx.pin_ctr = 0;
		}
	}
	g_app_ctx.current_state = state_input_pin;
	pin_display[pin_display_num - 1] = SEG7_UNDERSCORE;
	pin_display[0] = SEG7_DIGIT(g_app_ctx.pin_ctr);
	display_drv_write_word(pin_display, pin_display_num);
}
static void action_rollback_pin_digit(void) {
	if (g_app_ctx.pin_ctr == 0) {
		return;
	}
	g_app_ctx.pin_ctr--;
	g_app_ctx.curr_dig = 0;
	pin_display[pin_display_num - 1] = SEG7_UNDERSCORE;
	pin_display[0] = SEG7_DIGIT(g_app_ctx.pin_ctr);
	display_drv_write_word(pin_display, pin_display_num);
}

static void action_rollback_card_num(void) {
	if (g_app_ctx.card_input_ctr == 0) {
		return;
	}
	g_app_ctx.card_input_ctr--;
	g_app_ctx.card_curr_dig = 0;
	_helper_refresh_card_display(true);
}
static void action_stop_misc_timer_and_menu() {
	// stop timerS,
	timer_drv_stop(g_app_ctx.timer_misc);
	timer_drv_stop(g_app_ctx.timer_misc_err);
	action_show_menu();
	// exit
}

static void action_set_init_pin(void) {
	g_app_ctx.pin_num = auth_id_pin_len((uint8_t *) (g_app_ctx.card_buff));
	g_app_ctx.pin_ctr = 0;
	g_app_ctx.curr_dig = 0;
	pin_display[pin_display_num - 1] = SEG7_UNDERSCORE;
	pin_display[0] = SEG7_0;
	display_drv_write_word(pin_display, pin_display_num);
	/* start a long timer, 30-60 sec, after which session expires */
	timer_drv_start(g_app_ctx.timer_misc_err, 30000, TIM_MODE_SINGLESHOT, NULL);
}

static void preset_menu_card(void) {
	g_app_ctx.manual = false;
	display_drv_write_word((uint8_t *) WAITING, WAITING_NUM);
}
static void preset_menu_manual(void) {
	g_app_ctx.manual = true;
	// card_display[0] = SEG7_DIGIT(g_app_ctx.card_input_ctr);
	card_display[card_display_num - 1] = SEG7_UNDERSCORE;
	display_drv_write_word((uint8_t *) card_display, card_display_num);
}
static void preset_menu_intensity(void) {
	intensity_display[intensity_display_num - 1] = SEG7_DIGIT(g_app_ctx.display_intensity);
	display_drv_write_word(intensity_display, intensity_display_num);
}

static void preset_failure_state(void) {
	g_app_ctx.current_state = state_failure;
	timer_drv_stop(g_app_ctx.timer_misc_err);
	display_drv_write_word((uint8_t *) FAIL, FAIL_NUM);
	led_drv_blink_failure();
	timer_drv_start(g_app_ctx.timer_timeout_block, 30000, TIM_MODE_SINGLESHOT, led_drv_stop_blink_failure);
}
static void preset_success_state(void) {
	g_app_ctx.current_state = state_success;
	display_drv_write_word((uint8_t *) OPEN, OPEN_NUM);
	led_drv_on_success();
	timer_drv_start(g_app_ctx.timer_misc, 5000, TIM_MODE_SINGLESHOT, led_drv_off_success);
}

static void action_input_card_increment(void) {
	g_app_ctx.card_curr_dig = (g_app_ctx.card_curr_dig + 1) % 10;
	_helper_refresh_card_display(false);
}
static void action_input_card_decrement(void) {
	if (g_app_ctx.card_curr_dig == 0)
		g_app_ctx.card_curr_dig = 9;
	else
		g_app_ctx.card_curr_dig--;
	_helper_refresh_card_display(false);
}
static void action_process_card_dig(void) {
	/* auth expects a stirng */
	g_app_ctx.card_input[g_app_ctx.card_input_ctr++] = g_app_ctx.card_curr_dig + '0';
	g_app_ctx.card_curr_dig = 0;
	if (g_app_ctx.card_input_ctr == g_app_ctx.card_input_len) {
		// compare, and set the new state to input pin or menu !
		if (auth_id_exists(g_app_ctx.card_input)) {
			// auth exists, so its like "EV_CRD_S"
			action_show_card_manual();
			g_app_ctx.pin_num = auth_id_pin_len(g_app_ctx.card_input);
			g_app_ctx.current_state = state_op_input_card;
			return;
		} else {
			action_show_error_card();
			g_app_ctx.current_state = state_op_input_card; // stay here, when timeout expires it will go to menu again
			return;
		}
	}
	g_app_ctx.current_state = state_op_input_card;
	_helper_refresh_card_display(true);
}

static void action_show_card_manual(void) {
	uint8_t seg_buf[ID_LENGHT];
	/* need to translate chars to segment codes*/
	for (int i = 0; i < g_app_ctx.card_input_len; i++) {
		uint8_t digit = g_app_ctx.card_input[i] - '0';
		seg_buf[i] = SEG7_DIGIT(digit);
	}
	timer_drv_start(g_app_ctx.timer_misc, 5000, TIM_MODE_SINGLESHOT, NULL);
	display_drv_write_word(seg_buf, g_app_ctx.card_input_len);
}

static void action_set_init_pin_manual(void) {
	/* start a long timer, 30-60 secs */
	g_app_ctx.pin_num = auth_id_pin_len((uint8_t *) (g_app_ctx.card_input));
	g_app_ctx.pin_ctr = 0;
	g_app_ctx.curr_dig = 0;
	pin_display[pin_display_num - 1] = SEG7_UNDERSCORE;
	pin_display[0] = SEG7_0;
	display_drv_write_word(pin_display, pin_display_num);
	timer_drv_start(g_app_ctx.timer_misc_err, 30000, TIM_MODE_SINGLESHOT, NULL);
}

static void _helper_refresh_card_display(bool show_cursor) {
	uint8_t ctr = g_app_ctx.card_input_ctr;
	for (int i = 0; i < 3; i++) {
		int src = (int) ctr - (3 - i);
		if (src < 0) {
			card_display[i] = SEG7_BLANK;
		} else {
			card_display[i] = SEG7_DIGIT(g_app_ctx.card_input[src] - '0');
		}
	}
	card_display[3] = show_cursor ? SEG7_UNDERSCORE : SEG7_DIGIT(g_app_ctx.card_curr_dig);
	display_drv_write_word(card_display, 4);
}