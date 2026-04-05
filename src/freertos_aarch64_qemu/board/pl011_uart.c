/* SPDX-License-Identifier: Apache-2.0 */

#include "pl011_uart.h"

/*
 * Minimal PL011 UART driver for QEMU virt machine.
 * Only TX is needed for benchmark output.
 */

/* PL011 register offsets */
#define UARTDR      0x00
#define UARTFR      0x18
#define UARTIBRD    0x24
#define UARTFBRD    0x28
#define UARTLCR_H   0x2C
#define UARTCR      0x30
#define UARTIMSC    0x38

/* Flag register bits */
#define FR_TXFF     (1 << 5)   /* Transmit FIFO full */

/* Control register bits */
#define CR_UARTEN   (1 << 0)   /* UART enable */
#define CR_TXE      (1 << 8)   /* Transmit enable */

/* Line control bits */
#define LCR_H_FEN   (1 << 4)   /* Enable FIFOs */
#define LCR_H_8N1   0x60       /* 8 data bits */

static inline void write32(uintptr_t addr, uint32_t val)
{
	*(volatile uint32_t *)addr = val;
}

static inline uint32_t read32(uintptr_t addr)
{
	return *(volatile uint32_t *)addr;
}

void uart_init(uintptr_t base, uint32_t baudrate, uint32_t clk_freq)
{
	/* Disable UART */
	write32(base + UARTCR, 0);

	/* Wait for TX FIFO empty */
	while (read32(base + UARTFR) & FR_TXFF)
		;

	/* Disable FIFOs */
	write32(base + UARTLCR_H, 0);

	/* Set baud rate divisor */
	if (baudrate > 0 && clk_freq > 0) {
		uint32_t divider = clk_freq / (16 * baudrate);
		uint32_t remainder = clk_freq % (16 * baudrate);
		uint32_t fraction = ((8 * remainder) / baudrate + 1) / 2;
		write32(base + UARTIBRD, divider & 0xFFFF);
		write32(base + UARTFBRD, fraction & 0x3F);
	}

	/* 8 data bits, no parity, 1 stop bit, enable FIFOs */
	write32(base + UARTLCR_H, LCR_H_8N1 | LCR_H_FEN);

	/* Disable all interrupts */
	write32(base + UARTIMSC, 0);

	/* Enable UART, TX only */
	write32(base + UARTCR, CR_UARTEN | CR_TXE);
}

void uart_putc(uintptr_t base, char ch)
{
	while (read32(base + UARTFR) & FR_TXFF)
		;
	write32(base + UARTDR, (uint32_t)ch);
}

void uart_puts(uintptr_t base, const char *str)
{
	while (*str) {
		if (*str == '\n')
			uart_putc(base, '\r');
		uart_putc(base, *str++);
	}
}

/*
 * Minimal printf implementation for benchmark output.
 * Supports:
 *   %d %u %x %s %c %%
 *   field width (for example %6llu, %-40s)
 *   l / ll length modifiers
 */
#include "qemu_virt.h"

static void uart_put_repeat(uintptr_t uart_base, char ch, int count)
{
	while (count-- > 0)
		uart_putc(uart_base, ch);
}

static int str_len(const char *str)
{
	int len = 0;

	while (str && str[len])
		len++;

	return len;
}

static int format_unsigned(char *buf, unsigned long long val, int radix)
{
	static const char digits[] = "0123456789abcdef";
	int i = 0;

	if (val == 0ULL) {
		buf[i++] = '0';
		return i;
	}

	while (val > 0ULL) {
		buf[i++] = digits[val % (unsigned long long)radix];
		val /= radix;
	}

	return i;
}

static void print_unsigned(uintptr_t uart_base, unsigned long long val,
			   int radix, int width, int left_align)
{
	char buf[32];
	int len = format_unsigned(buf, val, radix);
	int pad = (width > len) ? (width - len) : 0;

	if (!left_align)
		uart_put_repeat(uart_base, ' ', pad);

	while (--len >= 0)
		uart_putc(uart_base, buf[len]);

	if (left_align)
		uart_put_repeat(uart_base, ' ', pad);
}

static void print_signed(uintptr_t uart_base, long long val,
			 int width, int left_align)
{
	unsigned long long abs_val;
	int negative = 0;
	char buf[32];
	int len;
	int pad;

	if (val < 0) {
		negative = 1;
		abs_val = (unsigned long long)(-(val + 1)) + 1ULL;
	} else {
		abs_val = (unsigned long long)val;
	}

	len = format_unsigned(buf, abs_val, 10) + negative;
	pad = (width > len) ? (width - len) : 0;

	if (!left_align)
		uart_put_repeat(uart_base, ' ', pad);

	if (negative)
		uart_putc(uart_base, '-');

	len = format_unsigned(buf, abs_val, 10);
	while (--len >= 0)
		uart_putc(uart_base, buf[len]);

	if (left_align)
		uart_put_repeat(uart_base, ' ', pad);
}

static void print_string(uintptr_t uart_base, const char *str,
			 int width, int left_align)
{
	int len;
	int pad;

	if (!str)
		str = "(null)";

	len = str_len(str);
	pad = (width > len) ? (width - len) : 0;

	if (!left_align)
		uart_put_repeat(uart_base, ' ', pad);

	uart_puts(uart_base, str);

	if (left_align)
		uart_put_repeat(uart_base, ' ', pad);
}

int uart_printf(const char *fmt, ...)
{
	__builtin_va_list ap;
	uintptr_t uart_base = UART2_BASE;

	__builtin_va_start(ap, fmt);

	while (*fmt) {
		if (*fmt != '%') {
			if (*fmt == '\n')
				uart_putc(uart_base, '\r');
			uart_putc(uart_base, *fmt++);
			continue;
		}

		fmt++; /* skip '%' */

		int left_align = 0;
		int width = 0;
		int length = 0;

		if (*fmt == '-') {
			left_align = 1;
			fmt++;
		}

		while (*fmt >= '0' && *fmt <= '9') {
			width = (width * 10) + (*fmt - '0');
			fmt++;
		}

		while (*fmt == 'l') {
			length++;
			fmt++;
		}

		switch (*fmt) {
		case 'd':
			if (length >= 2)
				print_signed(uart_base, __builtin_va_arg(ap, long long),
					     width, left_align);
			else if (length == 1)
				print_signed(uart_base, __builtin_va_arg(ap, long),
					     width, left_align);
			else
				print_signed(uart_base, __builtin_va_arg(ap, int),
					     width, left_align);
			break;
		case 'u':
			if (length >= 2)
				print_unsigned(uart_base,
					      __builtin_va_arg(ap, unsigned long long),
					      10, width, left_align);
			else if (length == 1)
				print_unsigned(uart_base,
					      __builtin_va_arg(ap, unsigned long),
					      10, width, left_align);
			else
				print_unsigned(uart_base,
					      __builtin_va_arg(ap, unsigned int),
					      10, width, left_align);
			break;
		case 'x':
			if (length >= 2)
				print_unsigned(uart_base,
					      __builtin_va_arg(ap, unsigned long long),
					      16, width, left_align);
			else if (length == 1)
				print_unsigned(uart_base,
					      __builtin_va_arg(ap, unsigned long),
					      16, width, left_align);
			else
				print_unsigned(uart_base,
					      __builtin_va_arg(ap, unsigned int),
					      16, width, left_align);
			break;
		case 's': {
			const char *s = __builtin_va_arg(ap, const char *);
			print_string(uart_base, s, width, left_align);
			break;
		}
		case 'c': {
			char ch = (char)__builtin_va_arg(ap, int);
			if (!left_align && width > 1)
				uart_put_repeat(uart_base, ' ', width - 1);
			uart_putc(uart_base, ch);
			if (left_align && width > 1)
				uart_put_repeat(uart_base, ' ', width - 1);
			break;
		}
		case '%':
			uart_putc(uart_base, '%');
			break;
		default:
			uart_putc(uart_base, '%');
			uart_putc(uart_base, *fmt);
			break;
		}
		fmt++;
	}

	__builtin_va_end(ap);
	return 0;
}
