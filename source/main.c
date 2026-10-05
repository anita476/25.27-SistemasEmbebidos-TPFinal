#include "drivers/HAL/include/board.h"
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

/* Task 2 */
#define TASK2_STK_SIZE 512u
#define TASK2_STK_SIZE_LIMIT (TASK2_STK_SIZE / 10u)
#define TASK2_PRIO 3u
static OS_TCB Task2TCB;
static CPU_STK Task2Stk[TASK2_STK_SIZE];

/* Task 3 */
#define TASK3_STK_SIZE 512u
#define TASK3_STK_SIZE_LIMIT (TASK2_STK_SIZE / 10u)
#define TASK3_PRO 3u
static OS_TCB Task3TCB;
static CPU_STK Task3Stk[TASK3_STK_SIZE];

/* Example semaphore */
static OS_SEM semTest;

static void Task2(void *p_arg) {
	(void) p_arg;
	OS_ERR os_err;
	LED_COLOR_t color = BLUE_LED;

	while (1) {
		// OSSemPost(&semTest, OS_OPT_POST_1, &os_err);
		/* what color led msg to send ?*/
		const LED_COLOR_t *p_msg = (color == BLUE_LED) ? &msg_blue : &msg_green;
		OSQPost(&led_queue, (void *) p_msg, (OS_MSG_SIZE) sizeof(LED_COLOR_t), OS_OPT_POST_FIFO, &os_err);
		if (os_err != OS_ERR_NONE) {
			while (1) {
			}
		}
		color = (color == BLUE_LED) ? GREEN_LED : BLUE_LED;

		gpio_drv_toggle(PIN_LED_RED);
		OSTimeDlyHMSM(0u, 0u, 0u, 500u, OS_OPT_TIME_HMSM_STRICT, &os_err);
	}
}
static void Task3(void *p_arg) {
	(void) p_arg;
	OS_ERR os_err;
	OS_MSG_SIZE msg_size;
	CPU_TS ts;
	LED_COLOR_t *p_color;

	while (1) {
		p_color = (LED_COLOR_t *) OSQPend(&led_queue, 0u, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &os_err);
		if (os_err != OS_ERR_NONE) {
			continue;
		}

		switch (*p_color) {
			case BLUE_LED:
				gpio_drv_toggle(PIN_LED_BLUE);
				break;
			case GREEN_LED:
				gpio_drv_toggle(PIN_LED_GREEN);
				break;
		}
	}
}
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
	/* Create semaphore */
	OSSemCreate(&semTest, "Sem Test", 0u, &os_err);
	OSQCreate(&led_queue, "Led Queue", 10, &os_err);

	/* Create Task2 */
	OSTaskCreate(&Task2TCB,			   // tcb
				 "Task 2",			   // name
				 Task2,				   // func
				 0u,				   // arg
				 TASK2_PRIO,		   // prio
				 &Task2Stk[0u],		   // stack
				 TASK2_STK_SIZE_LIMIT, // stack limit
				 TASK2_STK_SIZE,	   // stack size
				 0u, 0u, 0u, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);

	/* Create Task3 */
	OSTaskCreate(&Task3TCB, "Task 3", Task3, 0u, TASK3_PRO, &Task3Stk[0u], TASK3_STK_SIZE_LIMIT, TASK3_STK_SIZE, 0u, 0u,
				 0u, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);
	if (os_err != OS_ERR_NONE) {
		while (1) {
		}
	}
	while (1) {
		OSTimeDlyHMSM(0u, 0u, 0u, 999u, OS_OPT_TIME_HMSM_STRICT, &os_err);
		// gpio_drv_toggle(PIN_LED_GREEN);
	}
}

int main(void) {
	OS_ERR err;

#if (CPU_CFG_NAME_EN == DEF_ENABLED)
	CPU_ERR cpu_err;
#endif

	hw_Init();

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
	OS_CPU_SysTickInit(SystemCoreClock / (uint32_t) OSCfg_TickRate_Hz);

	OSTaskCreate(&TaskStartTCB, "App Task Start", TaskStart, 0u, TASKSTART_PRIO, &TaskStartStk[0u],
				 (TASKSTART_STK_SIZE / 10u), TASKSTART_STK_SIZE, 0u, 0u, 0u,
				 (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP), &err);

	OSStart(&err);

	/* Should Never Get Here */
	while (1) {
	}
}