#!/usr/bin/env python3
"""Render report/report.html to report/report.pdf with headless Chromium.

Usage:  python3 report/build_pdf.py
Needs:  pip install playwright   and a Chromium (set CHROMIUM=/path/to/chrome
        if Playwright's own browser is not installed).
"""

import os
import sys
from pathlib import Path

from playwright.sync_api import sync_playwright

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "report.html"
TARGET = HERE / "report.pdf"

FOOTER = """
<div style="width:100%; font-family:Inter, sans-serif; font-size:7.5pt; color:#5b6573;
            padding:0 0.9in; display:flex; justify-content:space-between;">
  <span>COSC 369 &middot; Office-Hours Schedule Generator</span>
  <span><span class="pageNumber"></span> / <span class="totalPages"></span></span>
</div>"""


def main() -> int:
    with sync_playwright() as p:
        executable = os.environ.get("CHROMIUM")
        browser = p.chromium.launch(executable_path=executable) if executable else p.chromium.launch()
        page = browser.new_page()
        page.goto(SOURCE.as_uri(), wait_until="networkidle")
        page.pdf(
            path=str(TARGET),
            format="Letter",
            print_background=True,
            prefer_css_page_size=True,
            display_header_footer=True,
            header_template="<span></span>",
            footer_template=FOOTER,
        )
        browser.close()
    print(f"wrote {TARGET.relative_to(Path.cwd()) if TARGET.is_relative_to(Path.cwd()) else TARGET}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
