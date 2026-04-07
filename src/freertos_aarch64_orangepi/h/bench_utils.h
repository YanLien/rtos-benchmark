/* SPDX-License-Identifier: Apache-2.0 */

#ifndef _LATENCY_MEASURE_UNIT_H
#define _LATENCY_MEASURE_UNIT_H

#include <stdio.h>

#ifdef CSV_FORMAT_OUTPUT
#define FORMAT "%-50s,%8u,%8u\n"
#else
#define FORMAT "%-50s:%8u cycles , %8u ns\n"
#endif

#ifdef FREERTOS_AARCH64
#include "bench_porting_layer_aarch64.h"
#endif /* FREERTOS_AARCH64 */

struct bench_stats {
	bench_time_t avg;
	bench_time_t min;
	bench_time_t max;
	bench_time_t total;
};

void bench_stats_reset(struct bench_stats *stats);
void bench_stats_update(struct bench_stats *stats, bench_time_t value,
			uint32_t iteration);
void bench_stats_report_title(const char *title);
void bench_stats_report_line(const char *summary, const struct bench_stats *stats);
void bench_stats_report_na(const char *summary);

#endif
