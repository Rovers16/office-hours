#!/usr/bin/env python3
"""Read a syllabus / class schedule or a faculty file in any layout using Claude.

Instead of hand-writing a parser for one specific format, the file contents are
sent to the Claude API, which returns the data in one fixed JSON structure.
That structure is then written out as JSON, Prolog facts, or a pipe-separated
text file that the C program can read with fgets()/strtok().

Two ways to run it:

  API mode (automatic, needs ANTHROPIC_API_KEY, billed per request):
    python3 syllabus_reader.py syllabus input/syllabus.pdf -o build/schedule.psv
    python3 syllabus_reader.py faculty  input/faculty.docx -o build/faculty.pl

  Chat mode (free, uses the claude.ai chat by hand):
    python3 syllabus_reader.py syllabus --prompt > prompt.txt
        -> paste prompt.txt into claude.ai, attach the syllabus file,
           save Claude's reply as reply.json
    python3 syllabus_reader.py syllabus --from-json reply.json -o build/schedule.psv

Supported inputs: .pdf, .docx, and any plain-text file (.txt, .csv, .html, .md ...).
"""

import argparse
import base64
import json
import os
import sys
from pathlib import Path
import re
from typing import List, Literal, Optional

from pydantic import BaseModel

DEFAULT_MODEL = "claude-opus-5-5"

Day = Literal["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]


# ---------------------------------------------------------------------------
# Output structures. Claude is forced to answer in exactly these shapes.
# Times are 24-hour "HH:MM" so the C program can compare them easily.
# ---------------------------------------------------------------------------

class Course(BaseModel):
    code: str                   # e.g. "COSC 369"
    title: str                  # e.g. "Software Engineering I"
    section: Optional[str]
    lecturer: Optional[str]
    days: List[Day]
    start_time: Optional[str]   # "09:30"
    end_time: Optional[str]     # "10:45"
    room: Optional[str]         # "CS 108"
    notes: Optional[str]


class Syllabus(BaseModel):
    courses: List[Course]


class OfficeHour(BaseModel):
    day: Day
    start_time: str
    end_time: str
    course_code: Optional[str]


class Professor(BaseModel):
    name: str
    department: Optional[str]
    email: Optional[str]
    office: Optional[str]
    courses: List[str]          # course codes taught
    office_hours: List[OfficeHour]
    notes: Optional[str]


class Faculty(BaseModel):
    professors: List[Professor]


KINDS = {"syllabus": Syllabus, "faculty": Faculty}

INSTRUCTIONS = {
    "syllabus": (
        "The attached file is a university syllabus or class schedule. It may be "
        "formatted in any way (tables, paragraphs, bullet lists, abbreviations like "
        "'MW' or 'TTh'). Extract every course meeting you can find. Create one "
        "entry per course section. Write times in 24-hour HH:MM format and expand "
        "day abbreviations to full day names. Use null for anything the file does "
        "not state; never invent values."
    ),
    "faculty": (
        "The attached file describes faculty members / professors. It may be "
        "formatted in any way. Extract every professor with their department, "
        "contact details, the course codes they teach, and any office hours that "
        "are listed. Write times in 24-hour HH:MM format and use full day names. "
        "Use null or an empty list for anything the file does not state; never "
        "invent values."
    ),
}


# ---------------------------------------------------------------------------
# Turning an input file into a message Claude can read
# ---------------------------------------------------------------------------

def docx_to_text(path: Path) -> str:
    """Pull paragraphs and table cells out of a Word document, in order."""
    import docx  # python-docx
    from docx.table import Table
    from docx.text.paragraph import Paragraph

    document = docx.Document(str(path))
    lines = []
    for child in document.element.body.iterchildren():
        tag = child.tag.rsplit("}", 1)[-1]
        if tag == "p":
            lines.append(Paragraph(child, document).text)
        elif tag == "tbl":
            for row in Table(child, document).rows:
                lines.append(" | ".join(cell.text.strip() for cell in row.cells))
    return "\n".join(lines)


def file_to_content(path: Path) -> list:
    """Build the content blocks for the request: the file, then the instructions."""
    suffix = path.suffix.lower()
    if suffix == ".pdf":
        data = base64.standard_b64encode(path.read_bytes()).decode("ascii")
        return [{
            "type": "document",
            "source": {"type": "base64", "media_type": "application/pdf", "data": data},
        }]
    if suffix == ".docx":
        text = docx_to_text(path)
    else:
        text = path.read_text(encoding="utf-8", errors="replace")
    return [{"type": "text", "text": f"<file name=\"{path.name}\">\n{text}\n</file>"}]


# ---------------------------------------------------------------------------
# Calling Claude
# ---------------------------------------------------------------------------

def extract(path: Path, kind: str, model: str = DEFAULT_MODEL, client=None):
    """Send the file to Claude and return a validated Syllabus or Faculty object."""
    schema_model = KINDS[kind]
    if client is None:
        import anthropic
        client = anthropic.Anthropic()  # reads ANTHROPIC_API_KEY

    content = file_to_content(path)
    content.append({"type": "text", "text": INSTRUCTIONS[kind]})

    response = client.beta.messages.create(
        model=model,
        max_tokens=16000,
        output_config={
            "effort": "low",  # simple extraction; keeps cost and latency down
            "format": {"type": "json_schema", "schema": json_schema(schema_model)},
        },
        # If a safety classifier ever declines, retry on a suitable model automatically.
        betas=["server-side-fallback-2026-07-01"],
        fallbacks="default",
        messages=[{"role": "user", "content": content}],
    )

    if response.stop_reason == "refusal":
        raise RuntimeError(f"Claude declined to read {path}: {response.stop_details}")
    if response.stop_reason == "max_tokens":
        raise RuntimeError(f"{path} is too large to extract in one request")

    text = "".join(block.text for block in response.content if block.type == "text")
    return schema_model.model_validate_json(text)


# ---------------------------------------------------------------------------
# Chat mode: no API key, the user pastes the prompt into claude.ai
# ---------------------------------------------------------------------------

def chat_prompt(kind: str) -> str:
    """Text to paste into the claude.ai chat together with the attached file."""
    schema = json.dumps(json_schema(KINDS[kind]), indent=2)
    return (
        f"{INSTRUCTIONS[kind].replace('The attached file', 'The file I attached')}\n\n"
        "Reply with ONLY a JSON object that matches this JSON Schema - no "
        "explanation before or after it. Every property is required; use null "
        "where the schema allows it and the file has no value.\n\n"
        f"{schema}\n"
    )


def parse_reply(text: str, kind: str):
    """Validate JSON copied from a chat reply. Code fences and extra text around it are ignored."""
    fenced = re.search(r"```(?:json)?\s*(.*?)```", text, re.DOTALL)
    if fenced:
        text = fenced.group(1)
    start, end = text.find("{"), text.rfind("}")
    if start == -1 or end == -1:
        raise ValueError("no JSON object found in the reply")
    return KINDS[kind].model_validate_json(text[start:end + 1])


def json_schema(model: type[BaseModel]) -> dict:
    """Pydantic schema, tightened for structured outputs (no extra keys, all required)."""
    schema = model.model_json_schema()

    def tighten(node):
        if isinstance(node, dict):
            if node.get("type") == "object" and "properties" in node:
                node["additionalProperties"] = False
                node["required"] = list(node["properties"])
            node.pop("title", None)
            for value in node.values():
                tighten(value)
        elif isinstance(node, list):
            for value in node:
                tighten(value)

    tighten(schema)
    return schema


# ---------------------------------------------------------------------------
# Output writers
# ---------------------------------------------------------------------------

def prolog_atom(value) -> str:
    """Quote a value as a Prolog atom; null becomes 'none'."""
    if value is None or value == "":
        return "none"
    return "'" + str(value).replace("\\", "\\\\").replace("'", "\\'") + "'"


def to_prolog(data) -> str:
    lines = ["% Generated by syllabus_reader.py - do not edit by hand."]
    if isinstance(data, Syllabus):
        lines += [
            "% course(Code, Title, Section, Lecturer, Room).",
            "% meets(Code, Section, Day, Start, End).",
            ":- discontiguous course/5, meets/5.",
        ]
        for c in data.courses:
            lines.append(
                f"course({prolog_atom(c.code)}, {prolog_atom(c.title)}, "
                f"{prolog_atom(c.section)}, {prolog_atom(c.lecturer)}, {prolog_atom(c.room)})."
            )
            for day in c.days:
                lines.append(
                    f"meets({prolog_atom(c.code)}, {prolog_atom(c.section)}, {day.lower()}, "
                    f"{prolog_atom(c.start_time)}, {prolog_atom(c.end_time)})."
                )
    else:
        lines += [
            "% professor(Name, Department, Email, Office).",
            "% teaches(Name, CourseCode).",
            "% office_hour(Name, Day, Start, End, CourseCode).",
            ":- discontiguous professor/4, teaches/2, office_hour/5.",
        ]
        for p in data.professors:
            lines.append(
                f"professor({prolog_atom(p.name)}, {prolog_atom(p.department)}, "
                f"{prolog_atom(p.email)}, {prolog_atom(p.office)})."
            )
            for code in p.courses:
                lines.append(f"teaches({prolog_atom(p.name)}, {prolog_atom(code)}).")
            for oh in p.office_hours:
                lines.append(
                    f"office_hour({prolog_atom(p.name)}, {oh.day.lower()}, "
                    f"{prolog_atom(oh.start_time)}, {prolog_atom(oh.end_time)}, "
                    f"{prolog_atom(oh.course_code)})."
                )
    return "\n".join(lines) + "\n"


def psv_field(value) -> str:
    """One field of a pipe-separated line; '|' and newlines are stripped out."""
    if value is None:
        return ""
    return str(value).replace("|", "/").replace("\n", " ").strip()


def to_psv(data) -> str:
    """Pipe-separated records, one per line, easy to parse in C with strtok()."""
    lines = []
    if isinstance(data, Syllabus):
        lines.append("#COURSE|code|title|section|lecturer|days|start|end|room")
        for c in data.courses:
            days = ",".join(c.days)
            lines.append("COURSE|" + "|".join(psv_field(v) for v in (
                c.code, c.title, c.section, c.lecturer, days, c.start_time, c.end_time, c.room)))
    else:
        lines.append("#PROF|name|department|email|office|courses")
        lines.append("#OFFICE_HOUR|name|day|start|end|course")
        for p in data.professors:
            lines.append("PROF|" + "|".join(psv_field(v) for v in (
                p.name, p.department, p.email, p.office, ",".join(p.courses))))
            for oh in p.office_hours:
                lines.append("OFFICE_HOUR|" + "|".join(psv_field(v) for v in (
                    p.name, oh.day, oh.start_time, oh.end_time, oh.course_code)))
    return "\n".join(lines) + "\n"


WRITERS = {
    "json": lambda data: data.model_dump_json(indent=2) + "\n",
    "prolog": to_prolog,
    "psv": to_psv,
}
EXTENSIONS = {".json": "json", ".pl": "prolog", ".psv": "psv"}


def extract_with_api(args):
    """API mode. Returns the data, or None after printing an error."""
    import anthropic
    try:
        return extract(args.input, args.kind, model=args.model)
    except anthropic.AuthenticationError:
        print("error: ANTHROPIC_API_KEY is invalid (see README.md)", file=sys.stderr)
    except TypeError as exc:
        if "authentication" not in str(exc):
            raise
        print("error: ANTHROPIC_API_KEY is not set (see README.md, or use --prompt "
              "for the free chat mode)", file=sys.stderr)
    except anthropic.APIConnectionError as exc:
        print(f"error: could not reach the Claude API: {exc}", file=sys.stderr)
    except anthropic.APIStatusError as exc:
        print(f"error: Claude API returned {exc.status_code}: {exc.message}", file=sys.stderr)
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
    return None


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        epilog=__doc__.split("\n\n", 2)[2],
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("kind", choices=KINDS, help="what the input file contains")
    parser.add_argument("input", type=Path, nargs="?",
                        help="syllabus or faculty file (.pdf, .docx, .txt, ...) - API mode")
    parser.add_argument("--prompt", action="store_true",
                        help="print the text to paste into the claude.ai chat (free mode)")
    parser.add_argument("--from-json", type=Path, metavar="REPLY",
                        help="file holding the JSON reply copied from the claude.ai chat (free mode)")
    parser.add_argument("-o", "--output", type=Path, help="output file (default: stdout)")
    parser.add_argument("-f", "--format", choices=WRITERS,
                        help="output format (default: from --output extension, else json)")
    parser.add_argument("-m", "--model", default=os.environ.get("CLAUDE_MODEL", DEFAULT_MODEL),
                        help=f"Claude model id for API mode (default: {DEFAULT_MODEL}, or $CLAUDE_MODEL)")
    args = parser.parse_args(argv)

    if sum([args.input is not None, args.prompt, args.from_json is not None]) != 1:
        parser.error("give exactly one of: an input file, --prompt, or --from-json REPLY")

    if args.prompt:
        sys.stdout.write(chat_prompt(args.kind))
        return 0

    if args.from_json:
        if not args.from_json.is_file():
            parser.error(f"reply file not found: {args.from_json}")
        try:
            data = parse_reply(args.from_json.read_text(encoding="utf-8"), args.kind)
        except ValueError as exc:  # includes pydantic.ValidationError
            print(f"error: {args.from_json} is not a valid {args.kind} reply:\n{exc}", file=sys.stderr)
            return 1
    else:
        if not args.input.is_file():
            parser.error(f"input file not found: {args.input}")
        data = extract_with_api(args)
        if data is None:
            return 1

    fmt = args.format or EXTENSIONS.get(args.output.suffix if args.output else "", "json")
    output = WRITERS[fmt](data)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
        print(f"wrote {args.output}", file=sys.stderr)
    else:
        sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
