// id_pin_map.c
#include "include/auth.h"
#include <string.h>
#define MAP_SIZE (sizeof(map) / sizeof(map[0]))

typedef struct {
	uint8_t id[ID_LENGTH];
	uint8_t pin[MAX_PIN_LENGTH];
	uint8_t pin_len; // 4 or 5 !
} UserEntry_t;

static const UserEntry_t map[] = { //{{4, 0, 5, 0, 7, 1, 0, 1}, {1, 2, 3, 4}, 4},
	{"40507101", {1, 2, 3, 4}, 4},
	{"60612683", {1, 2, 0, 2, 4}, 5}};

bool auth_id_exists(const uint8_t *id) {
	for (int i = 0; i < MAP_SIZE; i++) {
		if (memcmp(map[i].id, id, ID_LENGTH) == 0)
			return true;
	}
	return false;
}

bool auth_id_pin_match(const uint8_t *id, const uint8_t *pin, uint8_t pin_len) {
	for (int i = 0; i < MAP_SIZE; i++) {
		if (memcmp(map[i].id, id, ID_LENGTH) != 0)
			continue;
		if (map[i].pin_len != pin_len)
			return false;
		return memcmp(map[i].pin, pin, pin_len) == 0;
	}
	return false;
}

int8_t auth_id_pin_len(const uint8_t *id) {
	for (int i = 0; i < MAP_SIZE; i++) {
		if (memcmp(map[i].id, id, ID_LENGTH) == 0)
			return map[i].pin_len;
	}
	return -1; // id not found
}