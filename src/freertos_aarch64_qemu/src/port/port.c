/*
 * FreeRTOS AArch64 port for RK3588 (GICv3 system register interface)
 *
 * Adapted from FreeRTOS-Kernel portable/GCC/ARM_AARCH64/port.c
 * Modified to use GICv3 ICC_*_EL1 system registers instead of MMIO.
 *
 * SPDX-License-Identifier: MIT
 */

/* Scheduler includes */
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

/* GICv3 system register access helpers */
static inline uint64_t read_icc_pmr_el1(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, ICC_PMR_EL1" : "=r" (val));
	return val;
}

static inline void write_icc_pmr_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_PMR_EL1, %0" :: "r" (val));
}

static inline uint64_t read_icc_bpr0_el1(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, ICC_BPR0_EL1" : "=r" (val));
	return val;
}

static inline uint64_t read_icc_rpr_el1(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, ICC_RPR_EL1" : "=r" (val));
	return val;
}

static inline uint64_t read_icc_iar1_el1(void)
{
	uint64_t val;
	__asm__ volatile ("mrs %0, ICC_IAR1_EL1" : "=r" (val));
	return val;
}

static inline void write_icc_eoir1_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_EOIR1_EL1, %0" :: "r" (val));
}

static inline void write_icc_sre_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_SRE_EL1, %0" :: "r" (val));
}

static inline void write_icc_igrpen1_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_IGRPEN1_EL1, %0" :: "r" (val));
}

/* Constants */
#define portNO_CRITICAL_NESTING      ( ( size_t ) 0 )
#define portNO_FLOATING_POINT_CONTEXT ( ( StackType_t ) 0 )
#define portUNMASK_VALUE             ( 0xFFUL )
#define portBINARY_POINT_BITS        ( ( uint8_t ) 0x03 )
#define portMAX_BINARY_POINT_VALUE   ( ( uint8_t ) 0x00 )
#define portAPSR_MODE_BITS_MASK      ( 0x0C )
#define portDAIF_I                   ( 0x80 )
#define portFPU_REGISTER_WORDS       ( 32 * 2 ) /* 32 x 128-bit = 64 x 64-bit */

/* EL1 configuration for bare-metal AMP */
#define portSP_ELx   ( ( StackType_t ) 0x01 )
#define portSP_EL0   ( ( StackType_t ) 0x00 )
#define portEL1      ( ( StackType_t ) 0x04 )
#define portINITIAL_PSTATE ( portEL1 | portSP_EL0 )

/* GICv3 priority: use ICC_PMR_EL1 for interrupt masking */
#define portICCPMR_PRIORITY_MASK_REGISTER  read_icc_pmr_el1()
#define portICCBPR_BINARY_POINT_REGISTER   read_icc_bpr0_el1()
#define portICCRPR_RUNNING_PRIORITY_REGISTER read_icc_rpr_el1()

/* Used to pass IAR value from ASM to C handler */
__attribute__( ( used ) ) volatile uint64_t ullICCIARValue = 0;

/* Saved as part of task context */
volatile uint64_t ullCriticalNesting = 9999ULL;
uint64_t ullPortTaskHasFPUContext = pdFALSE;
uint64_t ullPortYieldRequired = pdFALSE;
uint64_t ullPortInterruptNesting = 0;

/* Forward declarations - match portmacro.h extern declarations */
extern UBaseType_t uxPortSetInterruptMask(void);
extern void vPortClearInterruptMask(UBaseType_t uxNewMaskValue);

__attribute__( ( used ) ) const uint64_t ullMaxAPIPriorityMask =
	( configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT );

extern void vPortRestoreTaskContext(void);

/*-----------------------------------------------------------*/

StackType_t * pxPortInitialiseStack(StackType_t * pxTopOfStack,
				    TaskFunction_t pxCode,
				    void * pvParameters)
{
	/* Setup the initial stack of the task as expected by portRESTORE_CONTEXT. */
	pxTopOfStack--;
	*pxTopOfStack = 0x0101010101010101ULL; /* X1 */
	pxTopOfStack--;
	*pxTopOfStack = (StackType_t) pvParameters; /* X0 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0303030303030303ULL; /* X3 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0202020202020202ULL; /* X2 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0505050505050505ULL; /* X5 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0404040404040404ULL; /* X4 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0707070707070707ULL; /* X7 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0606060606060606ULL; /* X6 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0909090909090909ULL; /* X9 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0808080808080808ULL; /* X8 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1111111111111111ULL; /* X11 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1010101010101010ULL; /* X10 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1313131313131313ULL; /* X13 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1212121212121212ULL; /* X12 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1515151515151515ULL; /* X15 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1414141414141414ULL; /* X14 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1717171717171717ULL; /* X17 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1616161616161616ULL; /* X16 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1919191919191919ULL; /* X19 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1818181818181818ULL; /* X18 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2121212121212121ULL; /* X21 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2020202020202020ULL; /* X20 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2323232323232323ULL; /* X23 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2222222222222222ULL; /* X22 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2525252525252525ULL; /* X25 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2424242424242424ULL; /* X24 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2727272727272727ULL; /* X27 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2626262626262626ULL; /* X26 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2929292929292929ULL; /* X29 (FP) */
	pxTopOfStack--;
	*pxTopOfStack = 0x2828282828282828ULL; /* X28 */
	pxTopOfStack--;
	*pxTopOfStack = (StackType_t) 0x00;    /* XZR - padding */
	pxTopOfStack--;
	*pxTopOfStack = (StackType_t) 0x00;    /* X30 (LR) */
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_PSTATE;    /* SPSR */
	pxTopOfStack--;
	*pxTopOfStack = (StackType_t) pxCode;  /* ELR (entry point) */

#if ( configUSE_TASK_FPU_SUPPORT == 1 )
	/* The task starts with interrupts enabled and no FPU context. */
	pxTopOfStack--;
	*pxTopOfStack = portNO_CRITICAL_NESTING;
	pxTopOfStack--;
	*pxTopOfStack = portNO_FLOATING_POINT_CONTEXT;
#elif ( configUSE_TASK_FPU_SUPPORT == 2 )
	/* Reserve and clear space for 32 128-bit Q registers. */
	pxTopOfStack -= portFPU_REGISTER_WORDS;
	memset(pxTopOfStack, 0x00, portFPU_REGISTER_WORDS * sizeof(StackType_t));

	pxTopOfStack--;
	*pxTopOfStack = portNO_CRITICAL_NESTING;
	pxTopOfStack--;
	*pxTopOfStack = pdTRUE;
	ullPortTaskHasFPUContext = pdTRUE;
#else
	#error "Invalid configUSE_TASK_FPU_SUPPORT setting."
#endif

	return pxTopOfStack;
}

/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler(void)
{
	uint32_t ulAPSR;

	__asm__ volatile ("MRS %0, CurrentEL" : "=r" (ulAPSR));
	ulAPSR &= portAPSR_MODE_BITS_MASK;

	/* Support EL1, EL2, and EL3 */
	if (ulAPSR == portEL1 || ulAPSR == 0x08 /* EL2 */ || ulAPSR == 0x0c /* EL3 */) {
		/* Enable GICv3 system register interface */
		write_icc_sre_el1(0x7);
		__asm__ volatile ("isb" ::: "memory");

		/* Set priority mask to allow all interrupts */
		write_icc_pmr_el1(portUNMASK_VALUE);

		/* Enable Group 1 interrupts */
		write_icc_igrpen1_el1(0x1);
		__asm__ volatile ("isb" ::: "memory");

		/* Disable interrupts while starting scheduler */
		portDISABLE_INTERRUPTS();

		/* Start the tick timer */
		configSETUP_TICK_INTERRUPT();

		/* Start the first task */
		vPortRestoreTaskContext();
	}

	return 0;
}

/*-----------------------------------------------------------*/

void vPortEndScheduler(void)
{
	configASSERT(ullCriticalNesting == 1000ULL);
}

/*-----------------------------------------------------------*/

void vPortEnterCritical(void)
{
	uxPortSetInterruptMask();
	ullCriticalNesting++;

	if (ullCriticalNesting == 1ULL) {
		configASSERT(ullPortInterruptNesting == 0);
	}
}

/*-----------------------------------------------------------*/

void vPortExitCritical(void)
{
	if (ullCriticalNesting > portNO_CRITICAL_NESTING) {
		ullCriticalNesting--;
		if (ullCriticalNesting == portNO_CRITICAL_NESTING) {
			vPortClearInterruptMask(pdFALSE);
		}
	}
}

/*-----------------------------------------------------------*/

static void portCLEAR_INTERRUPT_MASK_fn(void)
{
	portDISABLE_INTERRUPTS();
	write_icc_pmr_el1(portUNMASK_VALUE);
	__asm__ volatile ("dsb sy\n isb sy\n" ::: "memory");
	portENABLE_INTERRUPTS();
}

void FreeRTOS_Tick_Handler(void)
{
	/* Set interrupt mask before altering scheduler structures */
	portDISABLE_INTERRUPTS();
	write_icc_pmr_el1((uint32_t)(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT));
	__asm__ volatile ("dsb sy\n isb sy\n" ::: "memory");

	/* Clear tick interrupt source */
	configCLEAR_TICK_INTERRUPT();
	portENABLE_INTERRUPTS();

	/* Increment RTOS tick */
	if (xTaskIncrementTick() != pdFALSE) {
		ullPortYieldRequired = pdTRUE;
	}

	/* Restore interrupt mask */
	portCLEAR_INTERRUPT_MASK_fn();
}

/*-----------------------------------------------------------*/

void vPortClearInterruptMask(UBaseType_t uxNewMaskValue)
{
	if (uxNewMaskValue == pdFALSE) {
		portCLEAR_INTERRUPT_MASK_fn();
	}
}

/*-----------------------------------------------------------*/

UBaseType_t uxPortSetInterruptMask(void)
{
	uint32_t ulReturn;

	portDISABLE_INTERRUPTS();

	if (read_icc_pmr_el1() == (uint32_t)(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT)) {
		ulReturn = pdTRUE;
	} else {
		ulReturn = pdFALSE;
		write_icc_pmr_el1((uint32_t)(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT));
		__asm__ volatile ("dsb sy\n isb sy\n" ::: "memory");
	}

	portENABLE_INTERRUPTS();
	return ulReturn;
}

/*-----------------------------------------------------------*/

void vPortTaskUsesFPU(void)
{
	ullPortTaskHasFPUContext = pdTRUE;
}

/*-----------------------------------------------------------*/

#if (configASSERT_DEFINED == 1)
void vPortValidateInterruptPriority(void)
{
	configASSERT(read_icc_rpr_el1() >= (uint32_t)(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT));
	configASSERT((read_icc_bpr0_el1() & portBINARY_POINT_BITS) <= portMAX_BINARY_POINT_VALUE);
}
#endif
