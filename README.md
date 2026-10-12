# office-hours

COSC 369 project: a C program that builds each professor's teaching schedule
and office-hours schedule from two input files: a syllabus / class schedule
and a faculty file.

## Quick start

```bash
pip install -r requirements.txt   # anthropic is only used by make MODE=api
make                              # build, generate output/, write the Word file
make test                         # check the report against tests/expected_report.txt
make run                          # print the report on the screen
make clean                        # remove build/ and output/
```

`make` produces:

| File | What it is |
|------|------------|
| `build/libofficehours.a` | the static library |
| `build/officehours` | the program, linked against the library |
| `output/office_hours.txt` | the report, in the style of the sample tables |
| `output/office_hours.csv` | the same data for the Word generator |
| `output/office_hours.docx` | the final Word document |

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
Claude API on every build.

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

There are two ways to run it. Both produce exactly the same output files.

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

### Output formats

The output format follows the `-o` extension (`.psv`, `.pl`, `.json`) or can be
set with `--format`. In API mode, use `--model` (or `$CLAUDE_MODEL`) to pick a different
Claude model; e.g. `--model claude-haiku-5-5` is the cheapest option.

| Format | Contents |
|--------|----------|
| `psv`  | `COURSE\|code\|title\|section\|lecturer\|days\|start\|end\|room`, `PROF\|...`, `OFFICE_HOUR\|...` lines, easy to read in C with `fgets` + `strtok` |
| `prolog` | `course/5`, `meets/5`, `professor/4`, `teaches/2`, `office_hour/5` facts for `prolog.consult()` |
| `json` | The full structured data |

Times are always 24-hour `HH:MM` and days are full names (`Monday` ...).
