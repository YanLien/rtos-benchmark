/* SPDX-License-Identifier: Apache-2.0 */

#ifndef GICV3_H_
#define GICV3_H_

#include <stdint.h>

/*
 * GIC-600 (GICv3) minimal driver for RK3588
 * Uses system register interface (ICC_*_EL1).
 */

void gicv3_init(void);
void gicv3_init_secondary(void);
void gicv3_enable_interrupt(uint32_t intid);
void gicv3_disable_interrupt(uint32_t intid);
void gicv3_set_priority(uint32_t intid, uint8_t priority);
void gicv3_eoi(uint32_t intid);
uint32_t gicv3_acknowledge_irq(void);

#endif /* GICV3_H_ */
