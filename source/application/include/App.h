#ifndef _APP_H_
#define _APP_H_
#include "../../../SDK/CMSIS/fsl_device_registers.h"
#include "../../drivers/HAL/include/display.h"
#include "../../drivers/HAL/include/encoder.h"
#include "../../drivers/HAL/include/led.h"
#include "../../drivers/HAL/include/reader.h"
#include "../../drivers/HAL/include/shift_register.h"
#include "../../drivers/HAL/include/switch.h"
#include "../../drivers/HAL/include/timer.h"
#include "App_commons.h"
#include "auth.h"
#include "fsm_table.h"
/* eternal loop */
void App_Run(void);

/* initialize drivers (needs interrupts disabled)*/
void App_Init(void);


#endif 