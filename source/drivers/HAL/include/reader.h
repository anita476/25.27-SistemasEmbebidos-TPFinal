#ifndef _READER_H_
#define _READER_H_
#include <os.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct DriverMsg DriverMsg_t;

/********CODE DEFINITIONS ***************/

#define CARD_LENGTH 8U

// ISO/IEC 7811-6 Coded Characters

/*******HEXA BIN CODE + PARITY MSB  */

#define CODED_0 0b10000
#define CODED_1 0b00001
#define CODED_2 0b00010
#define CODED_3 0b10011

#define CODED_4 0b00100
#define CODED_5 0b10101
#define CODED_6 0b10110
#define CODED_7 0b00111

#define CODED_8 0b01000
#define CODED_9 0b11001
#define CODED_A 0b11010 // Symbolizes :
#define CODED_B 0b01011 // Symbolizes ;

#define CODED_C 0b11100 // Symbolizes <
#define CODED_D 0b01101 // Symbolizes =
#define CODED_E 0b01110 // Symbolizes >
#define CODED_F 0b11111 // Symbolizes ?

typedef struct {
	uint8_t id[CARD_LENGTH];
	uint8_t length;
} Card_t;

/**
 * @brief Initialize reader driver
 * @returns True if successful, false if not
 */
bool reader_drv_init(void);

/**
 * @brief Evaluates whether theres a pending card read event
 * @returns True if theres an unread card that finished processing, false if not@
 */
bool reader_drv_event(void);

/**
 * @brief Returns a a card number by pointer, if one exists and hasnt been processed yet
 * @param out_buf Pointer to the card buffer COPY.
 * @param out_length Length of the buffer.
 * @note Calling this function sets the event flag to false.
 */
void reader_drv_card(uint8_t *out_buf, uint8_t *out_len);

/**
 * @brief Creates the reader task.
 * @param event_queue Pointer to the queue where the driver will publish MSG_TYPE_CARD_READ msgs when a card is read.
 * @param prio Desired task priority
 * @returns OS_ERR_NONE if successfull, corresponding OS error code otherwise
 */
OS_ERR reader_drv_task_create(OS_Q *event_queue, OS_PRIO prio);

/**
 * @brief Return a card event message to the reader pool after processing it.
 * @param msg The pointer to the message received via event_queue
 * @note MUST be called after processing the message
 */
void reader_drv_release_event(DriverMsg_t *msg);

#endif /* _READER_H_ */