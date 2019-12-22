#include "statistics.h"
#include "llist.h"

static const char* UNITS[] = {
	"",
	"k",
	"M",
	"G",
	"T"
};

void statistics_update(struct statistics* stats, struct net* net) {
	int i = net->num_threads;
	struct timespec now;
	uint64_t bytes_prev = stats->num_bytes;

	clock_gettime(CLOCK_MONOTONIC, &now);
	while(i-- > 0) {
		struct net_thread* thread = &net->threads[i];
		struct llist* threadlist = thread->threadlist;
		struct llist_entry* cursor;

		if(thread->initialized) {
			llist_lock(threadlist);
			llist_for_each(threadlist, cursor) {
				struct net_connection_thread* conn_thread = llist_entry_get_value(cursor, struct net_connection_thread, list);
				stats->num_bytes += conn_thread->byte_count;
				conn_thread->byte_count = 0;
			}
			llist_unlock(threadlist);
		}
	}

	stats->bytes_per_second[stats->average_index++] = (stats->num_bytes - bytes_prev) * 1000000000UL / get_timespec_diff(&now, &stats->last_update);
	stats->average_index %= STATISTICS_NUM_AVERAGES;
	stats->last_update = now;
}

#define GET_AVERAGE(var, stats, field) \
	do { \
		int i_; \
		(var) = 0; \
		for(i_ = 0; i_ < STATISTICS_NUM_AVERAGES; i_++) { \
			(var) += ((stats)->field)[i_]; \
		} \
		(var) /= STATISTICS_NUM_AVERAGES; \
	} while(0)

#define DEFINE_UNIT(base) \
	static const char* value_get_unit_##base(uint64_t value) { \
		int i = 0; \
		while(value > base) { \
			if(i == ARRAY_LEN(UNITS) - 1) { \
				break; \
			} \
			i++; \
			value /= base; \
		} \
		return UNITS[i]; \
	} \
	static double value_get_scaled_##base(uint64_t value) { \
		double val = value; \
		int i = 0; \
		while(val > base) { \
			i++; \
			if(i >= ARRAY_LEN(UNITS)) { \
				break; \
			} \
			val /= base; \
		} \
		return val; \
	}

DEFINE_UNIT(1024);
DEFINE_UNIT(1000);

const char* statistics_traffic_get_unit(struct statistics* stats) {
	return value_get_unit_1024(stats->num_bytes);
}

double statistics_traffic_get_scaled(struct statistics* stats) {
	return value_get_scaled_1024(stats->num_bytes);
}

const char* statistics_throughput_get_unit(struct statistics* stats) {
	uint64_t bytes_per_second;
	GET_AVERAGE(bytes_per_second, stats, bytes_per_second);
	return value_get_unit_1000(bytes_per_second * 8ULL);
}

double statistics_throughput_get_scaled(struct statistics* stats) {
	uint64_t bytes_per_second;
	GET_AVERAGE(bytes_per_second, stats, bytes_per_second);
	return value_get_scaled_1000(bytes_per_second * 8ULL);
}