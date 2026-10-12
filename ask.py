#!/usr/bin/env python3
"""Ask questions about the teaching and office-hours schedule, answered by Prolog.

Python only translates the question into a Prolog query and formats the
answer; the knowledge (facts) and reasoning (rules) live in prolog/.

Usage:
    python3 ask.py                                   answer the sample questions
    python3 ask.py who-teaches "COSC 369"
    python3 ask.py office-hours wednesday
    python3 ask.py schedule mensah
    python3 ask.py available mensah wednesday 11:30
    python3 ask.py where "COSC 330"
    python3 ask.py help-for "COSC 631"               office hours for a course
    python3 ask.py free-days
    python3 ask.py clashes
    python3 ask.py query "evening_class(Code, Day, Start)"
    python3 ask.py -i                                type Prolog queries yourself

Run `make` first: it builds the fact files the knowledge base loads.
Requires SWI-Prolog and pyswip (pip install pyswip).
"""

import argparse
import re
import sys
from pathlib import Path

KNOWLEDGE_BASE = Path(__file__).resolve().parent / "prolog" / "knowledge_base.pl"
DAYS = ["monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"]


# ---------------------------------------------------------------------------
# Talking to Prolog
# ---------------------------------------------------------------------------

def load_prolog():
    try:
        from pyswip import Prolog
    except ImportError:
        sys.exit("error: pyswip is not installed (pip install pyswip; needs SWI-Prolog)")
    missing = [p for p in ("build/schedule.pl", "build/faculty.pl", "output/office_hours.pl")
               if not (KNOWLEDGE_BASE.parent.parent / p).is_file()]
    if missing:
        sys.exit(f"error: {', '.join(missing)} not found - run `make` first")
    prolog = Prolog()
    prolog.consult(str(KNOWLEDGE_BASE))
    return prolog


def atom(text: str) -> str:
    """Quote user text as a Prolog atom so it can't change the query."""
    return "'" + str(text).replace("\\", "\\\\").replace("'", "\\'") + "'"


def ask(prolog, query: str) -> list:
    """All solutions of a query, as dicts of variable -> str/int."""
    return [{k: v.decode() if isinstance(v, bytes) else v for k, v in row.items()}
            for row in prolog.query(query)]


# ---------------------------------------------------------------------------
# Formatting
# ---------------------------------------------------------------------------

def clock(t: str) -> str:
    """'13:00' -> '1:00 PM'."""
    if t in (None, "none"):
        return "TBA"
    h, m = map(int, t.split(":"))
    return f"{h % 12 or 12}:{m:02d} {'AM' if h < 12 else 'PM'}"


def span(start: str, end: str) -> str:
    return f"{clock(start)} - {clock(end)}"


def day_order(row: dict, key="Day", then="Start"):
    return DAYS.index(row[key]), row.get(then, "")


def course_suffix(code: str) -> str:
    return "" if code in (None, "none") else f" ({code})"


def parse_clock(text: str) -> str:
    """'11:30', '11:30am', '2pm', '14' -> 'HH:MM' as used in the facts."""
    m = re.fullmatch(r"\s*(\d{1,2})(?::(\d{2}))?\s*([ap]\.?m\.?)?\s*", text.lower())
    if not m:
        raise ValueError(f"not a time: {text!r} (try 11:30 or 2pm)")
    h, mins, ampm = int(m.group(1)), int(m.group(2) or 0), m.group(3)
    if ampm:
        h = h % 12 + (12 if ampm.startswith("p") else 0)
    if h > 23 or mins > 59:
        raise ValueError(f"not a time: {text!r}")
    return f"{h:02d}:{mins:02d}"


def parse_day(text: str) -> str:
    matches = [d for d in DAYS if d.startswith(text.strip().lower())] if len(text.strip()) >= 2 else []
    if len(matches) != 1:
        raise ValueError(f"not a day: {text!r}")
    return matches[0]


# ---------------------------------------------------------------------------
# Resolving loosely typed names through Prolog
# ---------------------------------------------------------------------------

def find_professor(prolog, text: str) -> str:
    names = sorted({r["P"] for r in ask(prolog, f"prof_named({atom(text)}, P)")})
    if not names:
        raise ValueError(f"no professor matches {text!r}")
    if len(names) > 1:
        raise ValueError(f"{text!r} matches several professors: {', '.join(names)}")
    return names[0]


def find_course(prolog, text: str) -> str:
    codes = sorted({r["C"] for r in ask(prolog, f"course_named({atom(text)}, C)")})
    if not codes:
        raise ValueError(f"no course matches {text!r}")
    return codes[0]


# ---------------------------------------------------------------------------
# Questions
# ---------------------------------------------------------------------------

def who_teaches(prolog, course_text):
    code = find_course(prolog, course_text)
    profs = sorted({r["P"] for r in ask(prolog, f"who_teaches({atom(code)}, P)")})
    print(f"Who teaches {code}?")
    print(f"  {', '.join(profs) if profs else 'Nobody is assigned to it.'}")


def office_hours_on(prolog, day_text):
    day = parse_day(day_text)
    rows = ask(prolog, f"office_hours_on({day}, P, Start, End, Code)")
    print(f"Which office hours are on {day.title()}?")
    if not rows:
        print("  None.")
    for r in sorted(rows, key=lambda r: (r["Start"], r["P"])):
        print(f"  {r['P']}: {span(r['Start'], r['End'])}{course_suffix(r['Code'])}")


def schedule(prolog, prof_text):
    prof = find_professor(prolog, prof_text)
    p = atom(prof)
    print(f"What is {prof}'s week?")
    classes = ask(prolog, f"class_time({p}, Code, Day, Start, End), where_is(Code, Room)")
    hours = ask(prolog, f"scheduled_office_hour({p}, Day, Start, End, Code)")
    online = ask(prolog, f"teaches_course({p}, Code, _), online_course(Code)")
    for day in DAYS:
        items = [(r["Start"], f"class   {span(r['Start'], r['End'])}  {r['Code']} in {r['Room']}")
                 for r in classes if r["Day"] == day]
        items += [(r["Start"], f"office  {span(r['Start'], r['End'])}{course_suffix(r['Code'])}")
                  for r in hours if r["Day"] == day]
        for i, (_, text) in enumerate(sorted(items)):
            print(f"  {(day.title() + ':') if i == 0 else '':<11}{text}")
    for r in online:
        print(f"  {'Online:':<11}{r['Code']} (no fixed meeting time)")
    [minutes] = ask(prolog, f"weekly_office_minutes({p}, M)")
    source = ask(prolog, f"office_hours_source({p}, S)")
    how = f", {source[0]['S']}" if source else ""
    print(f"  Office hours per week: {minutes['M'] / 60:g} h{how}")


def available(prolog, prof_text, day_text, time_text):
    prof, day, time = find_professor(prolog, prof_text), parse_day(day_text), parse_clock(time_text)
    p = atom(prof)
    print(f"Can I see {prof} on {day.title()} at {clock(time)}?")
    if ask(prolog, f"available({p}, {day}, {atom(time)})"):
        [r] = ask(prolog, f"in_office({p}, {day}, {atom(time)}, Code), "
                          f"scheduled_office_hour({p}, {day}, S, E, Code)")[:1]
        print(f"  Yes - office hours {span(r['S'], r['E'])}{course_suffix(r['Code'])}.")
    elif ask(prolog, f"in_class({p}, {day}, {atom(time)})"):
        print("  No - they are teaching then (instructors are not to be interrupted).")
    else:
        rows = ask(prolog, f"scheduled_office_hour({p}, {day}, S, E, _)")
        when = ", ".join(span(r["S"], r["E"]) for r in sorted(rows, key=lambda r: r["S"]))
        print(f"  No office hours then. {day.title()}: {when or 'none that day'}.")


def where(prolog, course_text):
    code = find_course(prolog, course_text)
    print(f"Where and when does {code} meet?")
    rows = ask(prolog, f"meets_on(Day, {atom(code)}, Start, End, Room)")
    if not rows:
        rooms = {r["R"] for r in ask(prolog, f"where_is({atom(code)}, R)")}
        print(f"  No fixed meeting time ({', '.join(sorted(rooms))}).")
    for r in sorted(rows, key=day_order):
        print(f"  {r['Day'].title():<10} {span(r['Start'], r['End'])} in {r['Room']}")


def help_for(prolog, course_text):
    code = find_course(prolog, course_text)
    print(f"When can students get help with {code}?")
    rows = ask(prolog, f"office_hours_for({atom(code)}, P, Day, Start, End)")
    if not rows:
        print("  No office hours are attached to this course.")
    for r in sorted(rows, key=day_order):
        print(f"  {r['Day'].title():<10} {span(r['Start'], r['End'])} with {r['P']}")


def free_days(prolog):
    print("Which weekdays does each professor have no classes or office hours?")
    rows = ask(prolog, "free_day(P, Day)")
    for prof in sorted({r["P"] for r in ask(prolog, "professor(P, _, _, _)")}):
        days = [r["Day"].title() for r in sorted(rows, key=day_order) if r["P"] == prof]
        print(f"  {prof}: {', '.join(days) or 'none'}")


def clashes(prolog):
    print("Do any office hours overlap a class?")
    rows = ask(prolog, "clash(P, Day, Code, Start, End)")
    if not rows:
        print("  No - every office hour is outside class time.")
    for r in sorted(rows, key=day_order):
        print(f"  {r['P']}: {r['Day'].title()} {span(r['Start'], r['End'])} overlaps {r['Code']}")


def raw_query(prolog, query):
    query = query.strip().rstrip(".")
    try:
        rows = ask(prolog, query)
    except Exception as exc:        # pyswip raises PrologError for bad queries
        print(f"  error: {exc}")
        return
    if not rows:
        print("  false.")
    for row in rows:
        print("  " + (", ".join(f"{k} = {v}" for k, v in row.items()) or "true."))


def interactive(prolog):
    print("Type a Prolog query (e.g. who_teaches('COSC 369', P)), or an empty line to quit.")
    while True:
        try:
            line = input("?- ")
        except EOFError:
            print()
            break
        if not line.strip():
            break
        raw_query(prolog, line)


def demo(prolog):
    """The sample questions; `make test` compares this output with tests/expected_answers.txt."""
    questions = [
        lambda: who_teaches(prolog, "COSC 369"),
        lambda: office_hours_on(prolog, "wednesday"),
        lambda: schedule(prolog, "mensah"),
        lambda: available(prolog, "mensah", "wednesday", "11:30"),
        lambda: available(prolog, "mensah", "tuesday", "10:00"),
        lambda: where(prolog, "cosc330"),
        lambda: help_for(prolog, "COSC 631"),
        lambda: free_days(prolog),
        lambda: clashes(prolog),
    ]
    for i, question in enumerate(questions):
        if i:
            print()
        question()


COMMANDS = {
    "who-teaches": (who_teaches, ["course"]),
    "office-hours": (office_hours_on, ["day"]),
    "schedule": (schedule, ["professor"]),
    "available": (available, ["professor", "day", "time"]),
    "where": (where, ["course"]),
    "help-for": (help_for, ["course"]),
    "free-days": (free_days, []),
    "clashes": (clashes, []),
    "query": (raw_query, ["prolog_query"]),
}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        epilog=__doc__.split("\n\n", 2)[2],
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("-i", "--interactive", action="store_true", help="type Prolog queries yourself")
    sub = parser.add_subparsers(dest="command")
    for name, (_, params) in COMMANDS.items():
        cmd = sub.add_parser(name)
        for param in params:
            cmd.add_argument(param)
    args = parser.parse_args(argv)

    prolog = load_prolog()
    try:
        if args.interactive:
            interactive(prolog)
        elif args.command is None:
            demo(prolog)
        else:
            func, params = COMMANDS[args.command]
            func(prolog, *(getattr(args, p) for p in params))
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
