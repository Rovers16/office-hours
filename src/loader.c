/*
 * loader.c - reading the pipe-separated schedule and faculty files.
 */
#include <stdio.h>
#include <string.h>

#include "loader.h"
#include "timeutil.h"

#define LINE_LEN 1024
#define MAX_FIELDS 12

/*
 * Split a line on '|' in place. Unlike strtok() this keeps empty fields,
 * so "COURSE|X||Y" gives four fields with the third one empty.
 */
static int split_fields(char *line, char *fields[], int max)
{
    int n = 0;
    line[strcspn(line, "\r\n")] = '\0';
    fields[n++] = line;
    for (char *p = line; *p && n < max; p++) {
        if (*p == '|') {
            *p = '\0';
            fields[n++] = p + 1;
        }
    }
    return n;
}

static int is_skippable(const char *line)
{
    while (*line == ' ' || *line == '\t')
        line++;
    return *line == '#' || *line == '\n' || *line == '\r' || *line == '\0';
}

static FILE *open_input(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        perror(path);
    return fp;
}

/* Parse a time field; on bad input report it and return -2. */
static int time_field(const char *text, const char *path, int line_no)
{
    int t = parse_time(text);
    if (t == -2)
        fprintf(stderr, "%s:%d: bad time '%s' (expected HH:MM)\n", path, line_no, text);
    return t;
}

int load_schedule(const char *path, Schedule *out)
{
    FILE *fp = open_input(path);
    if (fp == NULL)
        return -1;

    char line[LINE_LEN];
    char *f[MAX_FIELDS];
    int line_no = 0, status = 0;

    while (fgets(line, sizeof line, fp)) {
        line_no++;
        if (is_skippable(line))
            continue;
        int n = split_fields(line, f, MAX_FIELDS);
        if (strcmp(f[0], "COURSE") != 0 || n != 9) {
            fprintf(stderr, "%s:%d: expected COURSE|code|title|section|lecturer|days|start|end|room\n",
                    path, line_no);
            status = -1;
            continue;
        }

        Course *c = schedule_add(out);
        if (c == NULL) {
            fprintf(stderr, "out of memory\n");
            status = -1;
            break;
        }
        copy_field(c->code, sizeof c->code, f[1]);
        copy_field(c->title, sizeof c->title, f[2]);
        copy_field(c->section, sizeof c->section, f[3]);
        copy_field(c->lecturer, sizeof c->lecturer, f[4]);
        copy_field(c->room, sizeof c->room, f[8]);
        if (parse_day_list(f[5], &c->days) != 0) {
            fprintf(stderr, "%s:%d: unknown day in '%s'\n", path, line_no, f[5]);
            status = -1;
        }
        c->start = time_field(f[6], path, line_no);
        c->end = time_field(f[7], path, line_no);
        if (c->start == -2 || c->end == -2) {
            status = -1;
        } else if (c->start != NO_TIME && c->end != NO_TIME && c->end <= c->start) {
            fprintf(stderr, "%s:%d: %s ends before it starts\n", path, line_no, c->code);
            status = -1;
        }
    }
    fclose(fp);
    return status;
}

static int add_professor(const char *path, int line_no, char *f[], int n, Faculty *out)
{
    if (n != 6) {
        fprintf(stderr, "%s:%d: expected PROF|name|department|email|office|courses\n", path, line_no);
        return -1;
    }
    Professor *p = faculty_add(out);
    if (p == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }
    copy_field(p->name, sizeof p->name, f[1]);
    copy_field(p->department, sizeof p->department, f[2]);
    copy_field(p->email, sizeof p->email, f[3]);
    copy_field(p->office, sizeof p->office, f[4]);
    for (char *code = strtok(f[5], ","); code; code = strtok(NULL, ",")) {
        if (p->n_course_codes == MAX_PROF_COURSES) {
            fprintf(stderr, "%s:%d: more than %d courses for %s\n",
                    path, line_no, MAX_PROF_COURSES, p->name);
            return -1;
        }
        copy_field(p->course_codes[p->n_course_codes++], CODE_LEN, code);
    }
    return 0;
}

static int add_office_hour(const char *path, int line_no, char *f[], int n, Faculty *out)
{
    if (n != 6) {
        fprintf(stderr, "%s:%d: expected OFFICE_HOUR|name|day|start|end|course\n", path, line_no);
        return -1;
    }
    char name[NAME_LEN];
    copy_field(name, sizeof name, f[1]);
    Professor *p = faculty_find(out, name);
    if (p == NULL) {
        fprintf(stderr, "%s:%d: office hour for '%s' comes before their PROF line\n",
                path, line_no, name);
        return -1;
    }
    if (p->n_office_hours == MAX_OFFICE_HOURS) {
        fprintf(stderr, "%s:%d: too many office hours for %s\n", path, line_no, name);
        return -1;
    }

    OfficeHour *oh = &p->office_hours[p->n_office_hours];
    oh->day = parse_day(f[2]);
    oh->start = time_field(f[3], path, line_no);
    oh->end = time_field(f[4], path, line_no);
    copy_field(oh->course, sizeof oh->course, f[5]);
    if (oh->day < 0) {
        fprintf(stderr, "%s:%d: unknown day '%s'\n", path, line_no, f[2]);
        return -1;
    }
    if (oh->start < 0 || oh->end < 0 || oh->end <= oh->start) {
        fprintf(stderr, "%s:%d: office hour needs a start time before its end time\n",
                path, line_no);
        return -1;
    }
    p->n_office_hours++;
    p->source = OH_PROVIDED;
    return 0;
}

int load_faculty(const char *path, Faculty *out)
{
    FILE *fp = open_input(path);
    if (fp == NULL)
        return -1;

    char line[LINE_LEN];
    char *f[MAX_FIELDS];
    int line_no = 0, status = 0;

    while (fgets(line, sizeof line, fp)) {
        line_no++;
        if (is_skippable(line))
            continue;
        int n = split_fields(line, f, MAX_FIELDS);
        int rc;
        if (strcmp(f[0], "PROF") == 0) {
            rc = add_professor(path, line_no, f, n, out);
        } else if (strcmp(f[0], "OFFICE_HOUR") == 0) {
            rc = add_office_hour(path, line_no, f, n, out);
        } else {
            fprintf(stderr, "%s:%d: unknown record type '%s'\n", path, line_no, f[0]);
            rc = -1;
        }
        if (rc != 0)
            status = -1;
    }
    fclose(fp);
    return status;
}
