#ifndef _READER_H_
#define _READER_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/********CODE DEFINITIONS ***************/

#define ID_LENGHT 8U

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

#endif /* _READER_H_ */