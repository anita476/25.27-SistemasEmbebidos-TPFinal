#ifndef _AUTH_H_
#define _AUTH_H_
#include <stdint.h>
#include <stdbool.h>

#define ID_LENGTH  8
#define MAX_PIN_LENGTH 5
/**
 * @brief Indicates whether a given id exists
 * @returns True if it exists as a user, false if not
 */
bool auth_id_exists(const uint8_t *id);

/**
 * @brief Authenticator function, returns whether an id-pin pair is correct or not
 * @returns True if they match, false if not.
 */
bool auth_id_pin_match(const uint8_t *id, const uint8_t *pin, uint8_t pin_len);

/**
 * @brief Returns the length of an id's pin
 * @returns The length of the pin, or -1 if the id does not exist
 */
int8_t auth_id_pin_len(const uint8_t *id);

#endif /* _AUTH_H_ */