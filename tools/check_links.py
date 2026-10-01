#!/usr/bin/env python3
"""
check_links.py - verify that every *link* in the course resolves.

    python tools/check_links.py             # check markdown links and images
    python tools/check_links.py --external  # also list the external URLs used

Only real links are checked: `[text](target)` and `![alt](target)`. Backticked
code spans such as `src/cpu.c` are prose, not links, and are ignored - otherwise
every mention of an identifier looks like a broken link.

A relative target is accepted if it resolves against the document's own
directory OR against the course root, because both styles are in use here.
External URLs are listed but never fetched: this environment gets HTTP 403 from
gbdev.io even though the pages are fine in a browser.

Exit code is 0 when everything resolves, 1 otherwise. Run it after renaming or
adding any document.
"""

import argparse
import os
import re
import sys

LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)\)")
SKIP_DIRS = {"build", ".git", "__pycache__"}


def relative_targets(text):
    for match in LINK.finditer(text):
        target = match.group(1).strip()
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        target = target.split("#", 1)[0]
        # Globs and directory listings are illustrative, not links.
        if not target or any(ch in target for ch in "*{}<>"):
            continue
        yield target


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--external", action="store_true",
                    help="also print the external URLs referenced")
    ap.add_argument("root", nargs="?", default=".")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    checked = broken = files = 0
    external = set()
    problems = []

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in sorted(filenames):
            if not name.endswith(".md"):
                continue
            path = os.path.join(dirpath, name)
            files += 1
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()

            if args.external:
                for match in re.finditer(r"https?://[^\s)>`\]]+", text):
                    external.add(match.group(0).rstrip(".,"))

            for target in relative_targets(text):
                checked += 1
                candidates = [
                    os.path.normpath(os.path.join(dirpath, target)),
                    os.path.normpath(os.path.join(root, target)),
                ]
                if any(os.path.exists(c) for c in candidates):
                    continue
                broken += 1
                problems.append((os.path.relpath(path, root).replace("\\", "/"), target))

    for rel, target in problems:
        print("BROKEN  %-46s -> %s" % (rel, target))

    print("\n%d markdown files, %d relative links checked, %d broken"
          % (files, checked, broken))

    if args.external:
        print("\n%d distinct external URLs referenced:" % len(external))
        for url in sorted(external):
            print("  %s" % url)

    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
