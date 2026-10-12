/*
 * model.c - growing arrays and comparing names/codes.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "model.h"

void schedule_init(Schedule *s)
{
    s->courses = NULL;
    s->n_courses = 0;
    s->cap = 0;
}

void schedule_free(Schedule *s)
{
    free(s->courses);
    schedule_init(s);
}

Course *schedule_add(Schedule *s)
{
    if (s->n_courses == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 16;
        Course *grown = realloc(s->courses, cap * sizeof *grown);
        if (grown == NULL)
            return NULL;
        s->courses = grown;
        s->cap = cap;
    }
    Course *c = &s->courses[s->n_courses++];
    memset(c, 0, sizeof *c);
    c->start = c->end = NO_TIME;
    return c;
}

void faculty_init(Faculty *f)
{
    f->profs = NULL;
    f->n_profs = 0;
    f->cap = 0;
}

void faculty_free(Faculty *f)
{
    free(f->profs);
    faculty_init(f);
}

Professor *faculty_add(Faculty *f)
{
    if (f->n_profs == f->cap) {
        size_t cap = f->cap ? f->cap * 2 : 8;
        Professor *grown = realloc(f->profs, cap * sizeof *grown);
        if (grown == NULL)
            return NULL;
        f->profs = grown;
        f->cap = cap;
    }
    Professor *p = &f->profs[f->n_profs++];
    memset(p, 0, sizeof *p);
    return p;
}

Professor *faculty_find(Faculty *f, const char *name)
{
    for (size_t i = 0; i < f->n_profs; i++)
        if (strcmp(f->profs[i].name, name) == 0)
            return &f->profs[i];
    return NULL;
}

void copy_field(char *dst, size_t n, const char *src)
{
    while (isspace((unsigned char)*src))
        src++;
    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1]))
        len--;
    if (len >= n)
        len = n - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

int same_course_code(const char *a, const char *b)
{
    for (;;) {
        while (*a && !isalnum((unsigned char)*a))
            a++;
        while (*b && !isalnum((unsigned char)*b))
            b++;
        if (*a == '\0' || *b == '\0')
            return *a == *b;
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
}

/* ---- person names ------------------------------------------------------ */

#define MAX_TOKENS 8
#define TOKEN_LEN 32

static int is_title(const char *t)
{
    static const char *titles[] = { "dr", "prof", "professor", "mr", "mrs", "ms", "phd", NULL };
    for (int i = 0; titles[i]; i++)
        if (strcmp(t, titles[i]) == 0)
            return 1;
    return 0;
}

/* Split a name into lower-case words, dropping punctuation and titles. */
static int name_tokens(const char *name, char tokens[MAX_TOKENS][TOKEN_LEN])
{
    int n = 0;
    while (*name && n < MAX_TOKENS) {
        while (*name && !isalpha((unsigned char)*name))
            name++;
        int len = 0;
        while (isalpha((unsigned char)*name) || *name == '\'' || *name == '-') {
            if (isalpha((unsigned char)*name) && len < TOKEN_LEN - 1)
                tokens[n][len++] = (char)tolower((unsigned char)*name);
            name++;
        }
        tokens[n][len] = '\0';
        if (len > 0 && !is_title(tokens[n]))
            n++;
    }
    return n;
}

/* Every word of the shorter name appears in the longer one ("Dr. Mensah" vs "Akosua Mensah"). */
int same_person(const char *a, const char *b)
{
    char ta[MAX_TOKENS][TOKEN_LEN], tb[MAX_TOKENS][TOKEN_LEN];
    int na = name_tokens(a, ta), nb = name_tokens(b, tb);
    if (na == 0 || nb == 0)
        return 0;
    if (na > nb) {
        return same_person(b, a);
    }
    for (int i = 0; i < na; i++) {
        int found = 0;
        for (int j = 0; j < nb && !found; j++)
            found = strcmp(ta[i], tb[j]) == 0;
        if (!found)
            return 0;
    }
    return 1;
}
