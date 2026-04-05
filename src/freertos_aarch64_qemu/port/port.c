/*
 * FreeRTOS AArch64 SMP port for QEMU virt (Cortex-A55, GICv3)
 *
 * Adapted from FreeRTOS-Kernel portable/GCC/ARM_AARCH64/port.c
 * and portable/GCC/ARM_CR82/port.c for SMP support.
 * Uses GICv3 ICC_*_EL1 system registers and SGI for IPI.
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

static inline void write_icc_sgi1r_el1(uint64_t val)
{
	__asm__ volatile ("msr ICC_SGI1R_EL1, %0" :: "r" (val) : "memory");
	__asm__ volatile ("isb" ::: "memory");
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

/*-----------------------------------------------------------
 * Per-core state arrays (SMP)
 *----------------------------------------------------------*/

#if ( configNUMBER_OF_CORES == 1 )
	volatile uint64_t ullCriticalNesting = 9999ULL;
	uint64_t ullPortTaskHasFPUContext = pdFALSE;
	volatile uint64_t ullPortYieldRequired = pdFALSE;
	volatile uint64_t ullPortInterruptNesting = 0;
#else
	volatile uint64_t ullCriticalNestings[ configNUMBER_OF_CORES ];
	volatile uint64_t ullPortTaskHasFPUContext[ configNUMBER_OF_CORES ];
	volatile uint64_t ullPortYieldRequired[ configNUMBER_OF_CORES ];
	volatile uint64_t ullPortInterruptNestings[ configNUMBER_OF_CORES ];
#endif

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
	#if ( configNUMBER_OF_CORES == 1 )
		ullPortTaskHasFPUContext = pdTRUE;
	#else
		ullPortTaskHasFPUContext[ 0 ] = pdTRUE;
	#endif
#else
	#error "Invalid configUSE_TASK_FPU_SUPPORT setting."
#endif

	return pxTopOfStack;
}

/*-----------------------------------------------------------*/

#if ( configNUMBER_OF_CORES == 1 )

/*-----------------------------------------------------------
 * Single-core scheduler start
 *----------------------------------------------------------*/

BaseType_t xPortStartScheduler(void)
{
	uint32_t ulAPSR;

	__asm__ volatile ("MRS %0, CurrentEL" : "=r" (ulAPSR));
	ulAPSR &= portAPSR_MODE_BITS_MASK;

	if (ulAPSR == portEL1 || ulAPSR == 0x08 || ulAPSR == 0x0c) {
		write_icc_sre_el1(0x7);
		__asm__ volatile ("isb" ::: "memory");
		write_icc_pmr_el1(portUNMASK_VALUE);
		write_icc_igrpen1_el1(0x1);
		__asm__ volatile ("isb" ::: "memory");
		portDISABLE_INTERRUPTS();
		configSETUP_TICK_INTERRUPT();
		vPortRestoreTaskContext();
	}

	return 0;
}

#else /* configNUMBER_OF_CORES > 1 */

/*-----------------------------------------------------------
 * SMP spinlock and IPI implementation
 *----------------------------------------------------------*/

/* Spinlock gate words (one per lock type) */
PRIVILEGED_DATA static volatile uint32_t ulGateWord[ eLockCount ];

/* Which core owns which lock (bitmask per core) */
PRIVILEGED_DATA static volatile uint64_t ullOwnedByCore[ portMAX_CORE_COUNT ];

/* Recursion count per lock */
PRIVILEGED_DATA static volatile uint64_t ullRecursionCountByLock[ eLockCount ];

/* Secondary core ready flag (set from startup_aarch64.S) */
volatile uint8_t ucSecondaryCoresReadyFlags = 0;

/* Per-core scheduler started flag. Primary sets before secondary
 * calls vPortRestoreTaskContext. */
volatile uint8_t ucSchedulerStartedFlags = 0;

static inline void prvSpinUnlock(volatile uint32_t *ulLock)
{
	__asm__ volatile (
		"dmb sy         \n"
		"mov w1, #0     \n"
		"str w1, [%x0]  \n"
		"sev            \n"
		"dsb sy         \n"
		"isb sy         \n"
		:
		: "r" (ulLock)
		: "memory", "w1"
	);
}

static inline uint32_t prvSpinTrylock(volatile uint32_t *ulLock)
{
	register uint32_t ulRet;
	__asm__ volatile (
		"1:                     \n"
		"ldxr w1, [%x1]         \n"
		"cbnz w1, 2f            \n"
		"mov  w2, #1            \n"
		"stxr w3, w2, [%x1]     \n"
		"cbnz w3, 1b            \n"
		"dmb  sy                \n"
		"mov %w0, #0            \n"
		"b    3f                \n"
		"2:                     \n"
		"clrex                  \n"
		"mov %w0, #1            \n"
		"3:                     \n"
		: "=r" (ulRet)
		: "r" (ulLock)
		: "memory", "w1", "w2", "w3"
	);
	return ulRet;
}

static inline uint64_t prvGet64(volatile uint64_t *x)
{
	__asm__ volatile ("dsb sy" ::: "memory");
	return *x;
}

static inline void prvSet64(volatile uint64_t *x, uint64_t value)
{
	*x = value;
	__asm__ volatile ("dsb sy" ::: "memory");
}

void vPortRecursiveLock(uint8_t ucCoreID, ePortRTOSLock eLockNum, BaseType_t uxAcquire)
{
	configASSERT(ucCoreID < portMAX_CORE_COUNT);
	configASSERT(eLockNum < eLockCount);

	uint32_t ulLockBit = 1u << eLockNum;

	if (uxAcquire) {
		if (prvSpinTrylock(&ulGateWord[eLockNum]) != 0) {
			/* Check if this core already owns the lock (recursive) */
			if (prvGet64(&ullOwnedByCore[ucCoreID]) & ulLockBit) {
				prvSet64(&ullRecursionCountByLock[eLockNum],
					prvGet64(&ullRecursionCountByLock[eLockNum]) + 1);
				return;
			}

			/* Spin waiting for the lock */
			while (prvSpinTrylock(&ulGateWord[eLockNum]) != 0) {
				__asm__ volatile (
					"sevl            \n"
					"1: wfe          \n"
					"ldr w2, [%x0]   \n"
					"cbnz w2, 1b     \n"
					:
					: "r" (&ulGateWord[eLockNum])
					: "memory", "w2"
				);
			}
		}

		__asm__ __volatile__ ("dmb sy" ::: "memory");

		prvSet64(&ullRecursionCountByLock[eLockNum], 1);
		prvSet64(&ullOwnedByCore[ucCoreID],
			prvGet64(&ullOwnedByCore[ucCoreID]) | ulLockBit);
	} else {
		/* Release */
		prvSet64(&ullRecursionCountByLock[eLockNum],
			prvGet64(&ullRecursionCountByLock[eLockNum]) - 1);

		if (!prvGet64(&ullRecursionCountByLock[eLockNum])) {
			prvSet64(&ullOwnedByCore[ucCoreID],
				prvGet64(&ullOwnedByCore[ucCoreID]) & ~ulLockBit);
			prvSpinUnlock(&ulGateWord[eLockNum]);
			__asm__ __volatile__ ("dmb sy" ::: "memory");
		}
	}
}

/*-----------------------------------------------------------*/

void vInterruptCore(uint32_t ulInterruptID, uint8_t ucCoreID)
{
	uint64_t ulRegVal = 0;
	uint32_t ulCoreMask = (1UL << ucCoreID);
	ulRegVal |= ((ulCoreMask & 0xFFFF) | ((ulInterruptID & 0xF) << 24U));
	write_icc_sgi1r_el1(ulRegVal);
}

/*-----------------------------------------------------------*/

uint8_t ucPortGetCoreID(void)
{
	/* Direct MPIDR_EL1 read (EL1 only, no SVC needed for bare metal) */
	uint64_t ullMpidrEl1;
	__asm__ volatile ("mrs %0, MPIDR_EL1" : "=r" (ullMpidrEl1));
	return (uint8_t)(ullMpidrEl1 & 0xff);
}

uint8_t ucPortGetCoreIDFromIsr(void)
{
	uint64_t ullMpidrEl1;
	__asm__ volatile ("mrs %0, MPIDR_EL1" : "=r" (ullMpidrEl1));
	return (uint8_t)(ullMpidrEl1 & 0xff);
}

/*-----------------------------------------------------------
 * SGI handler - called when another core sends an SGI yield
 *-----------------------------------------------------------*/

void FreeRTOS_SGI_Handler(void)
{
	UBaseType_t uxInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	UBaseType_t uxSavedInterruptStatus = portENTER_CRITICAL_FROM_ISR();

	ullPortYieldRequired[portGET_CORE_ID_FROM_ISR()] = pdTRUE;

	portEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
	portCLEAR_INTERRUPT_MASK_FROM_ISR(uxInterruptStatus);
}

/*-----------------------------------------------------------
 * Secondary core entry - called from startup_aarch64.S
 *-----------------------------------------------------------*/

void vPortSecondaryEntry(void)
{
	uint8_t coreID = ucPortGetCoreID();

	/* Disable interrupts while entering scheduler */
	portDISABLE_INTERRUPTS();

	/* Set initial critical nesting for this core */
	ullCriticalNestings[coreID] = 9999ULL;
	ullPortTaskHasFPUContext[coreID] = pdFALSE;
	ullPortYieldRequired[coreID] = pdFALSE;
	ullPortInterruptNestings[coreID] = 0;

	/* Initialize per-core GIC Redistributor */
	extern void gicv3_init_secondary(void);
	gicv3_init_secondary();

	/* Wait for the primary core's scheduler to set up our pxCurrentTCBs[].
	 * The primary sets the bit after vTaskStartScheduler has prepared all cores. */
	while (!(ucSchedulerStartedFlags & (1U << coreID))) {
		__asm__ volatile ("wfe" ::: "memory");
	}

	/* Setup the tick timer on this core */
	configSETUP_TICK_INTERRUPT();

	/* Start running tasks on this core */
	vPortRestoreTaskContext();

	/* Should never return */
	for (;;) {
		__asm__ volatile ("wfi" ::: "memory");
	}
}

/*-----------------------------------------------------------
 * Primary core scheduler start (SMP)
 *-----------------------------------------------------------*/

BaseType_t xPortStartScheduler(void)
{
	uint32_t ulAPSR;
	uint8_t coreID;

	__asm__ volatile ("MRS %0, CurrentEL" : "=r" (ulAPSR));
	ulAPSR &= portAPSR_MODE_BITS_MASK;

	if (ulAPSR == portEL1 || ulAPSR == 0x08 || ulAPSR == 0x0c) {
		/* Enable GICv3 system register interface */
		write_icc_sre_el1(0x7);
		__asm__ volatile ("isb" ::: "memory");
		write_icc_pmr_el1(portUNMASK_VALUE);
		write_icc_igrpen1_el1(0x1);
		__asm__ volatile ("isb" ::: "memory");

		/* Initialize primary core state */
		coreID = ucPortGetCoreID();
		ullCriticalNestings[coreID] = 9999ULL;
		ullPortTaskHasFPUContext[coreID] = pdFALSE;
		ullPortYieldRequired[coreID] = pdFALSE;
		ullPortInterruptNestings[coreID] = 0;

		portDISABLE_INTERRUPTS();

		/* Setup the tick timer */
		configSETUP_TICK_INTERRUPT();

		/* Signal all secondary cores that the scheduler is ready.
		 * pxCurrentTCBs[] has been populated by vTaskStartScheduler. */
		ucSchedulerStartedFlags = (1U << configNUMBER_OF_CORES) - 1;
		__asm__ volatile ("dsb sy; sev" ::: "memory");

		/* Start the first task on primary core */
		vPortRestoreTaskContext();
	}

	return 0;
}

#endif /* configNUMBER_OF_CORES > 1 */

/*-----------------------------------------------------------*/

void vPortEndScheduler(void)
{
#if ( configNUMBER_OF_CORES == 1 )
	configASSERT(ullCriticalNesting == 1000ULL);
#else
	configASSERT(ullCriticalNestings[portGET_CORE_ID()] == 1000ULL);
#endif
}

/*-----------------------------------------------------------*/

#if ( configNUMBER_OF_CORES == 1 )

void vPortEnterCritical(void)
{
	uxPortSetInterruptMask();
	ullCriticalNesting++;

	if (ullCriticalNesting == 1ULL) {
		configASSERT(ullPortInterruptNesting == 0);
	}
}

void vPortExitCritical(void)
{
	if (ullCriticalNesting > portNO_CRITICAL_NESTING) {
		ullCriticalNesting--;
		if (ullCriticalNesting == portNO_CRITICAL_NESTING) {
			vPortClearInterruptMask(pdFALSE);
		}
	}
}

#endif /* configNUMBER_OF_CORES == 1 */

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
	portDISABLE_INTERRUPTS();
	write_icc_pmr_el1((uint32_t)(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT));
	__asm__ volatile ("dsb sy\n isb sy\n" ::: "memory");

	configCLEAR_TICK_INTERRUPT();
	portENABLE_INTERRUPTS();

	if (xTaskIncrementTick() != pdFALSE) {
#if ( configNUMBER_OF_CORES == 1 )
		ullPortYieldRequired = pdTRUE;
#else
		ullPortYieldRequired[portGET_CORE_ID()] = pdTRUE;
#endif
	}

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
#if ( configNUMBER_OF_CORES == 1 )
	ullPortTaskHasFPUContext = pdTRUE;
#else
	ullPortTaskHasFPUContext[portGET_CORE_ID()] = pdTRUE;
#endif
}

/*-----------------------------------------------------------*/

#if (configASSERT_DEFINED == 1)
void vPortValidateInterruptPriority(void)
{
	configASSERT(read_icc_rpr_el1() >= (uint32_t)(configMAX_API_CALL_INTERRUPT_PRIORITY << portPRIORITY_SHIFT));
	configASSERT((read_icc_bpr0_el1() & portBINARY_POINT_BITS) <= portMAX_BINARY_POINT_VALUE);
}
#endif
