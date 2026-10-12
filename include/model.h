/*
 * model.h - data structures shared by every module of the office-hours system.
 *
 * Times are stored as minutes after midnight (9:30 AM -> 570) so they can be
 * compared and added with plain integer arithmetic. -1 means "no time given".
 * Days are stored as an index 0..6 (Monday..Sunday); a course's meeting days
 * are a bit mask (bit 0 = Monday).
 */
#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>

#define NO_TIME (-1)
#define DAYS_PER_WEEK 7

#define CODE_LEN 24
#define NAME_LEN 96
#define TEXT_LEN 128

#define MAX_PROF_COURSES 16
#define MAX_OFFICE_HOURS 32

/* One course section from the syllabus / class-schedule file. */
typedef struct {
    char code[CODE_LEN];        /* "COSC 369" */
    char title[TEXT_LEN];       /* "Software Engineering I" */
    char section[CODE_LEN];     /* may be empty */
    char lecturer[NAME_LEN];    /* may be empty */
    unsigned days;              /* bit mask of meeting days */
    int start;                  /* minutes after midnight, or NO_TIME */
    int end;
    char room[CODE_LEN];
} Course;

/* One office-hour block. */
typedef struct {
    int day;                    /* 0 = Monday ... 6 = Sunday */
    int start;
    int end;
    char course[CODE_LEN];      /* course the block is attached to, may be empty */
} OfficeHour;

/* Where a professor's office hours came from. */
typedef enum {
    OH_GENERATED,               /* computed by the scheduler */
    OH_PROVIDED                 /* supplied by the lecturer in the faculty file */
} OfficeHourSource;

/* One professor from the faculty file, plus what the program works out. */
typedef struct {
    char name[NAME_LEN];
    char department[NAME_LEN];
    char email[NAME_LEN];
    char office[CODE_LEN];

    char course_codes[MAX_PROF_COURSES][CODE_LEN];  /* from the faculty file */
    size_t n_course_codes;

    OfficeHour office_hours[MAX_OFFICE_HOURS];
    size_t n_office_hours;
    OfficeHourSource source;

    /* Filled in by scheduler: indexes into Schedule.courses this professor teaches. */
    size_t teaching[MAX_PROF_COURSES];
    size_t n_teaching;
} Professor;

typedef struct {
    Course *courses;
    size_t n_courses;
    size_t cap;
} Schedule;

typedef struct {
    Professor *profs;
    size_t n_profs;
    size_t cap;
} Faculty;

void schedule_init(Schedule *s);
void schedule_free(Schedule *s);
Course *schedule_add(Schedule *s);          /* returns a zeroed slot, NULL if out of memory */

void faculty_init(Faculty *f);
void faculty_free(Faculty *f);
Professor *faculty_add(Faculty *f);         /* returns a zeroed slot, NULL if out of memory */
Professor *faculty_find(Faculty *f, const char *name);

/* Copy src into dst (size n), always NUL-terminated, trimming surrounding spaces. */
void copy_field(char *dst, size_t n, const char *src);

/* "COSC 369", "cosc369" and "COSC-369" all compare equal. */
int same_course_code(const char *a, const char *b);

/* "Dr. Jane Smith", "Jane Smith" and "jane smith" all compare equal. */
int same_person(const char *a, const char *b);

#endif /* MODEL_H */
