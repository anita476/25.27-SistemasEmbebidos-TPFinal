#include "include/pisr.h"
#include "hardware.h"

static unsigned int ticks;

typedef struct {
	unsigned int period;
	pisrCallbackPtr_t callback;
} pisrCallback_t;

typedef struct {
	pisrCallback_t callbacks[PISR_CANT];
	int used_irqs;
	int max_period;
} pisrType;
static pisrType pIrqs;

bool pisr_drv_register(pisrCallbackPtr_t fun, unsigned int period) {
	if (pIrqs.used_irqs >= PISR_CANT) {
		return false;
	}
	pIrqs.callbacks[pIrqs.used_irqs].callback = fun;
	pIrqs.callbacks[pIrqs.used_irqs].period = period;
	if (period > pIrqs.max_period) {
		pIrqs.max_period = period;
	}
	pIrqs.used_irqs++;
	return true;
}

void pisr_drv_tick(void) {
	ticks++;
	for (int i = 0; i < pIrqs.used_irqs; i++) {
		if (ticks % pIrqs.callbacks[i].period == 0) {
			pIrqs.callbacks[i].callback();
		}
	}
	if (ticks == pIrqs.max_period) {
		ticks = 0;
	}
}
