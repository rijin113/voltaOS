#include "stdio.h"
#include "kernel.h"
#include "stm32f4xx_hal.h"

/* Initialize variables for multithreading and scheduling in the OS */
uint32_t stack_pool;
uint32_t* last_stack_ptr;
thread tcb_array[32]; // Only 32 threads can be created from 16KB of stack space
int thread_index;
int numThreadsRunning;

/* Set up function to handle system calls */
void SVC_Handler_Main( unsigned int *svc_args )
{
	unsigned int svc_number;
	/*
	* Stack contains:
	* r0, r1, r2, r3, r12, r14, the return address and xPSR
	* First argument (r0) is svc_args[0]
	*/
	svc_number = ( ( char * )svc_args[ 6 ] )[ -2 ] ;
	switch( svc_number )
	{
		case RUN_FIRST_THREAD:
			__set_PSP((uint32_t)tcb_array[thread_index].sp);
			runFirstThread();
			break;
		case YIELD:
			tcb_array[thread_index].runtime = tcb_array[thread_index].timeslice;
			_ICSR |= 1<<28; //Pend an interrupt to do the context switch
			__asm("isb");
			break;
		default: /* unknown SVC */
			break;
	}
}

/* Check how much of the stack space is allocated and if
   more threads can be created. */
uint32_t * checkif_stack_available()
{
	  if(stack_pool > 0x0)
	  {
		  uint32_t PSP = (uint32_t)(last_stack_ptr) - STACK_SIZE;
		  last_stack_ptr = (uint32_t*)PSP;
		  stack_pool = stack_pool - STACK_SIZE;
		  return last_stack_ptr;
	  }
	  else
	  {
		  return NULL;
	  }
}

/* Return the index with the lowest deadline */
int find_lowestDeadline()
{
	int tmp_deadline = 999999;
	int tmp_index = 0;
	for (int i=0; i < numThreadsRunning;i++)
	{
		if(tcb_array[i].deadline < tmp_deadline)
		{
			tmp_index = i;
			tmp_deadline = tcb_array[i].deadline;
		}
	}
	return tmp_index;
}

/* Initialize the OS kernel */
void os_kernel_initialize()
{
	SHPR3 |= 0xFE << 16; // Shift the constant 0xFE 16 bits to set PendSV priority
	SHPR2 |= 0xFDU << 24; // Set the priority of SVC higher than PendSV

	stack_pool = 0x4000; // Set 16KB of stack space
	last_stack_ptr = *(uint32_t**)0x0; //Initialize the first stack ptr as MSP
	thread_index = -1;
	numThreadsRunning = 0;
}

/* Create a thread with fixed metadata (Ex. Deadline, Period, etc) */
uint32_t os_createthread(void (*function)(void *args), void * args)
{
	  if(checkif_stack_available() != NULL)
	  {
		  /* Filling up the thread stack */
		  *(--last_stack_ptr) = 1<<24;
		  *(--last_stack_ptr) = (uint32_t)function; // loading the function into the PC register
		  for (int i = 1; i <= 14; i++)
		  {
			  /* Set R0 to user argument if reached.
			     This is to make sure user arguments are retained
			   	 after a context switch. */
			  if (i == 6)
			  {
				  *(--last_stack_ptr) = args;
			  }
			  else
			  {
				  *(--last_stack_ptr) = 0xA;
			  }
		  }

		  numThreadsRunning++;
		  thread_index++;
		  tcb_array[thread_index].timeslice = 5;
		  tcb_array[thread_index].runtime = 5;
		  tcb_array[thread_index].deadline = 10;
		  tcb_array[thread_index].period = 10;
		  tcb_array[thread_index].sp = last_stack_ptr;
		  tcb_array[thread_index].thread_function = function;

		  // Thread was created successfully
		  return 1;
	  }
	  else
	  {
		  // Thread was not created successfully
		  return 0;
	  }
}

/* Create a thread with variable metadata */
uint32_t os_createthreadWithDeadline(void (*function)(void *args), void * args, int thread_time, int deadline)
{
	  if(checkif_stack_available() != NULL)
	  {
		  /* Filling up the thread stack */
		  *(--last_stack_ptr) = 1<<24;
		  *(--last_stack_ptr) = (uint32_t)function; // loading the function into the PC register
		  for (int i = 1; i <= 14; i++)
		  {
			  /* Set R0 to user argument if reached.
			     This is to make sure user arguments are retained
			   	 after a context switch. */
			  if (i == 6)
			  {
				  *(--last_stack_ptr) = args;
			  }
			  else
			  {
				  *(--last_stack_ptr) = 0xA;
			  }
		  }

		  numThreadsRunning++;
		  thread_index++;
		  tcb_array[thread_index].timeslice = thread_time;
		  tcb_array[thread_index].runtime = thread_time;
		  tcb_array[thread_index].period = deadline;
		  tcb_array[thread_index].deadline = deadline;
		  tcb_array[thread_index].sp = last_stack_ptr;
		  tcb_array[thread_index].thread_function = function;

		  // Thread was created successfully
		  return 1;
	  }
	  else
	  {
		  // Thread was not created successfully
		  return 0;
	  }
}

/* Start the kernel and any created threads by the user */
void os_kernel_start()
{
	thread_index = find_lowestDeadline();
	__asm("SVC #3");
}

/* Implement EDF Sceduling (Earliest Deadline First) */
void osSched()
{
	tcb_array[thread_index].sp = (uint32_t*)(__get_PSP() - 8*4);
	thread_index = find_lowestDeadline();
	__set_PSP((uint32_t)tcb_array[thread_index].sp);
	return;
}

/* Call SVC Handler when a thread voluntarily yields */
void osYield(void)
{
	__asm("SVC #8");
}


