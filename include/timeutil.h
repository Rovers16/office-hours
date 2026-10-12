/*
 * timeutil.h - converting days and times between text and numbers.
 */
#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <stddef.h>

/* "09:30" or "9:30" (24-hour) -> 570. Returns NO_TIME for "" and -2 for bad input. */
int parse_time(const char *text);

/* 570 -> "9:30 AM", 780 -> "1:00 PM". */
void format_time(int minutes, char *buf, size_t n);

/* "Monday" / "monday" / "Mon" -> 0 ... "Sunday" -> 6, or -1 if unknown. */
int parse_day(const char *text);

/* 0 -> "Monday". */
const char *day_name(int day);

/* "Tuesday,Thursday" -> bit mask. Returns -1 if any day is unknown. */
int parse_day_list(const char *text, unsigned *mask);

/* Bit mask -> "Monday & Wednesday" or "Monday, Wednesday & Friday". */
void format_day_list(unsigned mask, char *buf, size_t n);

#endif /* TIMEUTIL_H */
