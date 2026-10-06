#ifndef _FSM_H_
#define _FSM_H_

#define TABLE_END 0xFF

// Forward declaration for self-referencing struct
typedef struct FSMState_t FSMState_t;

// has to be a user or pisr
typedef enum {
	EV_CLICK,
	EV_DOUBLE_CLICK,
	EV_LONG_CLICK,
	EV_ENC_CCW,
	EV_ENC_CW,
	EV_TIMEOUT_MISC,	   /* for visual timeouts, show an image for around 2 secs */
	EV_TIMEOUT_MISC_ERROR, /* for visual ERROR timeouts */
	EV_TIMEOUT_BLOCK_CTR,  /* in block, timeout a certain number of times to see countdown*/
	EV_END_TRIES,		   /* block after x retries*/
	EV_RCV_CARD_S,		   /* recieved the card interrupt flag, and card is correct !*/
	EV_RCV_CARD_F,		   /* recieved card but the user doesnt ecist*/
	EV_SUCCESS,
	EV_FAILURE,
	EV_NONE
} EVENT;

struct FSMState_t {
	EVENT event;
	FSMState_t *next_state;
	void (*action_fun)(void); /* callback action */
};

/** fsm entrypoint */
FSMState_t *fsm(FSMState_t *p_state_table, EVENT curr_event);

#endif /* _FSM_H_ */