/*
 * timeutil.c - converting days and times between text and numbers.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "model.h"
#include "timeutil.h"

static const char *DAY_NAMES[DAYS_PER_WEEK] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
};

int parse_time(const char *text)
{
    int h, m;
    char extra;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return NO_TIME;
    if (sscanf(text, "%d:%d %c", &h, &m, &extra) != 2)
        return -2;
    if (h < 0 || h > 23 || m < 0 || m > 59)
        return -2;
    return h * 60 + m;
}

void format_time(int minutes, char *buf, size_t n)
{
    if (minutes < 0) {
        snprintf(buf, n, "TBA");
        return;
    }
    int h = minutes / 60 % 24, m = minutes % 60;
    int h12 = h % 12 == 0 ? 12 : h % 12;
    snprintf(buf, n, "%d:%02d %s", h12, m, h < 12 ? "AM" : "PM");
}

int parse_day(const char *text)
{
    char word[16];
    size_t len = 0;
    while (isspace((unsigned char)*text))
        text++;
    while (isalpha((unsigned char)text[len]) && len < sizeof word - 1) {
        word[len] = (char)tolower((unsigned char)text[len]);
        len++;
    }
    word[len] = '\0';
    if (len < 2)
        return -1;
    for (int d = 0; d < DAYS_PER_WEEK; d++) {
        /* accept the full name or any prefix of at least 3 letters ("Wed") */
        char full[16];
        size_t i;
        for (i = 0; DAY_NAMES[d][i]; i++)
            full[i] = (char)tolower((unsigned char)DAY_NAMES[d][i]);
        full[i] = '\0';
        if (strcmp(word, full) == 0 || (len >= 3 && strncmp(word, full, len) == 0))
            return d;
    }
    return -1;
}

const char *day_name(int day)
{
    return day >= 0 && day < DAYS_PER_WEEK ? DAY_NAMES[day] : "?";
}

int parse_day_list(const char *text, unsigned *mask)
{
    char buf[128];
    *mask = 0;
    copy_field(buf, sizeof buf, text);
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        int d = parse_day(tok);
        if (d < 0)
            return -1;
        *mask |= 1u << d;
    }
    return 0;
}

void format_day_list(unsigned mask, char *buf, size_t n)
{
    int days[DAYS_PER_WEEK], count = 0;
    for (int d = 0; d < DAYS_PER_WEEK; d++)
        if (mask & (1u << d))
            days[count++] = d;

    buf[0] = '\0';
    for (int i = 0; i < count; i++) {
        const char *sep = i == 0 ? "" : (i == count - 1 ? " & " : ", ");
        size_t used = strlen(buf);
        snprintf(buf + used, n - used, "%s%s", sep, DAY_NAMES[days[i]]);
    }
}
