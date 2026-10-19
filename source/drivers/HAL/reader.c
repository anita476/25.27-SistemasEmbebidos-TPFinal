#include "include/reader.h"
#include "../MCAL/include/gpio.h"
#include "include/board.h"
#include "include/messages.h"
#include <string.h>

#define TASK_READER_STK_SIZE 512u
#define CARD_NUM_DIGITS 8
#define CARD_POOL_SIZE 4u /* how many cards can we save untill full*/

static DriverMsg_t card_event_pool[CARD_POOL_SIZE];
static bool card_event_in_use[CARD_POOL_SIZE];

static OS_Q free_card_slots;
static OS_Q *shared_event_queue;

void card_Enable_ISR(void);
void card_Clock_ISR(void);
/* actual card task*/
static void TaskCardReader(void *p_arg);

/**
 * card reading sem for reading !
 */
static OS_SEM reader_isr_sem;
static OS_ERR reader_sem_err;
static OS_ERR reader_queue_err;

volatile uint8_t buffer[CARD_LENGTH];
volatile int bit_count = 0;
volatile int char_count = 0;
volatile uint8_t temp_char = 0;
volatile bool reading_active = false;
volatile bool card_ready = false;

/* OS TASK VARIABLES */
static OS_TCB TaskReaderTCB;
static CPU_STK TaskReaderStk[TASK_READER_STK_SIZE];

/********************** READER FUNCTIONS */
/* @todo change bool to OS_ERR */
bool reader_drv_init(void) {
	// FIRST initializze gpios
	gpio_drv_mode(PIN_CARD_DATA, INPUT); // @todo change all to input pullup if all are high on idle
	gpio_drv_mode(PIN_CARD_CLOCK, INPUT);
	gpio_drv_mode(PIN_CARD_ENABLE, INPUT);

	bool int1 = gpio_drv_IRQ(PIN_CARD_ENABLE, GPIO_IRQ_MODE_FALLING_EDGE, card_Enable_ISR);
	// FALLING EDGE !
	bool int2 = gpio_drv_IRQ(PIN_CARD_CLOCK, GPIO_IRQ_MODE_FALLING_EDGE, card_Clock_ISR);

	if (int1 == false || int2 == false) {
		return false;
	}
	OS_ERR err;
	OSSemCreate(&reader_isr_sem, "Internal reader semaphore", 0, &err);
	if (err != OS_ERR_NONE) {
		return false;
	}
	OSQCreate(&free_card_slots, "Private card queue", CARD_POOL_SIZE, &err);
	if (err != OS_ERR_NONE) {
		return false;
	}
	/* each post means a card slot is free */
	for (size_t i = 0; i < CARD_POOL_SIZE; ++i) {
		OSQPost(&free_card_slots, &card_event_pool[i], sizeof(card_event_pool[i]), OS_OPT_POST_FIFO, &err);
		if (err != OS_ERR_NONE) {
			return false;
		}
	}
	return true;
}

static void reader_drv_reset(void) {
	bit_count = 0;
	char_count = 0;
	temp_char = 0;
	reading_active = false;
	card_ready = false;
}

static size_t reader_drv_card_slot_index(const DriverMsg_t *msg) {
	for (size_t i = 0; i < CARD_POOL_SIZE; ++i) {
		if (msg == &card_event_pool[i]) {
			return i;
		}
	}
	return CARD_POOL_SIZE;
}

/* replace with queue reading !!*/
bool reader_drv_event(void) {
	return card_ready;
}

/* this is the tasks task*/
void reader_drv_card(uint8_t *out_buf, uint8_t *out_len) {
	if (!card_ready) {
		return;
	}
	*out_len = CARD_NUM_DIGITS; /* char count will be 19*/
	memcpy(out_buf, (void *) buffer, CARD_NUM_DIGITS);
	reader_drv_reset();
}

void card_Enable_ISR(void) {
	reader_drv_reset();
}

void card_Clock_ISR(void) {
	bool bit = !gpio_drv_read(PIN_CARD_DATA);

	if (!reading_active) {
		temp_char = (temp_char >> 1) | (bit << 4); // LSB first
		if (temp_char == CODED_B) {
			reading_active = true;
			bit_count = 0;
			temp_char = 0;
		}
	} else {
		temp_char |= (bit << bit_count); // LSB FIRST !!!
		bit_count++;
		if (bit_count == 5) {
			if (temp_char == CODED_D) { // it still keeps reading until FS, only that it doesnt put it in the buffer
				buffer[char_count] = '\0';
				card_ready = true;
				reading_active = false;
				/* notify posting to sem*/
				OSSemPost(&reader_isr_sem, OS_OPT_POST_ALL, &reader_sem_err);
			} else if (char_count < 8) {
				buffer[char_count++] = '0' + (temp_char & 0x0F); // & 0f to get rid of parity bitt
			}
			bit_count = 0;
			temp_char = 0;
		}
	}
}

static void TaskCardReader(void *p_arg) {
	(void) p_arg; /* avoid warning */
	DriverMsg_t *msg;
	Card_t discarded_card; // if we need to discard
	OS_ERR err;
	CPU_SR_ALLOC();

	while (1) {
		OSSemPend(&reader_isr_sem, 0, OS_OPT_PEND_BLOCKING, NULL, &reader_sem_err);
		if (reader_sem_err != OS_ERR_NONE) {
			continue;
		}
		/* if here, theres a card*/

		msg = OSQPend(&free_card_slots, 0u, OS_OPT_PEND_NON_BLOCKING, NULL, NULL, &reader_queue_err);
		if (reader_queue_err == OS_ERR_Q_EMPTY) {
			// no slot is free -> discard the car
			reader_drv_card(discarded_card.id, &discarded_card.length);
			continue;
		}
		if (reader_queue_err != OS_ERR_NONE) {
			// something else went wrong ..
			continue;
		}

		size_t slot = reader_drv_card_slot_index(msg);
		if (slot == CARD_POOL_SIZE) {
			reader_queue_err = OS_ERR_PTR_INVALID;
			continue;
		}
		CPU_CRITICAL_ENTER();
		card_event_in_use[slot] = true;
		CPU_CRITICAL_EXIT();

		msg->type = MSG_TYPE_CARD_READ;
		reader_drv_card(msg->data.card.id, &msg->data.card.length);

		OSQPost(shared_event_queue, msg, sizeof(*msg), OS_OPT_POST_FIFO, &err);
		if (err != OS_ERR_NONE) {
			reader_drv_release_event(msg);
		}
	}
}

void reader_drv_release_event(DriverMsg_t *msg) {
	OS_ERR err;

	if (msg == NULL) {
		return;
	}

	size_t slot = reader_drv_card_slot_index(msg);
	if (slot == CARD_POOL_SIZE) {
		return;
	}

	/* both the application and */
	CPU_SR_ALLOC();
	CPU_CRITICAL_ENTER();
	if (!card_event_in_use[slot]) {
		CPU_CRITICAL_EXIT();
		return;
	}
	card_event_in_use[slot] = false;
	CPU_CRITICAL_EXIT();

	/* release it */
	OSQPost(&free_card_slots, msg, sizeof(*msg), OS_OPT_POST_FIFO, &err);
	if (err != OS_ERR_NONE) {
		CPU_CRITICAL_ENTER();
		// couldnt release it
		card_event_in_use[slot] = true;
		CPU_CRITICAL_EXIT();
	}
}

OS_ERR reader_drv_task_create(OS_Q *event_queue, OS_PRIO prio) {
	OS_ERR err;
	if (event_queue == NULL) {
		return OS_ERR_OBJ_PTR_NULL;
	}
	shared_event_queue = event_queue; /* we post to this queue and application processes*/
	OSTaskCreate(&TaskReaderTCB, "Card Reader", TaskCardReader, NULL, prio, &TaskReaderStk[0],
				 (TASK_READER_STK_SIZE - TASK_READER_STK_SIZE / 10u), TASK_READER_STK_SIZE, 0u, 0u, NULL,
				 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR, &err);
	if (err != OS_ERR_NONE) {
		shared_event_queue = NULL;
	}
	return err;
}