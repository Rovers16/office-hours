# Makefile for the COSC 369 office-hours generator.
#
#   make            build everything and generate the output (default),
#                   including the SQLite database output/office_hours.db
#   make lib        build only the static library build/libofficehours.a
#   make run        print the generated report on the screen
#   make ask        answer the sample questions with Prolog (needs SWI-Prolog + pyswip)
#   make test       compare the report and the Prolog answers with tests/
#   make report     rebuild report/report.pdf from report/report.html (needs playwright)
#   make clean      remove everything that make created
#
# Where the two input files come from (see README.md):
#   make                 MODE=chat (default): use the JSON replies saved from the
#                        free claude.ai chat in data/ - no API key, no Internet
#   make MODE=api        send input/ files to the Claude API (needs ANTHROPIC_API_KEY)
#   make MODE=ollama     send input/ files to a local Ollama model (needs Ollama running;
#                        pick the model with OLLAMA_MODEL=..., default llama3.1)

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2
CPPFLAGS = -Iinclude
AR      ?= ar
ARFLAGS  = rcs
PYTHON  ?= python3

MODE ?= chat
OLLAMA_MODEL ?= llama3.1

SRC_DIR   = src
BUILD_DIR = build
OUT_DIR   = output

# Modules that go into the static library (everything except main.c).
LIB_MODULES = model timeutil loader scheduler report
LIB_OBJS    = $(LIB_MODULES:%=$(BUILD_DIR)/%.o)
LIB         = $(BUILD_DIR)/libofficehours.a
MAIN_OBJ    = $(BUILD_DIR)/main.o
PROGRAM     = $(BUILD_DIR)/officehours

# The two input files of the C program (pipe-separated, made by syllabus_reader.py).
SCHEDULE_PSV = $(BUILD_DIR)/schedule.psv
FACULTY_PSV  = $(BUILD_DIR)/faculty.psv

# The same data as Prolog facts, for prolog/knowledge_base.pl.
SCHEDULE_PL = $(BUILD_DIR)/schedule.pl
FACULTY_PL  = $(BUILD_DIR)/faculty.pl

# Original, human-written input files.
SYLLABUS_INPUT = input/syllabus_sample.txt
FACULTY_INPUT  = input/faculty_sample.txt

# Structured data extracted from them by Claude.
ifeq ($(MODE),chat)
SYLLABUS_JSON = data/syllabus_reply.json
FACULTY_JSON  = data/faculty_reply.json
else ifeq ($(MODE),api)
SYLLABUS_JSON = $(BUILD_DIR)/syllabus_api.json
FACULTY_JSON  = $(BUILD_DIR)/faculty_api.json
else ifeq ($(MODE),ollama)
SYLLABUS_JSON = $(BUILD_DIR)/syllabus_ollama.json
FACULTY_JSON  = $(BUILD_DIR)/faculty_ollama.json
else
$(error MODE must be 'chat', 'api' or 'ollama', not '$(MODE)')
endif

REPORT_TXT  = $(OUT_DIR)/office_hours.txt
REPORT_CSV  = $(OUT_DIR)/office_hours.csv
REPORT_DOCX = $(OUT_DIR)/office_hours.docx
REPORT_PL   = $(OUT_DIR)/office_hours.pl
REPORT_SQL  = $(BUILD_DIR)/office_hours.sql
REPORT_DB   = $(OUT_DIR)/office_hours.db
ANSWERS     = $(OUT_DIR)/prolog_answers.txt

.PHONY: all lib run ask test report clean

all: $(REPORT_TXT) $(REPORT_DOCX) $(REPORT_DB) $(SCHEDULE_PL) $(FACULTY_PL)

lib: $(LIB)

# ---- C: compile, archive, link ---------------------------------------------

# Every object depends on its source and on the headers it includes.
$(BUILD_DIR)/model.o:     $(SRC_DIR)/model.c     include/model.h
$(BUILD_DIR)/timeutil.o:  $(SRC_DIR)/timeutil.c  include/timeutil.h include/model.h
$(BUILD_DIR)/loader.o:    $(SRC_DIR)/loader.c    include/loader.h include/model.h include/timeutil.h
$(BUILD_DIR)/scheduler.o: $(SRC_DIR)/scheduler.c include/scheduler.h include/model.h include/timeutil.h
$(BUILD_DIR)/report.o:    $(SRC_DIR)/report.c    include/report.h include/model.h include/timeutil.h
$(BUILD_DIR)/main.o:      $(SRC_DIR)/main.c      include/loader.h include/model.h include/report.h \
                          include/scheduler.h include/timeutil.h

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Static library: an archive of the module object files.
$(LIB): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# Link main.o against the library (-L where to look, -l which library).
$(PROGRAM): $(MAIN_OBJ) $(LIB)
	$(CC) $(CFLAGS) $(MAIN_OBJ) -L$(BUILD_DIR) -lofficehours -o $@

# ---- Python: turn the syllabus and faculty files into the C program's input --

# API mode only: one Claude request per input file. In chat mode the JSON
# files in data/ are the replies saved from the claude.ai chat.
$(BUILD_DIR)/syllabus_api.json: $(SYLLABUS_INPUT) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py syllabus $< -o $@
$(BUILD_DIR)/faculty_api.json: $(FACULTY_INPUT) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py faculty $< -o $@

# Ollama mode only: the same, with a local model instead of the Claude API.
$(BUILD_DIR)/syllabus_ollama.json: $(SYLLABUS_INPUT) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py --backend ollama --model $(OLLAMA_MODEL) syllabus $< -o $@
$(BUILD_DIR)/faculty_ollama.json: $(FACULTY_INPUT) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py --backend ollama --model $(OLLAMA_MODEL) faculty $< -o $@

# Convert the extracted data (no Claude request) into the formats we need.
$(BUILD_DIR)/schedule.%: $(SYLLABUS_JSON) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py syllabus --from-json $< -o $@
$(BUILD_DIR)/faculty.%: $(FACULTY_JSON) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py faculty --from-json $< -o $@

# ---- Output -----------------------------------------------------------------

# One run of the program writes the text report, the CSV, the Prolog facts and the SQL.
$(REPORT_TXT): $(PROGRAM) $(SCHEDULE_PSV) $(FACULTY_PSV) | $(OUT_DIR)
	$(PROGRAM) -o $(REPORT_TXT) -c $(REPORT_CSV) -p $(REPORT_PL) -s $(REPORT_SQL) \
	    $(SCHEDULE_PSV) $(FACULTY_PSV)
$(REPORT_CSV) $(REPORT_PL) $(REPORT_SQL): $(REPORT_TXT)
	@:

$(REPORT_DOCX): $(REPORT_CSV) make_docx.py
	$(PYTHON) make_docx.py $< $@

# SQLite database, loaded from the SQL script with Python's built-in sqlite3.
$(REPORT_DB): $(REPORT_SQL) make_db.py
	$(PYTHON) make_db.py $< $@

run: $(PROGRAM) $(SCHEDULE_PSV) $(FACULTY_PSV)
	$(PROGRAM) $(SCHEDULE_PSV) $(FACULTY_PSV)

# ---- Prolog -------------------------------------------------------------------

KNOWLEDGE = $(SCHEDULE_PL) $(FACULTY_PL) $(REPORT_PL) prolog/knowledge_base.pl prolog/rules.pl

$(ANSWERS): $(KNOWLEDGE) ask.py
	$(PYTHON) ask.py > $@

ask: $(KNOWLEDGE)
	$(PYTHON) ask.py

# ---- Project report -------------------------------------------------------------

report: report/report.pdf

report/report.pdf: report/report.html report/img/docx_page.png report/build_pdf.py
	$(PYTHON) report/build_pdf.py

# ---- Tests ----------------------------------------------------------------------

test: $(REPORT_TXT) $(ANSWERS) $(REPORT_DB)
	diff -u tests/expected_report.txt $(REPORT_TXT)
	diff -u tests/expected_database.sql $(REPORT_SQL)
	diff -u tests/expected_answers.txt $(ANSWERS)
	@echo "all tests passed"

$(BUILD_DIR) $(OUT_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)
