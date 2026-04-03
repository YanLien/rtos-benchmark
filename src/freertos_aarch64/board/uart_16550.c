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
 * Simple printf implementation for benchmark output.
 * Supports: %d, %u, %x, %s, %c, %%
 */
#include "orange_pi_5.h"

static void print_num(uintptr_t uart_base, unsigned long val, int radix)
{
	char buf[20];
	int i = 0;

	if (val == 0) {
		uart_putc(uart_base, '0');
		return;
	}

	while (val > 0) {
		int rem = val % radix;
		buf[i++] = (rem < 10) ? ('0' + rem) : ('a' + rem - 10);
		val /= radix;
	}

	while (--i >= 0)
		uart_putc(uart_base, buf[i]);
}

static void print_long_val(uintptr_t uart_base, long val)
{
	if (val < 0) {
		uart_putc(uart_base, '-');
		val = -val;
	}
	print_num(uart_base, (unsigned long)val, 10);
}

static void print_ulong_val(uintptr_t uart_base, unsigned long val)
{
	print_num(uart_base, val, 10);
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
		switch (*fmt) {
		case 'd':
			print_long_val(uart_base, __builtin_va_arg(ap, long));
			break;
		case 'u':
			print_ulong_val(uart_base, __builtin_va_arg(ap, unsigned long));
			break;
		case 'x':
			print_num(uart_base, __builtin_va_arg(ap, unsigned long), 16);
			break;
		case 's': {
			const char *s = __builtin_va_arg(ap, const char *);
			if (s)
				uart_puts(uart_base, s);
			else
				uart_puts(uart_base, "(null)");
			break;
		}
		case 'c':
			uart_putc(uart_base, (char)__builtin_va_arg(ap, int));
			break;
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
