/* SPDX-License-Identifier: Apache-2.0 */

#include "uart_16550.h"

/*
 * Minimal 16550 UART driver for RK3588 (Orange Pi 5)
 * Only TX is needed for benchmark output.
 */

/* 16550 register offsets */
#define UART_THR    0x00  /* Transmit Holding Register (write) */
#define UART_RBR    0x00  /* Receive Buffer Register (read) */
#define UART_IER    0x04  /* Interrupt Enable Register */
#define UART_FCR    0x08  /* FIFO Control Register (write) */
#define UART_LCR    0x0C  /* Line Control Register */
#define UART_MCR    0x10  /* Modem Control Register */
#define UART_LSR    0x14  /* Line Status Register */
#define UART_MSR    0x18  /* Modem Status Register */
#define UART_SCR    0x1C  /* Scratch Register */

/* LSR bits */
#define LSR_THRE    (1 << 5)  /* Transmit Holding Register Empty */
#define LSR_DR      (1 << 0)  /* Data Ready */

/* LCR bits */
#define LCR_DLAB    (1 << 7)  /* Divisor Latch Access Bit */
#define LCR_8N1     0x03      /* 8 data bits, no parity, 1 stop bit */

/* FCR bits */
#define FCR_ENABLE  0x01      /* Enable FIFOs */
#define FCR_CLEAR  0x06       /* Clear both FIFOs */

static inline void uart_write_reg(uintptr_t base, uint32_t offset, uint8_t val)
{
	*(volatile uint8_t *)(base + offset) = val;
}

static inline uint8_t uart_read_reg(uintptr_t base, uint32_t offset)
{
	return *(volatile uint8_t *)(base + offset);
}

void uart_init(uintptr_t base, uint32_t baudrate, uint32_t clk_freq)
{
	uint16_t divisor = (uint16_t)(clk_freq / (16 * baudrate));

	/* Disable interrupts */
	uart_write_reg(base, UART_IER, 0x00);

	/* Enable DLAB to set baud rate */
	uart_write_reg(base, UART_LCR, LCR_DLAB);

	/* Set divisor */
	uart_write_reg(base, 0x00, (uint8_t)(divisor & 0xFF));       /* DLL */
	uart_write_reg(base, 0x04, (uint8_t)((divisor >> 8) & 0xFF)); /* DLH */

	/* 8 data bits, no parity, 1 stop bit, clear DLAB */
	uart_write_reg(base, UART_LCR, LCR_8N1);

	/* Enable FIFOs and clear them */
	uart_write_reg(base, UART_FCR, FCR_ENABLE | FCR_CLEAR);

	/* Set DTR + RTS */
	uart_write_reg(base, UART_MCR, 0x03);
}

void uart_putc(uintptr_t base, char ch)
{
	while (!(uart_read_reg(base, UART_LSR) & LSR_THRE))
		;
	uart_write_reg(base, UART_THR, (uint8_t)ch);
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
#include "board_config.h"

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
