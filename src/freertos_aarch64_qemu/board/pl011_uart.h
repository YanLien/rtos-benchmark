/* SPDX-License-Identifier: Apache-2.0 */

#ifndef PL011_UART_H_
#define PL011_UART_H_

#include <stdint.h>
#include <stddef.h>

void uart_init(uintptr_t base, uint32_t baudrate, uint32_t clk_freq);
void uart_putc(uintptr_t base, char ch);
void uart_puts(uintptr_t base, const char *str);

/* Printf-like output to UART for benchmark results */
int uart_printf(const char *fmt, ...);

#endif /* PL011_UART_H_ */
