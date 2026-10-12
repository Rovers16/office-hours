# Makefile for the COSC 369 office-hours generator.
#
#   make            build everything and generate the output (default)
#   make lib        build only the static library build/libofficehours.a
#   make run        print the generated report on the screen
#   make test       compare the generated report with tests/expected_report.txt
#   make clean      remove everything that make created
#
# Where the two input files come from (see README.md):
#   make                 MODE=chat (default): use the JSON replies saved from the
#                        free claude.ai chat in data/ - no API key, no Internet
#   make MODE=api        send input/ files to the Claude API (needs ANTHROPIC_API_KEY)

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2
CPPFLAGS = -Iinclude
AR      ?= ar
ARFLAGS  = rcs
PYTHON  ?= python3

MODE ?= chat

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

# Original, human-written input files and the saved claude.ai chat replies.
SYLLABUS_INPUT = input/syllabus_sample.txt
FACULTY_INPUT  = input/faculty_sample.txt
SYLLABUS_REPLY = data/syllabus_reply.json
FACULTY_REPLY  = data/faculty_reply.json

REPORT_TXT  = $(OUT_DIR)/office_hours.txt
REPORT_CSV  = $(OUT_DIR)/office_hours.csv
REPORT_DOCX = $(OUT_DIR)/office_hours.docx

.PHONY: all lib run test clean

all: $(REPORT_TXT) $(REPORT_DOCX)

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

ifeq ($(MODE),api)
$(SCHEDULE_PSV): $(SYLLABUS_INPUT) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py syllabus $< -o $@
$(FACULTY_PSV): $(FACULTY_INPUT) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py faculty $< -o $@
else ifeq ($(MODE),chat)
$(SCHEDULE_PSV): $(SYLLABUS_REPLY) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py syllabus --from-json $< -o $@
$(FACULTY_PSV): $(FACULTY_REPLY) syllabus_reader.py | $(BUILD_DIR)
	$(PYTHON) syllabus_reader.py faculty --from-json $< -o $@
else
$(error MODE must be 'chat' or 'api', not '$(MODE)')
endif

# ---- Output -----------------------------------------------------------------

# One run of the program writes both the text report and the CSV.
$(REPORT_TXT): $(PROGRAM) $(SCHEDULE_PSV) $(FACULTY_PSV) | $(OUT_DIR)
	$(PROGRAM) -o $(REPORT_TXT) -c $(REPORT_CSV) $(SCHEDULE_PSV) $(FACULTY_PSV)
$(REPORT_CSV): $(REPORT_TXT)
	@:

$(REPORT_DOCX): $(REPORT_CSV) make_docx.py
	$(PYTHON) make_docx.py $< $@

run: $(PROGRAM) $(SCHEDULE_PSV) $(FACULTY_PSV)
	$(PROGRAM) $(SCHEDULE_PSV) $(FACULTY_PSV)

test: $(REPORT_TXT)
	diff -u tests/expected_report.txt $(REPORT_TXT) && echo "test passed"

$(BUILD_DIR) $(OUT_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)
