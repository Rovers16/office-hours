/*
 * report.h - writing the generated schedules.
 */
#ifndef REPORT_H
#define REPORT_H

#include <stdio.h>
#include "model.h"

/* Human-readable report in the style of the sample tables, one section per professor. */
void write_text_report(FILE *out, const Faculty *faculty, const Schedule *schedule);

/*
 * CSV for the Python document generator (make_docx.py). Every row starts with
 * a record type; all values are already formatted for display:
 *   PROF,name,department,office,email,source
 *   CLASS,name,"COSC 369 (Software Engineering I)","Tuesday & Thursday 9:30 AM to 10:45 AM",room
 *   OFFICE_HOUR,name,day,"10:45 AM - 1:00 PM",course
 */
void write_csv_report(FILE *out, const Faculty *faculty, const Schedule *schedule);

/*
 * The program's results as Prolog facts, for prolog/knowledge_base.pl.
 * Atoms use the same conventions as syllabus_reader.py (quoted names and
 * codes, lower-case days, 'HH:MM' times, none for an empty value):
 *   assigned(Professor, CourseCode, Section).
 *   scheduled_office_hour(Professor, Day, Start, End, CourseCode).
 *   office_hours_source(Professor, generated | provided).
 */
void write_prolog_facts(FILE *out, const Faculty *faculty, const Schedule *schedule);

/*
 * The program's results as an SQL script for SQLite (make_db.py turns it into
 * output/office_hours.db). Tables: professors, courses, course_days, teaches,
 * office_hours; views: class_schedule, office_hours_schedule. Times are
 * 'HH:MM' text (NULL for online / TBA courses); days are full names with a
 * day_number (1 = Monday) for sorting.
 */
void write_sql(FILE *out, const Faculty *faculty, const Schedule *schedule);

#endif /* REPORT_H */
