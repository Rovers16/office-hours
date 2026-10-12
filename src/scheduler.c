/*
 * scheduler.c - matching professors to courses and building office hours.
 */
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"
#include "timeutil.h"

Policy default_policy(void)
{
    Policy p;
    p.after_class = 135;        /* 2 h 15 min, e.g. 10:45 AM - 1:00 PM */
    p.before_class = 120;       /* 2 h, e.g. 3:00 PM - 5:00 PM before an evening class */
    p.min_block = 30;
    p.day_start = 8 * 60;
    p.day_end = 17 * 60;
    return p;
}

/* ---- matching ---------------------------------------------------------- */

static int lists_course(const Professor *p, const char *code)
{
    for (size_t i = 0; i < p->n_course_codes; i++)
        if (same_course_code(p->course_codes[i], code))
            return 1;
    return 0;
}

/*
 * A professor teaches a course section when the syllabus names them as the
 * lecturer, or when the faculty file lists the course code and the syllabus
 * does not name a different lecturer for that section.
 */
static int teaches(const Professor *p, const Course *c)
{
    if (c->lecturer[0] != '\0')
        return same_person(p->name, c->lecturer);
    return lists_course(p, c->code);
}

size_t match_courses(Faculty *faculty, const Schedule *schedule, FILE *warnings)
{
    size_t unassigned = 0;

    for (size_t i = 0; i < faculty->n_profs; i++)
        faculty->profs[i].n_teaching = 0;

    for (size_t c = 0; c < schedule->n_courses; c++) {
        const Course *course = &schedule->courses[c];
        int taken = 0;
        for (size_t i = 0; i < faculty->n_profs; i++) {
            Professor *p = &faculty->profs[i];
            if (!teaches(p, course))
                continue;
            if (p->n_teaching == MAX_PROF_COURSES) {
                fprintf(warnings, "warning: %s teaches more than %d sections; %s skipped\n",
                        p->name, MAX_PROF_COURSES, course->code);
                continue;
            }
            p->teaching[p->n_teaching++] = c;
            taken = 1;
        }
        if (!taken) {
            fprintf(warnings, "warning: no professor found for %s%s%s\n", course->code,
                    course->lecturer[0] ? " taught by " : "", course->lecturer);
            unassigned++;
        }
    }
    return unassigned;
}

/* ---- office hours ------------------------------------------------------ */

/* Everything a professor is busy with on one day: classes and office hours so far. */
typedef struct {
    int start[MAX_PROF_COURSES + MAX_OFFICE_HOURS];
    int end[MAX_PROF_COURSES + MAX_OFFICE_HOURS];
    int n;
} Busy;

static void busy_for_day(const Professor *p, const Schedule *s, int day, Busy *b)
{
    b->n = 0;
    for (size_t i = 0; i < p->n_teaching; i++) {
        const Course *c = &s->courses[p->teaching[i]];
        if ((c->days & (1u << day)) && c->start != NO_TIME && c->end != NO_TIME) {
            b->start[b->n] = c->start;
            b->end[b->n] = c->end;
            b->n++;
        }
    }
    for (size_t i = 0; i < p->n_office_hours; i++) {
        const OfficeHour *oh = &p->office_hours[i];
        if (oh->day == day) {
            b->start[b->n] = oh->start;
            b->end[b->n] = oh->end;
            b->n++;
        }
    }
}

/* Latest time >= t up to which the professor is free, or -1 if busy at t. */
static int free_until(const Busy *b, int t, int limit)
{
    int until = limit;
    for (int i = 0; i < b->n; i++) {
        if (b->start[i] <= t && t < b->end[i])
            return -1;
        if (b->start[i] >= t && b->start[i] < until)
            until = b->start[i];
    }
    return until;
}

/* Earliest time <= t from which the professor is free, or -1 if busy just before t. */
static int free_since(const Busy *b, int t, int limit)
{
    int since = limit;
    for (int i = 0; i < b->n; i++) {
        if (b->start[i] < t && t <= b->end[i])     /* busy during the minute before t */
            return -1;
        if (b->end[i] <= t && b->end[i] > since)
            since = b->end[i];
    }
    return since;
}

static int add_block(Professor *p, int day, int start, int end, const char *course,
                     FILE *warnings)
{
    if (p->n_office_hours == MAX_OFFICE_HOURS) {
        fprintf(warnings, "warning: more than %d office-hour blocks for %s; %s on %s skipped\n",
                MAX_OFFICE_HOURS, p->name, course, day_name(day));
        return -1;
    }
    OfficeHour *oh = &p->office_hours[p->n_office_hours++];
    oh->day = day;
    oh->start = start;
    oh->end = end;
    copy_field(oh->course, sizeof oh->course, course);
    return 0;
}

/* Place one block for a class meeting: right after it if possible, else right before it. */
static int place_block(Professor *p, const Schedule *s, const Policy *pol,
                       const Course *c, int day, FILE *warnings)
{
    Busy b;
    busy_for_day(p, s, day, &b);

    if (c->end < pol->day_end) {
        int until = free_until(&b, c->end, pol->day_end);
        if (until >= 0) {
            int end = c->end + pol->after_class < until ? c->end + pol->after_class : until;
            if (end - c->end >= pol->min_block)
                return add_block(p, day, c->end, end, c->code, warnings);
        }
    }

    if (c->start > pol->day_start) {
        int class_start = c->start < pol->day_end ? c->start : pol->day_end;
        int since = free_since(&b, class_start, pol->day_start);
        if (since >= 0) {
            int start = class_start - pol->before_class > since ? class_start - pol->before_class : since;
            if (class_start - start >= pol->min_block)
                return add_block(p, day, start, class_start, c->code, warnings);
        }
    }

    fprintf(warnings, "warning: no free time for %s office hours on %s (%s)\n",
            p->name, day_name(day), c->code);
    return -1;
}

/* Class meetings of one professor, in week order, so morning blocks are placed first. */
typedef struct {
    int day;
    const Course *course;
} Meeting;

static int compare_meetings(const void *a, const void *b)
{
    const Meeting *x = a, *y = b;
    if (x->day != y->day)
        return x->day - y->day;
    return x->course->start - y->course->start;
}

static int generate(Professor *p, const Schedule *s, const Policy *pol, FILE *warnings)
{
    Meeting meetings[MAX_PROF_COURSES * DAYS_PER_WEEK];
    int n = 0, problems = 0;

    for (size_t i = 0; i < p->n_teaching; i++) {
        const Course *c = &s->courses[p->teaching[i]];
        if (c->start == NO_TIME || c->end == NO_TIME)
            continue;               /* online / TBA sections have no fixed time */
        for (int d = 0; d < DAYS_PER_WEEK; d++) {
            if (c->days & (1u << d)) {
                meetings[n].day = d;
                meetings[n].course = c;
                n++;
            }
        }
    }
    qsort(meetings, (size_t)n, sizeof meetings[0], compare_meetings);

    p->n_office_hours = 0;
    p->source = OH_GENERATED;
    for (int i = 0; i < n; i++)
        if (place_block(p, s, pol, meetings[i].course, meetings[i].day, warnings) != 0)
            problems++;
    return problems;
}

/* Lecturer-supplied office hours are kept; report any that clash with a class. */
static int check_provided(const Professor *p, const Schedule *s, FILE *warnings)
{
    int problems = 0;
    for (size_t i = 0; i < p->n_office_hours; i++) {
        const OfficeHour *oh = &p->office_hours[i];
        for (size_t j = 0; j < p->n_teaching; j++) {
            const Course *c = &s->courses[p->teaching[j]];
            if (!(c->days & (1u << oh->day)) || c->start == NO_TIME)
                continue;
            if (oh->start < c->end && c->start < oh->end) {
                char a[16], b[16];
                format_time(oh->start, a, sizeof a);
                format_time(oh->end, b, sizeof b);
                fprintf(warnings, "warning: %s's office hour on %s %s - %s overlaps %s\n",
                        p->name, day_name(oh->day), a, b, c->code);
                problems++;
            }
        }
    }
    return problems;
}

static int compare_office_hours(const void *a, const void *b)
{
    const OfficeHour *x = a, *y = b;
    if (x->day != y->day)
        return x->day - y->day;
    return x->start - y->start;
}

void sort_office_hours(Professor *p)
{
    qsort(p->office_hours, p->n_office_hours, sizeof p->office_hours[0], compare_office_hours);
}

int assign_office_hours(Faculty *faculty, const Schedule *schedule,
                        const Policy *policy, FILE *warnings)
{
    int problems = 0;
    for (size_t i = 0; i < faculty->n_profs; i++) {
        Professor *p = &faculty->profs[i];
        if (p->source == OH_PROVIDED)
            problems += check_provided(p, schedule, warnings);
        else
            problems += generate(p, schedule, policy, warnings);
        sort_office_hours(p);
    }
    return problems;
}
