#ifndef TIMESPEC_H
#define TIMESPEC_H

#include <pthread.h>

static inline bool operator<=(const struct timespec &tv1, const struct timespec &tv2) {
	if(tv1.tv_sec < tv2.tv_sec)
		return true;
	if(tv1.tv_sec > tv2.tv_sec)
		return false;
	return tv1.tv_nsec <= tv2.tv_nsec;
}

static inline bool operator>=(const struct timespec &tv1, const struct timespec &tv2) {
	if(tv1.tv_sec < tv2.tv_sec)
		return false;
	if(tv1.tv_sec > tv2.tv_sec)
		return true;
	return tv1.tv_nsec >= tv2.tv_nsec;
}

static inline bool operator<(const struct timespec &tv1, const struct timespec &tv2) {
	return ! (tv1 >= tv2);
}

static inline bool operator>(const struct timespec &tv1, const struct timespec &tv2) {
	return ! (tv1 <= tv2);
}

static inline bool operator==(const struct timespec &tv1, const struct timespec &tv2) {
	return tv1.tv_sec == tv2.tv_sec && tv1.tv_nsec == tv2.tv_nsec;
}

static inline struct timespec &operator-=(struct timespec &later, const struct timespec &earlier) {
	later.tv_sec -= earlier.tv_sec;
	later.tv_nsec -= earlier.tv_nsec;
	if(later.tv_nsec < 0) {
		later.tv_nsec += 1000000000;
		later.tv_sec--;
	}
	return later;
}

static inline struct timespec operator-(const struct timespec &later,
										const struct timespec &earlier) {
	struct timespec diff = later;
	diff -= earlier;
	return diff;
}

static inline long to_ms(const struct timespec &ts) {
	return ts.tv_sec * 1000 +
		ts.tv_nsec / 1000000;
}

static inline struct timespec &operator+=(struct timespec &ts, const long interval) {
	ts.tv_sec += interval / 1000;
	ts.tv_nsec += (interval % 1000) * 1000000;
	if(ts.tv_nsec > 1000000000)
	{
		ts.tv_nsec -= 1000000000;
		ts.tv_sec++;
	}
	return ts;
}

#endif // TIMESPEC_H
