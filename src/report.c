/*
 * report.c - writing the generated schedules as text and CSV.
 */
#include <string.h>

#include "report.h"
#include "timeutil.h"

#define CELL_LEN 192

/* "COSC 369 (Software Engineering I)", with the section if there is one. */
static void course_label(const Course *c, char *buf, size_t n)
{
    if (c->section[0] && c->title[0])
        snprintf(buf, n, "%s-%s (%s)", c->code, c->section, c->title);
    else if (c->section[0])
        snprintf(buf, n, "%s-%s", c->code, c->section);
    else if (c->title[0])
        snprintf(buf, n, "%s (%s)", c->code, c->title);
    else
        snprintf(buf, n, "%s", c->code);
}

/* "10:45 AM - 1:00 PM", or "3:00 - 5:00 PM" when both are in the same half of the day. */
static void time_range(int start, int end, char *buf, size_t n)
{
    char a[16], b[16];
    format_time(start, a, sizeof a);
    format_time(end, b, sizeof b);
    if (start >= 0 && end >= 0 && (start < 720) == (end < 720))
        a[strlen(a) - 3] = '\0';                /* drop " AM" / " PM" */
    snprintf(buf, n, "%s - %s", a, b);
}

/* "Tuesday & Thursday 9:30 AM to 10:45 AM" */
static void course_times(const Course *c, char *buf, size_t n)
{
    char days[96], a[16], b[16];
    format_day_list(c->days, days, sizeof days);
    if (c->start == NO_TIME) {
        snprintf(buf, n, "%s%sTBA", days, days[0] ? " " : "");
        return;
    }
    format_time(c->start, a, sizeof a);
    format_time(c->end, b, sizeof b);
    snprintf(buf, n, "%s %s to %s", days, a, b);
}

static void office_hour_text(const OfficeHour *oh, char *buf, size_t n)
{
    char range[48];
    time_range(oh->start, oh->end, range, sizeof range);
    if (oh->course[0])
        snprintf(buf, n, "%s (%s)", range, oh->course);
    else
        snprintf(buf, n, "%s", range);
}

static void rule(FILE *out, char ch, int width)
{
    for (int i = 0; i < width; i++)
        fputc(ch, out);
    fputc('\n', out);
}

static void write_professor(FILE *out, const Professor *p, const Schedule *s)
{
    char label[CELL_LEN], times[CELL_LEN];
    int w_course = (int)strlen("Course"), w_times = (int)strlen("Times");

    for (size_t i = 0; i < p->n_teaching; i++) {
        const Course *c = &s->courses[p->teaching[i]];
        course_label(c, label, sizeof label);
        course_times(c, times, sizeof times);
        if ((int)strlen(label) > w_course)
            w_course = (int)strlen(label);
        if ((int)strlen(times) > w_times)
            w_times = (int)strlen(times);
    }
    int width = w_course + w_times + 16;

    rule(out, '=', width);
    fprintf(out, "%s\n", p->name);
    if (p->department[0])
        fprintf(out, "Department: %s\n", p->department);
    if (p->office[0])
        fprintf(out, "Office:     %s\n", p->office);
    if (p->email[0])
        fprintf(out, "Email:      %s\n", p->email);
    rule(out, '=', width);

    fprintf(out, "\nCLASS SCHEDULE\n");
    fprintf(out, "%-*s   %-*s   %s\n", w_course, "Course", w_times, "Times", "Location");
    rule(out, '-', width);
    if (p->n_teaching == 0)
        fprintf(out, "(no classes found in the schedule)\n");
    for (size_t i = 0; i < p->n_teaching; i++) {
        const Course *c = &s->courses[p->teaching[i]];
        course_label(c, label, sizeof label);
        course_times(c, times, sizeof times);
        fprintf(out, "%-*s   %-*s   %s\n", w_course, label, w_times, times, c->room);
    }

    fprintf(out, "\nOFFICE HOURS (%s)\n",
            p->source == OH_PROVIDED ? "provided by the lecturer" : "generated");
    for (int day = 0; day < DAYS_PER_WEEK; day++) {
        int first = 1;
        char heading[16];
        snprintf(heading, sizeof heading, "%s:", day_name(day));
        for (size_t i = 0; i < p->n_office_hours; i++) {
            const OfficeHour *oh = &p->office_hours[i];
            if (oh->day != day)
                continue;
            office_hour_text(oh, times, sizeof times);
            if (first)
                fprintf(out, "    %-11s%s\n", heading, times);
            else
                fprintf(out, "    %-11s& %s\n", "", times);
            first = 0;
        }
        if (first && day < 5)                   /* show empty weekdays, like the sample */
            fprintf(out, "    %s\n", heading);
    }
    fprintf(out, "\nNOTE: Instructors are not to be interrupted during class times.\n\n");
}

void write_text_report(FILE *out, const Faculty *faculty, const Schedule *schedule)
{
    for (size_t i = 0; i < faculty->n_profs; i++)
        write_professor(out, &faculty->profs[i], schedule);
}

/* ---- CSV ---------------------------------------------------------------- */

static void csv_field(FILE *out, const char *value, int last)
{
    fputc('"', out);
    for (const char *p = value; *p; p++) {
        if (*p == '"')
            fputc('"', out);
        fputc(*p, out);
    }
    fputc('"', out);
    fputc(last ? '\n' : ',', out);
}

void write_csv_report(FILE *out, const Faculty *faculty, const Schedule *schedule)
{
    char a[CELL_LEN], b[CELL_LEN];

    for (size_t i = 0; i < faculty->n_profs; i++) {
        const Professor *p = &faculty->profs[i];

        csv_field(out, "PROF", 0);
        csv_field(out, p->name, 0);
        csv_field(out, p->department, 0);
        csv_field(out, p->office, 0);
        csv_field(out, p->email, 0);
        csv_field(out, p->source == OH_PROVIDED ? "provided" : "generated", 1);

        for (size_t j = 0; j < p->n_teaching; j++) {
            const Course *c = &schedule->courses[p->teaching[j]];
            course_label(c, a, sizeof a);
            course_times(c, b, sizeof b);
            csv_field(out, "CLASS", 0);
            csv_field(out, p->name, 0);
            csv_field(out, a, 0);
            csv_field(out, b, 0);
            csv_field(out, c->room, 1);
        }

        for (size_t j = 0; j < p->n_office_hours; j++) {
            const OfficeHour *oh = &p->office_hours[j];
            time_range(oh->start, oh->end, a, sizeof a);
            csv_field(out, "OFFICE_HOUR", 0);
            csv_field(out, p->name, 0);
            csv_field(out, day_name(oh->day), 0);
            csv_field(out, a, 0);
            csv_field(out, oh->course, 1);
        }
    }
}

/* ---- Prolog ------------------------------------------------------------- */

/* A quoted Prolog atom; an empty value becomes none. */
static void prolog_atom(FILE *out, const char *value)
{
    if (value[0] == '\0') {
        fputs("none", out);
        return;
    }
    fputc('\'', out);
    for (const char *p = value; *p; p++) {
        if (*p == '\'' || *p == '\\')
            fputc('\\', out);
        fputc(*p, out);
    }
    fputc('\'', out);
}

/* 570 -> '09:30', the same form syllabus_reader.py uses. */
static void prolog_time(FILE *out, int minutes)
{
    fprintf(out, "'%02d:%02d'", minutes / 60, minutes % 60);
}

static void lower_day(FILE *out, int day)
{
    for (const char *p = day_name(day); *p; p++)
        fputc(*p >= 'A' && *p <= 'Z' ? *p - 'A' + 'a' : *p, out);
}

void write_prolog_facts(FILE *out, const Faculty *faculty, const Schedule *schedule)
{
    fputs("% Generated by officehours - do not edit by hand.\n"
          "% assigned(Professor, CourseCode, Section).\n"
          "% scheduled_office_hour(Professor, Day, Start, End, CourseCode).\n"
          "% office_hours_source(Professor, generated | provided).\n"
          ":- discontiguous assigned/3, scheduled_office_hour/5, office_hours_source/2.\n", out);

    for (size_t i = 0; i < faculty->n_profs; i++) {
        const Professor *p = &faculty->profs[i];
        fputc('\n', out);

        fputs("office_hours_source(", out);
        prolog_atom(out, p->name);
        fprintf(out, ", %s).\n", p->source == OH_PROVIDED ? "provided" : "generated");

        for (size_t j = 0; j < p->n_teaching; j++) {
            const Course *c = &schedule->courses[p->teaching[j]];
            fputs("assigned(", out);
            prolog_atom(out, p->name);
            fputs(", ", out);
            prolog_atom(out, c->code);
            fputs(", ", out);
            prolog_atom(out, c->section);
            fputs(").\n", out);
        }

        for (size_t j = 0; j < p->n_office_hours; j++) {
            const OfficeHour *oh = &p->office_hours[j];
            fputs("scheduled_office_hour(", out);
            prolog_atom(out, p->name);
            fputs(", ", out);
            lower_day(out, oh->day);
            fputs(", ", out);
            prolog_time(out, oh->start);
            fputs(", ", out);
            prolog_time(out, oh->end);
            fputs(", ", out);
            prolog_atom(out, oh->course);
            fputs(").\n", out);
        }
    }
}
