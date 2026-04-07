/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Bare-metal support functions for FreeRTOS on RK3588.
 * Provides memset, memcpy, memmove, and assert handler.
 * main() is provided by bench_all.c when RTOS_HAS_MAIN_ENTRY_POINT is set.
 */

#include <stddef.h>
#include <stdint.h>

/* ---- Bare-metal memory functions ---- */

void *memset(void *s, int c, size_t n)
{
	unsigned char *p = (unsigned char *)s;
	while (n--)
		*p++ = (unsigned char)c;
	return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;
	while (n--)
		*d++ = *s++;
	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;

	if (d < s) {
		while (n--)
			*d++ = *s++;
	} else {
		d += n;
		s += n;
		while (n--)
			*--d = *--s;
	}
	return dest;
}

/* ---- assert handler ---- */

void __assert_fail(const char *expr, const char *file, int line, const char *func)
{
	extern int uart_printf(const char *fmt, ...);
	uart_printf("ASSERT FAILED: %s\n  at %s:%d in %s\n", expr, file, line, func);
	for (;;)
		;
}

void vPortUnexpectedSyncException(uint64_t esr, uint64_t elr,
				  uint64_t far, uint64_t spsr)
{
	extern int uart_printf(const char *fmt, ...);

	uart_printf("\r\nUNEXPECTED SYNC EXCEPTION\r\n");
	uart_printf("  ESR_EL1 = 0x%llx\r\n", esr);
	uart_printf("  ELR_EL1 = 0x%llx\r\n", elr);
	uart_printf("  FAR_EL1 = 0x%llx\r\n", far);
	uart_printf("  SPSR_EL1 = 0x%llx\r\n", spsr);

	for (;;)
		;
}
