#include "stm32f4xx_hal.h"

#define RUN_FIRST_THREAD 0x3
#define STACK_SIZE 0x200
#define SHPR2 *(uint32_t*)0xE000ED1C //for setting SVC priority, bits 31-24
#define SHPR3 *(uint32_t*)0xE000ED20 // PendSV is bits 23-16
#define _ICSR *(uint32_t*)0xE000ED04 //This lets us trigger PendSV
#define YIELD 0x8

extern void runFirstThread(void);

/* A struct containing a thread's metadata */
typedef struct k_thread{
	int timeslice;
	int runtime;
	int period;
	int deadline;
	uint32_t* sp; //stack pointer
	void (*thread_function)(void*); //function pointer
}thread;

int find_lowestDeadline();
void SVC_Handler_Main(unsigned int *svc_args);

/* Functions a user can call in main to initialize
   the kernel and create threads */
void os_kernel_initialize(void);
uint32_t os_createthread(void (*function)(void *args), void *args);
uint32_t os_createthreadWithDeadline(void (*function)(void *args), void * args, int thread_time, int deadline);
void os_kernel_start(void);
void osSched();
void osYield(void);
