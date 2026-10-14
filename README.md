# office-hours

COSC 369 project: a C program that builds each professor's teaching schedule
and office-hours schedule from two input files: a syllabus / class schedule
and a faculty file.

The project report is [`report/report.pdf`](report/report.pdf).

## Quick start

```bash
sudo apt install swi-prolog-nox   # only needed for the Prolog part (make ask / make test)
pip install -r requirements.txt   # anthropic / ollama only for make MODE=api / MODE=ollama
make                              # build, generate output/, write the Word file
make test                         # check the report, the database and the Prolog answers against tests/
make run                          # print the report on the screen
make ask                          # answer the sample questions with Prolog
make clean                        # remove build/ and output/
make report                       # rebuild the PDF report (report/report.html -> report/report.pdf)
```

`make` produces:

| File | What it is |
|------|------------|
| `build/libofficehours.a` | the static library |
| `build/officehours` | the program, linked against the library |
| `output/office_hours.txt` | the report, in the style of the sample tables |
| `output/office_hours.csv` | the same data for the Word generator |
| `output/office_hours.docx` | the final Word document |
| `output/office_hours.db` | the results as an SQLite database (see [Querying the database](#querying-the-database)) |
| `output/office_hours.pl` | the program's results as Prolog facts |
| `build/schedule.pl`, `build/faculty.pl` | the input data as Prolog facts |
| `output/prolog_answers.txt` | answers to the sample questions (`make test`) |

## How the system fits together

```
input/syllabus_sample.txt ─┐                          ┌─> build/schedule.psv ─┐
input/faculty_sample.txt  ─┴─> Claude ─> data/*.json ─┴─> build/faculty.psv  ─┴─> officehours (C)
                           (syllabus_reader.py)                                       │
                                                                          output/office_hours.txt
                                                                          output/office_hours.csv ─> make_docx.py ─> .docx
```

1. **Reading the inputs (Python + Claude).** Syllabi come in every layout, so
   `syllabus_reader.py` has Claude extract the courses and professors into a
   fixed structure. It then writes them as two simple pipe-separated files.
   See [Step 1 in detail](#step-1-in-detail-reading-the-input-files-with-claude).
2. **Scheduling (C).** `build/officehours SCHEDULE FACULTY` takes exactly two
   input files. It matches professors to the courses they teach, keeps the
   office hours a lecturer supplied, and generates office hours for everyone
   else.
3. **Document (Python).** `make_docx.py` only lays out the C program's CSV as a
   Word document. All the scheduling decisions are made in C.
4. **Questions (Prolog).** The input data and the C program's results are
   also written as Prolog facts. Prolog combines them with the rules in
   `prolog/rules.pl` to answer questions such as "who teaches COSC 369?".
   See [Asking questions with Prolog](#asking-questions-with-prolog).

### Source layout

| Module | Header | Responsibility |
|--------|--------|----------------|
| `src/model.c` | `include/model.h` | data structures, growing arrays, comparing course codes and names |
| `src/timeutil.c` | `include/timeutil.h` | `"09:30"` ↔ minutes, `"9:30 AM"`, day names and day lists |
| `src/loader.c` | `include/loader.h` | reading and validating the two input files |
| `src/scheduler.c` | `include/scheduler.h` | matching professors to courses and placing office hours |
| `src/report.c` | `include/report.h` | the text report and the CSV |
| `src/main.c` | | command-line options; calls the library |

All modules except `main.c` are compiled into `libofficehours.a`.

### Office-hour rules

- **Lecturer-supplied office hours** in the faculty file are kept as they are.
  If any of them overlaps one of that lecturer's classes, a warning is printed.
- **Generated office hours.** For each class meeting, in week order, one block
  is placed:
  - **right after the class** (default 2 h 15 min), up to the next class or
    5:00 PM, whichever comes first;
  - **right before the class** (default 2 h) if there is no room after it. This
    covers evening classes.
  - Gaps shorter than 30 minutes are not used. Blocks never overlap a class or
    each other, which follows the rule that instructors are not interrupted
    during class times.
- **Matching professors to courses.** A professor teaches a section if the
  syllabus names them as its lecturer. "Dr. Mensah" matches "Dr. Kwame
  Mensah": titles are ignored and every word of the shorter name must appear
  in the longer one. If the syllabus names no lecturer, the faculty file's
  course list decides. A course nobody teaches is reported as a warning.

All the limits can be changed without recompiling:
`build/officehours --after 120 --before 90 --min 45 --day-start 09:00 --day-end 18:00 SCHEDULE FACULTY`.

With the sample inputs, Dr. Mensah's generated office hours come out exactly
like the sample in the assignment. Dr. Okafor shows the lecturer-supplied case
and an online course with no meeting time.

### Input file format (what the C program reads)

```
COURSE|code|title|section|lecturer|days|start|end|room
COURSE|COSC 369|Software Engineering I||Dr. Mensah|Tuesday,Thursday|09:30|10:45|CS 108

PROF|name|department|email|office|course,course,...
OFFICE_HOUR|name|day|start|end|course
```

Times are 24-hour `HH:MM`. Empty fields are allowed. Lines starting with `#`
are comments. Bad lines are reported with the file name and line number.

## Querying the database

`make` also stores the results in an SQLite database, `output/office_hours.db`.
The C program writes them as SQL (`build/officehours -s build/office_hours.sql`)
and `make_db.py` loads that script with Python's built-in `sqlite3`, so nothing
extra needs installing.

| Table / view | Contents |
|--------------|----------|
| `professors` | `id`, `name`, `department`, `email`, `office`, `office_hours_source` (`generated` / `provided`) |
| `courses` | `id`, `code`, `title`, `section`, `lecturer`, `room`, `start_time`, `end_time` (NULL for online courses) |
| `course_days` | `course_id`, `day`, `day_number` (1 = Monday): the days each course meets |
| `teaches` | `professor_id`, `course_id` |
| `office_hours` | `professor_id`, `day`, `day_number`, `start_time`, `end_time`, `course_code` |
| `class_schedule` (view) | every class meeting with the professor's name, day, times and room |
| `office_hours_schedule` (view) | every office-hour block with the professor's name and office |

Times are `HH:MM` text (24-hour), so they sort and compare correctly.

```bash
sudo apt install sqlite3              # the command-line tool (optional)
sqlite3 -header -column output/office_hours.db

sqlite> SELECT professor, start_time, end_time, course_code
   ...> FROM office_hours_schedule WHERE day = 'Wednesday' ORDER BY start_time;
sqlite> SELECT code, day, start_time, room FROM class_schedule
   ...> WHERE professor LIKE '%Mensah' ORDER BY day_number, start_time;
sqlite> .tables
sqlite> .quit
```

Without the `sqlite3` tool, use Python:
`python3 -c "import sqlite3; print(sqlite3.connect('output/office_hours.db').execute('SELECT * FROM professors').fetchall())"`.
The file also opens in graphical tools such as *DB Browser for SQLite*.

## Asking questions with Prolog

Python generates the **facts**, Prolog stores them together with hand-written
**rules**, and acts as the interpreter that answers questions.

| File | Written by | Contains |
|------|------------|----------|
| `build/schedule.pl` | `syllabus_reader.py` | `course(Code, Title, Section, Lecturer, Room)`, `meets(Code, Section, Day, Start, End)` |
| `build/faculty.pl` | `syllabus_reader.py` | `professor(Name, Department, Email, Office)`, `teaches(Name, Code)`, `office_hour(...)` supplied by lecturers |
| `output/office_hours.pl` | the C program (`-p`) | `assigned(Professor, Code, Section)`, `scheduled_office_hour(Professor, Day, Start, End, Code)`, `office_hours_source(Professor, generated/provided)` |
| `prolog/rules.pl` | by hand | the rules below |
| `prolog/knowledge_base.pl` | by hand | loads all of the above |

The office hours Prolog reasons about are the C program's final ones,
including the generated ones, so Prolog and the report always agree.

Some of the rules:

| Rule | Answers |
|------|---------|
| `who_teaches(Code, Prof)` | Who teaches this class? |
| `office_hours_on(Day, Prof, Start, End, Code)` | Which office hours are on Wednesday? |
| `available(Prof, Day, Time)` | Can I see this professor at this time? (in office hours and not in class) |
| `class_time/5`, `where_is/2`, `meets_on/5` | When and where does a class meet? |
| `office_hours_for(Code, Prof, Day, Start, End)` | When can I get help with this course? |
| `free_day(Prof, Day)`, `on_campus(Prof, Day)` | Which days is a professor not on campus? |
| `clash(Prof, Day, Code, Start, End)` | Does any office hour overlap a class? |
| `weekly_office_minutes(Prof, Minutes)` | How many office hours does a professor hold per week? |
| `online_course/1`, `evening_class/3`, `colleagues/2` | ... |

### Using it from Python (pyswip)

```bash
python3 ask.py                                   # the sample questions
python3 ask.py who-teaches "COSC 369"
python3 ask.py office-hours wednesday
python3 ask.py schedule mensah                   # a professor's whole week
python3 ask.py available mensah wednesday 11:30
python3 ask.py help-for "COSC 631"
python3 ask.py query "evening_class(Code, Day, Start)"
python3 ask.py -i                                # type Prolog queries yourself
```

Names and codes can be typed loosely: `mensah` finds Dr. Kwame Mensah, and
`cosc369` finds COSC 369. `ask.py` only builds the query and formats the
answer; all of the reasoning is done by the Prolog rules. User text is
always passed to Prolog as a quoted atom, so it cannot change the query.

### Using it directly in SWI-Prolog

```
$ swipl prolog/knowledge_base.pl
?- who_teaches('COSC 369', P).
P = 'Dr. Kwame Mensah'.

?- office_hours_on(wednesday, P, Start, End, Course).
P = 'Dr. Kwame Mensah', Start = '10:45', End = '13:00', Course = 'COSC 330' ;
...
```

## Where the input files come from

The two input files are **stored locally** in the project (`input/`). The
Claude replies for them are saved in `data/`. The program opens them with
ordinary `fopen()` calls on relative paths, so `make` works on a lab computer
with no Internet access and no API key. The output is also the same every time
you run it, which `make test` relies on. A university website could change its
format or go offline the day of the demo.

The only step that uses the Internet is turning a new syllabus into
structured data with Claude. Do that once when a syllabus changes, then save
the result in `data/`. `make MODE=api` instead re-reads `input/` through the
Claude API on every build, and `make MODE=ollama` through a local Ollama model.

## Static libraries and the Makefile

**Static library.** A static library (`.a`) is an archive of compiled object
files. `make` compiles each module separately (`gcc -c model.c -o model.o`,
...) and bundles the objects with `ar rcs libofficehours.a model.o ...`.
`main.o` is then linked with `-Lbuild -lofficehours`: `-L` says where to
look, and `-l` names the library (`libofficehours.a`). The linker copies the
code it needs into the executable, so `officehours` runs without the library
file present. A shared library (`.so`) would instead be loaded at run time.

**Makefile.** Each rule names a target, the files it depends on, and the
commands that rebuild it. `make` compares file timestamps and reruns only what
is out of date. Every `.o` lists the headers it includes, so editing
`timeutil.h` recompiles exactly the modules that use it. The rules chain all
the way from the input files to `office_hours.docx`, so one `make` command
compiles the code, builds the library, links the program, runs the Python
input step, runs the program, and produces the Word document.

## Step 1 in detail: reading the input files with Claude

`syllabus_reader.py` has Claude read an input file and gets back the
courses (or professors and office hours) in one fixed structure, so the
program works no matter how the syllabus is laid out: tables, paragraphs,
abbreviations like `MW`/`TTh`, PDF, Word, or plain text.

There are three ways to run it. All produce the same output files.

### Option A: free chat mode (claude.ai, no API key)

The claude.ai Free plan can't be called from a program, so in this mode you
do the one Claude step by hand and the script does the rest:

```bash
pip install pydantic python-docx

# 1. Print the instructions for Claude
mkdir -p build
python3 syllabus_reader.py syllabus --prompt > build/prompt.txt

# 2. Open https://claude.ai, start a new chat, attach your syllabus file,
#    paste the contents of build/prompt.txt, and send.
#    Copy Claude's JSON reply into data/syllabus_reply.json
#    (code fences or extra text around the JSON are fine).

# 3. Convert the reply into the files the C program / Prolog use
python3 syllabus_reader.py syllabus --from-json data/syllabus_reply.json -o build/schedule.psv
python3 syllabus_reader.py syllabus --from-json data/syllabus_reply.json -o build/schedule.pl
```

Do the same with `faculty` for the faculty file. The reply is checked against
the expected structure, so a wrong or incomplete reply is reported instead of
producing bad data. Save the replies as `data/syllabus_reply.json` and
`data/faculty_reply.json`; `make` builds from them, so step 3 happens
automatically and nothing needs the chat again until a syllabus changes.

> The two files in `data/` right now were written by hand in the exact format
> Claude returns, to match `input/`. Replace them with real chat replies for
> your own syllabus.

### Option B: API mode (automatic, needs a paid API key)

1. Create an API key at <https://console.anthropic.com>. API usage is billed
   per request, separately from any claude.ai plan.
2. Install the Python packages and set the key:

   ```bash
   pip install -r requirements.txt
   export ANTHROPIC_API_KEY=sk-ant-...
   ```

3. Run it on the input files directly:

   ```bash
   # Syllabus / class schedule -> pipe-separated file for the C program
   python3 syllabus_reader.py syllabus input/syllabus_sample.txt -o build/schedule.psv

   # Faculty file -> Prolog facts
   python3 syllabus_reader.py faculty input/faculty.docx -o build/faculty.pl

   # Print JSON to the screen
   python3 syllabus_reader.py syllabus input/syllabus.pdf
   ```

### Option C: Ollama mode (automatic, free, runs on your computer)

The same extraction as API mode, but with an open model running locally in
[Ollama](https://ollama.com). No API key or Internet connection is needed once
the model is downloaded.

1. Install Ollama, start it, and download a model (about 5 GB; needs at least 8 GB of RAM, 16 GB is comfortable):

   ```bash
   curl -fsSL https://ollama.com/install.sh | sh   # Linux / WSL
   ollama serve &                                  # if it is not already running
   ollama pull llama3.1
   pip install -r requirements.txt
   ```

2. Run it on the input files with `--backend ollama`:

   ```bash
   python3 syllabus_reader.py --backend ollama syllabus input/syllabus_sample.txt -o build/schedule.psv
   python3 syllabus_reader.py --backend ollama --model qwen2.5 faculty input/faculty_sample.txt
   ```

   or build everything with `make MODE=ollama` (`make MODE=ollama OLLAMA_MODEL=qwen2.5`
   for another model).

The reply is forced into the same JSON structure and checked the same way. PDFs
are converted to text first (with `pypdf`), because Ollama models can't read
PDF files. Small local models make more mistakes than Claude on messy
syllabi, for example `TTh` or AM/PM times, so check the generated
`build/schedule.psv` before trusting it. A scanned PDF (page images with no
text layer) is reported as an error rather than sent to the model as empty
text; use the Claude API or chat mode for those. The model's context window is
sized to each file, so a long syllabus is not silently cut short. If Ollama runs on another machine, or
on Windows while this runs in WSL, set `OLLAMA_HOST` (e.g.
`export OLLAMA_HOST=http://192.168.1.10:11434`).

### Output formats

The output format follows the `-o` extension (`.psv`, `.pl`, `.json`) or can be
set with `--format`. In API mode, use `--model` (or `$CLAUDE_MODEL`) to pick a different
Claude model; e.g. `--model claude-haiku-5-5` is the cheapest option. In Ollama
mode, `--model` (or `$OLLAMA_MODEL`) is any model you have pulled; the default is
`llama3.1`.

| Format | Contents |
|--------|----------|
| `psv`  | `COURSE\|code\|title\|section\|lecturer\|days\|start\|end\|room`, `PROF\|...`, `OFFICE_HOUR\|...` lines, easy to read in C with `fgets` + `strtok` |
| `prolog` | `course/5`, `meets/5`, `professor/4`, `teaches/2`, `office_hour/5` facts for `prolog.consult()` |
| `json` | The full structured data |

Times are always 24-hour `HH:MM` and days are full names (`Monday` ...).
