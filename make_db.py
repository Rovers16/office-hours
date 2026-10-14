#!/usr/bin/env python3
"""Load the SQL script written by the C program into an SQLite database file.

All scheduling decisions are made by the C program (officehours -s FILE); this
script only executes its SQL with Python's built-in sqlite3 module, so no
extra software is needed.

Usage:
    python3 make_db.py build/office_hours.sql output/office_hours.db

Query the result, for example:
    sqlite3 output/office_hours.db "SELECT * FROM office_hours_schedule WHERE day = 'Wednesday'"
"""

import sqlite3
import sys
from pathlib import Path

TABLES = ["professors", "courses", "course_days", "teaches", "office_hours"]


def build(sql_path: Path, db_path: Path) -> dict:
    """Create db_path from scratch and return the number of rows in each table."""
    script = sql_path.read_text(encoding="utf-8")
    db_path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = db_path.with_name(db_path.name + ".tmp")
    tmp_path.unlink(missing_ok=True)

    # Build into a temporary file and rename it at the end, so a failed run
    # never leaves a half-written database behind.
    db = sqlite3.connect(tmp_path)
    try:
        db.executescript(script)
        problems = db.execute("PRAGMA foreign_key_check").fetchall()
        if problems:
            raise ValueError(f"{sql_path}: rows refer to missing ids: {problems}")
        counts = {t: db.execute(f"SELECT COUNT(*) FROM {t}").fetchone()[0] for t in TABLES}
    except BaseException:
        db.close()
        tmp_path.unlink(missing_ok=True)
        raise
    db.close()
    tmp_path.replace(db_path)
    return counts


def main(argv) -> int:
    if len(argv) != 3:
        print("usage: python3 make_db.py SQL_FILE DB_FILE", file=sys.stderr)
        return 2
    sql_path, db_path = Path(argv[1]), Path(argv[2])
    try:
        counts = build(sql_path, db_path)
    except (OSError, sqlite3.Error, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    summary = ", ".join(f"{n} {t}" for t, n in counts.items())
    print(f"wrote {db_path} ({summary})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
