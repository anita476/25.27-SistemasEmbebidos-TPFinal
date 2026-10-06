#include "drivers/HAL/include/board.h"
#include "application/include/App.h"
#include "hardware.h"
#include <os.h>
/* message queue */
typedef enum { BLUE_LED = 0, GREEN_LED } LED_COLOR_t;

/* Messages are pointers, so the data they point to must outlive the post */
static const LED_COLOR_t msg_blue = BLUE_LED;
static const LED_COLOR_t msg_green = GREEN_LED;

static OS_Q led_queue;



/* Task Start */
#define TASKSTART_STK_SIZE 512u
#define TASKSTART_PRIO 2u
static OS_TCB TaskStartTCB;
static CPU_STK TaskStartStk[TASKSTART_STK_SIZE];
static CPU_TS ts;


static void TaskStart(void *p_arg) {
	(void) p_arg;
	OS_ERR os_err;

	/* Initialize the uC/CPU Services. */
	CPU_Init();

#if OS_CFG_STAT_TASK_EN > 0u
	/* (optional) Compute CPU capacity with no task running */
	OSStatTaskCPUUsageInit(&os_err);
#endif

#ifdef CPU_CFG_INT_DIS_MEAS_EN
	CPU_IntDisMeasMaxCurReset();
#endif

	ts = CPU_TS_TmrRd();
	/* Create message queue */
	OSQCreate(&led_queue, "Led Queue", 10, &os_err);
	
	if (os_err != OS_ERR_NONE) {
		while (1) {
		}
	}

	/* run the actual application as one task*/
	App_Run();
}

int main(void) {
	OS_ERR err;

#if (CPU_CFG_NAME_EN == DEF_ENABLED)
	CPU_ERR cpu_err;
#endif

	hw_Init();
	hw_DisableInterrupts();
	App_Init(); /* Program-specific setup */
	hw_EnableInterrupts();

	/* RGB LED */
	gpio_drv_mode(PIN_LED_RED, OUTPUT);
	gpio_drv_mode(PIN_LED_BLUE, OUTPUT);
	gpio_drv_mode(PIN_LED_GREEN, OUTPUT);

	gpio_drv_write(PIN_LED_RED, !LED_ACTIVE);
	gpio_drv_write(PIN_LED_BLUE, !LED_ACTIVE);
	gpio_drv_write(PIN_LED_GREEN, !LED_ACTIVE);

	OSInit(&err);
#if OS_CFG_SCHED_ROUND_ROBIN_EN > 0u
	/* Enable task round robin. */
	OSSchedRoundRobinCfg((CPU_BOOLEAN) 1, 0, &err);
#endif
	OS_AppTimeTickHookPtr = pisr_drv_tick;
	OS_CPU_SysTickInit(SystemCoreClock / (uint32_t) OSCfg_TickRate_Hz);

	OSTaskCreate(&TaskStartTCB, "App Task", TaskStart, 0u, TASKSTART_PRIO, &TaskStartStk[0u],
				 (TASKSTART_STK_SIZE / 10u), TASKSTART_STK_SIZE, 0u, 0u, 0u,
				 (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP), &err);

	OSStart(&err);

	/* Should Never Get Here */
	while (1) {
	}
}