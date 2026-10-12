# office-hours
## Reading syllabus and faculty files with Claude

`syllabus_reader.py` sends an input file to the Claude API and gets back the
courses (or professors and office hours) in one fixed structure, so the
program works no matter how the syllabus is laid out: tables, paragraphs,
abbreviations like `MW`/`TTh`, PDF, Word, or plain text.

### Setup

1. Create an API key at <https://console.anthropic.com> (API usage is billed
   separately from a claude.ai Free/Pro subscription; new accounts may get a
   small amount of trial credit).
2. Install the Python packages and set the key:

   ```bash
   pip install -r requirements.txt
   export ANTHROPIC_API_KEY=sk-ant-...
   ```

### Usage

```bash
# Syllabus / class schedule -> pipe-separated file for the C program
python3 syllabus_reader.py syllabus input/syllabus_sample.txt -o build/schedule.psv

# Faculty file -> Prolog facts
python3 syllabus_reader.py faculty input/faculty.docx -o build/faculty.pl

# Print JSON to the screen
python3 syllabus_reader.py syllabus input/syllabus.pdf
```

The output format follows the `-o` extension (`.psv`, `.pl`, `.json`) or can be
set with `--format`. Use `--model` (or `$CLAUDE_MODEL`) to pick a different
Claude model; e.g. `--model claude-haiku-5-5` is the cheapest option.

| Format | Contents |
|--------|----------|
| `psv`  | `COURSE\|code\|title\|section\|lecturer\|days\|start\|end\|room`, `PROF\|...`, `OFFICE_HOUR\|...` lines, easy to read in C with `fgets` + `strtok` |
| `prolog` | `course/5`, `meets/5`, `professor/4`, `teaches/2`, `office_hour/5` facts for `prolog.consult()` |
| `json` | The full structured data |

Times are always 24-hour `HH:MM` and days are full names (`Monday` ...).
