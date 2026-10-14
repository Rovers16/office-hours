/*
 * main.c - command-line front end of the office-hours generator.
 *
 * Usage: officehours [options] SCHEDULE_FILE FACULTY_FILE
 *
 * Reads exactly two input files (the class schedule and the faculty file),
 * works out which professor teaches what, generates office hours where the
 * lecturer did not provide them, and writes a text report (plus, on request,
 * a CSV file, Prolog facts and an SQL script for SQLite).
 * All of the work is done by the static library libofficehours.a.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loader.h"
#include "model.h"
#include "report.h"
#include "scheduler.h"
#include "timeutil.h"

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
        "Usage: %s [options] SCHEDULE_FILE FACULTY_FILE\n"
        "\n"
        "Options:\n"
        "  -o FILE         write the text report to FILE (default: standard output)\n"
        "  -c FILE         also write a CSV version of the report to FILE\n"
        "  -p FILE         also write the results as Prolog facts to FILE\n"
        "  -s FILE         also write the results as an SQL script (SQLite) to FILE\n"
        "  --after MIN     length of an office-hour block after a class (default 135)\n"
        "  --before MIN    length of a block before a class when there is no room after (default 120)\n"
        "  --min MIN       shortest block worth scheduling (default 30)\n"
        "  --day-start HH:MM   earliest office-hour time (default 08:00)\n"
        "  --day-end HH:MM     latest office-hour time (default 17:00)\n"
        "  -h, --help      show this help\n",
        prog);
}

static int minutes_arg(const char *text, int *out)
{
    char *end;
    long v = strtol(text, &end, 10);
    if (*end != '\0' || v <= 0 || v > 24 * 60)
        return -1;
    *out = (int)v;
    return 0;
}

static int clock_arg(const char *text, int *out)
{
    int t = parse_time(text);
    if (t < 0)
        return -1;
    *out = t;
    return 0;
}

static FILE *open_output(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
        perror(path);
    return fp;
}

int main(int argc, char **argv)
{
    const char *report_path = NULL, *csv_path = NULL, *prolog_path = NULL,
               *sql_path = NULL;
    const char *inputs[2];
    int n_inputs = 0;
    Policy policy = default_policy();

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        int bad = 0;

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout, argv[0]);
            return 0;
        } else if (arg[0] == '-' && arg[1] != '\0' && value == NULL) {
            fprintf(stderr, "%s: option %s needs a value\n", argv[0], arg);
            return 2;
        } else if (strcmp(arg, "-o") == 0) {
            report_path = argv[++i];
        } else if (strcmp(arg, "-c") == 0) {
            csv_path = argv[++i];
        } else if (strcmp(arg, "-p") == 0) {
            prolog_path = argv[++i];
        } else if (strcmp(arg, "-s") == 0) {
            sql_path = argv[++i];
        } else if (strcmp(arg, "--after") == 0) {
            bad = minutes_arg(argv[++i], &policy.after_class);
        } else if (strcmp(arg, "--before") == 0) {
            bad = minutes_arg(argv[++i], &policy.before_class);
        } else if (strcmp(arg, "--min") == 0) {
            bad = minutes_arg(argv[++i], &policy.min_block);
        } else if (strcmp(arg, "--day-start") == 0) {
            bad = clock_arg(argv[++i], &policy.day_start);
        } else if (strcmp(arg, "--day-end") == 0) {
            bad = clock_arg(argv[++i], &policy.day_end);
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "%s: unknown option %s\n", argv[0], arg);
            usage(stderr, argv[0]);
            return 2;
        } else if (n_inputs < 2) {
            inputs[n_inputs++] = arg;
        } else {
            n_inputs++;
        }
        if (bad) {
            fprintf(stderr, "%s: bad value '%s' for %s\n", argv[0], argv[i], arg);
            return 2;
        }
    }

    if (n_inputs != 2) {
        fprintf(stderr, "%s: exactly two input files are required\n", argv[0]);
        usage(stderr, argv[0]);
        return 2;
    }
    if (policy.day_end <= policy.day_start) {
        fprintf(stderr, "%s: --day-end must be after --day-start\n", argv[0]);
        return 2;
    }

    Schedule schedule;
    Faculty faculty;
    schedule_init(&schedule);
    faculty_init(&faculty);
    int status = 1;

    if (load_schedule(inputs[0], &schedule) != 0 || load_faculty(inputs[1], &faculty) != 0)
        goto done;
    if (schedule.n_courses == 0 || faculty.n_profs == 0) {
        fprintf(stderr, "%s: no %s found in %s\n", argv[0],
                schedule.n_courses == 0 ? "courses" : "professors",
                schedule.n_courses == 0 ? inputs[0] : inputs[1]);
        goto done;
    }

    match_courses(&faculty, &schedule, stderr);
    assign_office_hours(&faculty, &schedule, &policy, stderr);

    FILE *out = report_path ? open_output(report_path) : stdout;
    if (out == NULL)
        goto done;
    write_text_report(out, &faculty, &schedule);
    if (out != stdout)
        fclose(out);

    if (csv_path) {
        FILE *csv = open_output(csv_path);
        if (csv == NULL)
            goto done;
        write_csv_report(csv, &faculty, &schedule);
        fclose(csv);
    }
    if (prolog_path) {
        FILE *pl = open_output(prolog_path);
        if (pl == NULL)
            goto done;
        write_prolog_facts(pl, &faculty, &schedule);
        fclose(pl);
    }
    if (sql_path) {
        FILE *sql = open_output(sql_path);
        if (sql == NULL)
            goto done;
        write_sql(sql, &faculty, &schedule);
        fclose(sql);
    }
    status = 0;

done:
    schedule_free(&schedule);
    faculty_free(&faculty);
    return status;
}
