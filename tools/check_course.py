#!/usr/bin/env python3
"""
check_course.py - verify that the course is internally consistent.

check_links.py proves that every hyperlink resolves. This script proves the harder
property: that the LADDER, the LESSONS and the TEST SUITE agree with each other.

Checks
  1. every lessons/L*.md carries the eight required markers
  2. every lesson's stated time budget sums to <= 90 minutes
  3. every test name a lesson *promises* exists in gb/tests/ and is a real TEST(),
     not a TEST_TODO stub
  4. no TEST_TODO stubs and no unauthored lesson markers remain
  5. every lesson file is linked from LESSONS.md
  6. LESSONS.md's ladder rows name only tests that exist
  7. the archived milestone briefs are still present in milestones/

A note on what counts as a promise: only the names on a lesson's
"**Tests that must go green**" line, plus the test names in LESSONS.md's ladder rows.
Names mentioned elsewhere in prose are illustrative and are deliberately NOT checked -
a lesson's "if it fails" table contains deliberate near-misses such as
`m00_smoke_build` (a typo'd filter) and wildcard families such as `m01_bus_*`.

Exit code 0 when the course is consistent, 1 when it is not. Standard library only.

    python tools/check_course.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LESSONS = os.path.join(ROOT, "lessons")
TESTS = os.path.join(ROOT, "gb", "tests")
MILESTONES = os.path.join(ROOT, "milestones")
LADDER = os.path.join(ROOT, "LESSONS.md")

# The eight markers every lesson must carry. The third is the explicit "what matters
# for the code you are about to write" highlight the course promises.
MARKERS = [
    (r"^\*\*Time\*\*", "Time budget"),
    (r"Tests that must go green", "the 'tests that must go green' promise"),
    (r"What matters for the code you are about to write", "the 'what matters' highlight"),
    (r"^## 1\. Read this", "section 1 (Read this)"),
    (r"^## 2\. Your task", "section 2 (Your task)"),
    (r"^## 3\. Prove it", "section 3 (Prove it)"),
    (r"^## 4\. If it fails", "section 4 (If it fails)"),
    (r"^## 5\. Done when", "section 5 (Done when)"),
]

TEST_NAME = r"m\d{2}_[a-z0-9_*]*"
TEST_RE = re.compile(r"^TEST\((\w+)\)", re.M)
STUB_RE = re.compile(r"^TEST_TODO\((\w+)", re.M)
TIME_RE = re.compile(r"~(\d+)\s*min")


def read(path):
    with open(path, "r", encoding="utf-8") as fh:
        return fh.read()


def promised_in_lesson(src):
    """The test names a lesson promises: its 'must go green' paragraph."""
    m = re.search(r"\*\*Tests that must go green\*\*(.*?)(?:\n\s*\n|\Z)",
                  src, re.S)
    if not m:
        return set()
    names = set()
    for span in re.findall(r"`([^`]+)`", m.group(1)):
        names.update(re.findall(TEST_NAME, span))
    return names


def check_name(name, real_tests, promised_wildcards, where, problems):
    """Exact names must exist; `m01_bus_*`/`m01_bus_` forms only need a prefix match."""
    prefix = name.rstrip("*_")
    if name.endswith(("*", "_")):
        if not any(t.startswith(prefix) for t in real_tests):
            problems.append(f"{where}: promises the family '{name}', which matches no test")
        else:
            promised_wildcards.add(name)
        return
    if name not in real_tests:
        problems.append(f"{where}: promises '{name}', which does not exist in gb/tests/")


def main():
    problems = []

    # ---- the test suite's vocabulary -----------------------------------------
    real_tests, stub_tests = set(), set()
    for name in sorted(f for f in os.listdir(TESTS) if f.endswith(".c")):
        src = read(os.path.join(TESTS, name))
        real_tests.update(TEST_RE.findall(src))
        stub_tests.update(STUB_RE.findall(src))

    if not real_tests:
        problems.append("no TEST() cases found in gb/tests/ - wrong directory?")

    # ---- every lesson --------------------------------------------------------
    lesson_files = sorted(f for f in os.listdir(LESSONS) if f.endswith(".md"))
    promises = 0
    wildcards = set()

    for name in lesson_files:
        src = read(os.path.join(LESSONS, name))

        for pattern, label in MARKERS:
            if not re.search(pattern, src, re.M):
                problems.append(f"{name}: missing {label}")

        time_line = re.search(r"^\*\*Time\*\*.*$", src, re.M)
        if time_line:
            minutes = [int(m) for m in TIME_RE.findall(time_line.group(0))]
            if minutes and sum(minutes) > 90:
                problems.append(f"{name}: time budget is {sum(minutes)} minutes (cap is 90)")

        for test in sorted(promised_in_lesson(src)):
            promises += 1
            if test in stub_tests:
                problems.append(f"{name}: promises '{test}', still a TEST_TODO stub")
            else:
                check_name(test, real_tests, wildcards, name, problems)

        if "authoring pending" in src:
            problems.append(f"{name}: still marked authoring pending")

    if stub_tests:
        problems.append(f"{len(stub_tests)} TEST_TODO stub(s) remain: "
                        + ", ".join(sorted(stub_tests)))

    # ---- the ladder ----------------------------------------------------------
    if not os.path.exists(LADDER):
        problems.append("LESSONS.md is missing")
    else:
        ladder = read(LADDER)

        for name in lesson_files:
            if name not in ladder:
                problems.append(f"{name} is never linked from LESSONS.md")

        for target in sorted(set(re.findall(r"lessons/([A-Za-z0-9._-]+\.md)", ladder))):
            if not os.path.exists(os.path.join(LESSONS, target)):
                problems.append(f"LESSONS.md links lessons/{target}, which does not exist")

        for test in sorted(set(re.findall(TEST_NAME, ladder))):
            check_name(test, real_tests, wildcards, "LESSONS.md", problems)

        if "| to author" in ladder:
            problems.append("LESSONS.md still has a 'to author' status cell")

    # ---- the archived briefs -------------------------------------------------
    if not os.path.isdir(MILESTONES):
        problems.append("milestones/ is missing - the briefs were supposed to move there")
    else:
        briefs = [f for f in os.listdir(MILESTONES) if f.endswith(".md")]
        if len(briefs) < 12:
            problems.append(f"milestones/ holds only {len(briefs)} briefs; 13 were archived")

    # ---- report --------------------------------------------------------------
    print(f"lessons:  {len(lesson_files)}")
    print(f"tests:    {len(real_tests)} real, {len(stub_tests)} stub")
    print(f"promises: {promises} test references, incl. {len(wildcards)} wildcard families")
    print(f"briefs:   {len(os.listdir(MILESTONES)) if os.path.isdir(MILESTONES) else 0} in milestones/")

    if problems:
        print("\nPROBLEMS:")
        for p in problems:
            print(f"  - {p}")
        print(f"\n{len(problems)} problem(s).")
        return 1

    print("\nOK: the ladder, the lessons and the test suite agree.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
