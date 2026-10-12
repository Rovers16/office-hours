/*
 * loader.h - reading the two input files.
 *
 * Both files are pipe-separated text produced by syllabus_reader.py:
 *
 *   schedule file:  COURSE|code|title|section|lecturer|days|start|end|room
 *   faculty file:   PROF|name|department|email|office|course,course,...
 *                   OFFICE_HOUR|name|day|start|end|course
 *
 * Lines starting with '#' and blank lines are ignored.
 */
#ifndef LOADER_H
#define LOADER_H

#include "model.h"

/* Return 0 on success. On failure an error is printed to stderr and -1 returned. */
int load_schedule(const char *path, Schedule *out);
int load_faculty(const char *path, Faculty *out);

#endif /* LOADER_H */
