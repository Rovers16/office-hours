# office-hours
## Reading syllabus and faculty files with Claude

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
#    Copy Claude's JSON reply into build/syllabus_reply.json
#    (code fences or extra text around the JSON are fine).

# 3. Convert the reply into the files the C program / Prolog use
python3 syllabus_reader.py syllabus --from-json build/syllabus_reply.json -o build/schedule.psv
python3 syllabus_reader.py syllabus --from-json build/syllabus_reply.json -o build/schedule.pl
```

Do the same with `faculty` for the faculty file. The reply is checked against
the expected structure, so a wrong or incomplete reply is reported instead of
producing bad data. Keep the saved `*_reply.json` files with your submission so
the Makefile can rebuild everything without going back to the chat.

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
