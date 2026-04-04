#include "bench_utils.h"

#include "bench_api.h"

#include <assert.h>
#include <stdint.h>

static uint32_t summary_len(const char *summary)
{
	uint32_t len = 0;

	while (summary && summary[len] != '\0')
		len++;

	return len;
}

static void bench_stats_report_prefix(const char *summary)
{
	uint32_t len = summary_len(summary);

	PRINTF(" %s", summary);
	while (len++ < 40U)
		PRINTF(" ");
	PRINTF(": ");
}

void bench_stats_reset(struct bench_stats *stats)
{
	stats->avg = 0;
	stats->min = (bench_time_t) -1;
	stats->max = 0;
	stats->total = 0;
}

void bench_stats_update(struct bench_stats *stats, bench_time_t value,
			uint32_t iteration)
{
	assert(iteration != 0);

	if (value < stats->min)
		stats->min = value;

	if (value > stats->max)
		stats->max = value;

	stats->total += value;
	stats->avg = stats->total / iteration;
}

void bench_stats_report_title(const char *title)
{
	PRINTF("** %s [avg, min, max] in nanoseconds **\n\r", title);
}

void bench_stats_report_line(const char *summary, const struct bench_stats *stats)
{
	bench_stats_report_prefix(summary);
	PRINTF("%u, %u, %u\n\r",
	       (unsigned int)bench_timing_cycles_to_ns(stats->avg),
	       (unsigned int)bench_timing_cycles_to_ns(stats->min),
	       (unsigned int)bench_timing_cycles_to_ns(stats->max));
}

void bench_stats_report_na(const char *summary)
{
	bench_stats_report_prefix(summary);
	PRINTF("n/a, n/a, n/a\n\r");
}

__weak void bench_collect_resources(void)
{
	// NO-Op
}
