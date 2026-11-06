#!/usr/bin/env python3
"""
Runs the examples of every page of the manual (man/*.txt) and checks that
they give the output written there, so the manual never lies about the
language. Under the EXAMPLES heading of a page (and any other heading
whose name ends in EXAMPLES), indented by 4 spaces:

  REPL sessions:  lines starting with ">>> " or "... " are typed in the
                  REPL; the lines after them are what it shows.
  Programs:       "$ cat name.pce" then the file, then
                  "$ ./pycee name.pce [args] [< input]" then what it
                  prints (and "$ echo $?" then the exit code, if wanted).
                  Without "<", the program reads nothing (/dev/null).

An error is written as its first line only ("TypeError: ..."): where it
happened ("(<stdin:3>, line 1)" and the code under it) is not compared.
An expected line that ends in "..." only has to be the start of the line
shown (for messages too long for the page).

    python3 tools/check_man.py [./pycee] [topic...]
"""

import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAN_DIR = os.path.join(ROOT, "man")
HEADING = re.compile(r"^[A-Z][A-Z ]*$")
WHERE = re.compile(r" \((<stdin:\d+>|[^()]*\.pce), line \d+\)$")


def pages(only):
    for name in sorted(os.listdir(MAN_DIR)):
        path = os.path.join(MAN_DIR, name)
        if not name.endswith(".txt") or not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as f:
            first, _, body = f.read().partition("\n")
        topics = first[2:].split()
        if not only or set(topics) & only:
            yield topics[0], body


def example_lines(body):
    """The lines of every EXAMPLES section, without their 4 spaces."""
    inside = False
    for line in body.split("\n"):
        if HEADING.match(line):
            inside = line.strip().endswith("EXAMPLES")
            continue
        if inside:
            yield line[4:] if line.startswith("    ") else line.strip()


def blocks(lines):
    """Splits into ("repl", inputs, expected) and ("prog", files, cmd, expected)."""
    lines = list(lines)
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith(">>> "):
            inputs, expected = [], []
            while i < len(lines) and not lines[i].startswith("$ "):
                if lines[i].startswith(">>> ") or lines[i].startswith("... "):
                    inputs.append(lines[i][4:])
                elif lines[i].strip():
                    expected.append(lines[i])
                i += 1
            yield ("repl", inputs, expected)
        elif line.startswith("$ cat "):
            files = {}
            while i < len(lines) and lines[i].startswith("$ cat "):
                name = lines[i][6:].strip()
                i += 1
                content = []
                while i < len(lines) and not lines[i].startswith("$ "):
                    content.append(lines[i])
                    i += 1
                files[name] = "\n".join(content).rstrip() + "\n"
            runs = []
            while i < len(lines) and lines[i].startswith("$ ") and not lines[i].startswith("$ cat "):
                cmd = lines[i][2:].strip()
                i += 1
                out = []
                while i < len(lines) and not lines[i].startswith("$ ") and not lines[i].startswith(">>> "):
                    if lines[i].strip():
                        out.append(lines[i])
                    i += 1
                runs.append((cmd, out))
            yield ("prog", files, runs)
        else:
            i += 1


def normalize(output):
    """Errors keep only their first line, without where they happened."""
    result = []
    lines = output.split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]
        m = WHERE.search(line)
        if m:
            result.append(line[:m.start()])
            i += 3
            continue
        if line.strip():
            result.append(line.rstrip())
        i += 1
    return result


def strip_traceback(lines):
    """A traceback shows only its last line (the error) in the manual."""
    out, skipping = [], False
    for line in lines:
        if line == "Traceback (most recent call last):":
            skipping = True
            continue
        if skipping and (line.startswith("  ") or line.startswith("    ")):
            continue
        skipping = False
        out.append(line)
    return out


def run_repl(pycee, inputs, cwd):
    p = subprocess.run([pycee], input="\n".join(inputs) + "\n", stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True, cwd=cwd, timeout=30)
    return strip_traceback(normalize(p.stdout))


def run_prog(pycee, files, runs, cwd):
    for name, content in files.items():
        with open(os.path.join(cwd, name), "w", encoding="utf-8") as f:
            f.write(content)
    got, want = [], []
    last = 0
    for cmd, out in runs:
        want += out
        if cmd == "echo $?":
            got.append(str(last))
            continue
        args = cmd.split()
        if args[0] != "./pycee":
            raise ValueError(f"unknown command: {cmd}")
        stdin = subprocess.DEVNULL
        if "<" in args:
            k = args.index("<")
            stdin = open(os.path.join(cwd, args[k + 1]), encoding="utf-8")
            args = args[:k]
        p = subprocess.run([pycee] + args[1:], stdin=stdin, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, cwd=cwd, timeout=30)
        last = p.returncode
        got += strip_traceback(normalize(p.stdout))
    return got, want


def matches(got, want):
    if len(got) != len(want):
        return False
    for g, w in zip(got, want):
        if w.endswith("...") and g.startswith(w[:-3]):
            continue
        if g != w:
            return False
    return True


def main():
    args = sys.argv[1:]
    pycee = os.path.abspath(args.pop(0)) if args and "/" in args[0] else os.path.join(ROOT, "pycee")
    failed = checked = 0
    for topic, body in pages(set(args)):
        with tempfile.TemporaryDirectory() as cwd:
            for block in blocks(example_lines(body)):
                checked += 1
                if block[0] == "repl":
                    got, want = run_repl(pycee, block[1], cwd), block[2]
                else:
                    got, want = run_prog(pycee, block[1], block[2], cwd)
                if not matches(got, want):
                    failed += 1
                    print(f"FAIL: man({topic})")
                    for w, g in zip(want + [""] * len(got), got + [""] * len(want)):
                        if w != g:
                            print(f"    expected: {w}\n    got:      {g}")
                            break
    print(f"man examples: {checked - failed} passed, {failed} failed")
    sys.exit(1 if failed else 0)


main()
