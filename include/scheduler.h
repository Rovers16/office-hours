/*
 * scheduler.h - matching professors to courses and building office hours.
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdio.h>
#include "model.h"

/* Rules used when office hours are generated (all values in minutes). */
typedef struct {
    int after_class;    /* length of a block placed right after a class        */
    int before_class;   /* length of a block placed right before a class       */
    int min_block;      /* shorter free gaps are not used                      */
    int day_start;      /* office hours never start before this time           */
    int day_end;        /* ... or end after it                                 */
} Policy;

/* 135 min after class, 120 min before, 30 min minimum, 8:00 AM - 5:00 PM. */
Policy default_policy(void);

/* Fill in Professor.teaching for every professor. Returns how many courses
 * in the schedule are not taught by anyone (they are reported as warnings). */
size_t match_courses(Faculty *faculty, const Schedule *schedule, FILE *warnings);

/*
 * Give every professor office hours.
 *  - If the lecturer supplied office hours in the faculty file they are kept,
 *    and any that clash with the lecturer's own classes are reported.
 *  - Otherwise, for every class meeting, a block is placed right after the
 *    class, or right before it if there is no room after it, never
 *    overlapping a class or another block.
 * Returns the number of problems written to `warnings`.
 */
int assign_office_hours(Faculty *faculty, const Schedule *schedule,
                        const Policy *policy, FILE *warnings);

/* Sort a professor's office hours by day, then start time. */
void sort_office_hours(Professor *p);

#endif /* SCHEDULER_H */
