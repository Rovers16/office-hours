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

#endif /* REPORT_H */
