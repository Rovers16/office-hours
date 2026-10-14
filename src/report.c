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

/* ---- SQL (SQLite) ------------------------------------------------------- */

/* A quoted SQL string literal; an empty value becomes NULL. */
static void sql_text(FILE *out, const char *value)
{
    if (value[0] == '\0') {
        fputs("NULL", out);
        return;
    }
    fputc('\'', out);
    for (const char *p = value; *p; p++) {
        if (*p == '\'')
            fputc('\'', out);           /* SQL escapes ' by doubling it */
        fputc(*p, out);
    }
    fputc('\'', out);
}

static void sql_time(FILE *out, int minutes)
{
    if (minutes < 0)
        fputs("NULL", out);
    else
        fprintf(out, "'%02d:%02d'", minutes / 60, minutes % 60);
}

static const char SQL_SCHEMA[] =
    "PRAGMA foreign_keys = ON;\n"
    "BEGIN TRANSACTION;\n"
    "\n"
    "DROP VIEW IF EXISTS class_schedule;\n"
    "DROP VIEW IF EXISTS office_hours_schedule;\n"
    "DROP TABLE IF EXISTS office_hours;\n"
    "DROP TABLE IF EXISTS teaches;\n"
    "DROP TABLE IF EXISTS course_days;\n"
    "DROP TABLE IF EXISTS courses;\n"
    "DROP TABLE IF EXISTS professors;\n"
    "\n"
    "CREATE TABLE professors (\n"
    "    id          INTEGER PRIMARY KEY,\n"
    "    name        TEXT NOT NULL UNIQUE,\n"
    "    department  TEXT,\n"
    "    email       TEXT,\n"
    "    office      TEXT,\n"
    "    office_hours_source TEXT NOT NULL CHECK (office_hours_source IN ('generated', 'provided'))\n"
    ");\n"
    "\n"
    "CREATE TABLE courses (\n"
    "    id          INTEGER PRIMARY KEY,\n"
    "    code        TEXT NOT NULL,\n"
    "    title       TEXT,\n"
    "    section     TEXT,\n"
    "    lecturer    TEXT,           -- as written in the syllabus\n"
    "    room        TEXT,\n"
    "    start_time  TEXT,           -- 'HH:MM', NULL for online / TBA\n"
    "    end_time    TEXT\n"
    ");\n"
    "\n"
    "CREATE TABLE course_days (\n"
    "    course_id   INTEGER NOT NULL REFERENCES courses(id),\n"
    "    day         TEXT NOT NULL,\n"
    "    day_number  INTEGER NOT NULL,   -- 1 = Monday ... 7 = Sunday\n"
    "    PRIMARY KEY (course_id, day)\n"
    ");\n"
    "\n"
    "CREATE TABLE teaches (\n"
    "    professor_id INTEGER NOT NULL REFERENCES professors(id),\n"
    "    course_id    INTEGER NOT NULL REFERENCES courses(id),\n"
    "    PRIMARY KEY (professor_id, course_id)\n"
    ");\n"
    "\n"
    "CREATE TABLE office_hours (\n"
    "    id           INTEGER PRIMARY KEY,\n"
    "    professor_id INTEGER NOT NULL REFERENCES professors(id),\n"
    "    day          TEXT NOT NULL,\n"
    "    day_number   INTEGER NOT NULL,\n"
    "    start_time   TEXT NOT NULL,\n"
    "    end_time     TEXT NOT NULL,\n"
    "    course_code  TEXT\n"
    ");\n"
    "\n"
    "-- Every class meeting, one row per professor, course and day.\n"
    "CREATE VIEW class_schedule AS\n"
    "    SELECT p.name AS professor, c.code, c.title, c.section, d.day, d.day_number,\n"
    "           c.start_time, c.end_time, c.room\n"
    "    FROM teaches t\n"
    "    JOIN professors p ON p.id = t.professor_id\n"
    "    JOIN courses c ON c.id = t.course_id\n"
    "    JOIN course_days d ON d.course_id = c.id;\n"
    "\n"
    "-- Every office-hour block with the professor's name.\n"
    "CREATE VIEW office_hours_schedule AS\n"
    "    SELECT p.name AS professor, o.day, o.day_number, o.start_time, o.end_time,\n"
    "           o.course_code, p.office, p.office_hours_source AS source\n"
    "    FROM office_hours o\n"
    "    JOIN professors p ON p.id = o.professor_id;\n";

void write_sql(FILE *out, const Faculty *faculty, const Schedule *schedule)
{
    fputs("-- Generated by officehours - do not edit by hand.\n", out);
    fputs(SQL_SCHEMA, out);

    fputs("\n-- Courses from the syllabus (id = position in the schedule file).\n", out);
    for (size_t i = 0; i < schedule->n_courses; i++) {
        const Course *c = &schedule->courses[i];
        fprintf(out, "INSERT INTO courses VALUES (%zu, ", i + 1);
        sql_text(out, c->code);
        fputs(", ", out);
        sql_text(out, c->title);
        fputs(", ", out);
        sql_text(out, c->section);
        fputs(", ", out);
        sql_text(out, c->lecturer);
        fputs(", ", out);
        sql_text(out, c->room);
        fputs(", ", out);
        sql_time(out, c->start);
        fputs(", ", out);
        sql_time(out, c->end);
        fputs(");\n", out);
        for (int d = 0; d < DAYS_PER_WEEK; d++)
            if (c->days & (1u << d))
                fprintf(out, "INSERT INTO course_days VALUES (%zu, '%s', %d);\n",
                        i + 1, day_name(d), d + 1);
    }

    size_t oh_id = 0;
    for (size_t i = 0; i < faculty->n_profs; i++) {
        const Professor *p = &faculty->profs[i];
        fprintf(out, "\n-- %s\n", p->name);
        fprintf(out, "INSERT INTO professors VALUES (%zu, ", i + 1);
        sql_text(out, p->name);
        fputs(", ", out);
        sql_text(out, p->department);
        fputs(", ", out);
        sql_text(out, p->email);
        fputs(", ", out);
        sql_text(out, p->office);
        fprintf(out, ", '%s');\n", p->source == OH_PROVIDED ? "provided" : "generated");

        for (size_t j = 0; j < p->n_teaching; j++)
            fprintf(out, "INSERT INTO teaches VALUES (%zu, %zu);\n", i + 1, p->teaching[j] + 1);

        for (size_t j = 0; j < p->n_office_hours; j++) {
            const OfficeHour *oh = &p->office_hours[j];
            fprintf(out, "INSERT INTO office_hours VALUES (%zu, %zu, '%s', %d, ",
                    ++oh_id, i + 1, day_name(oh->day), oh->day + 1);
            sql_time(out, oh->start);
            fputs(", ", out);
            sql_time(out, oh->end);
            fputs(", ", out);
            sql_text(out, oh->course);
            fputs(");\n", out);
        }
    }
    fputs("\nCOMMIT;\n", out);
}
