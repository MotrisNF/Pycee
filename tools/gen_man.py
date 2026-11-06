#!/usr/bin/env python3
"""
Builds src/man/man_data.c from the manual's entries, man/*.txt (not
man/_notes/). Each entry is a text file whose first line is "# <topic>"
(what is typed inside man(...): "string", "string.lower", "operators"),
or several topics that share the page ("# list.map tuple.map"); the rest
is the page as it is shown. Under its NAME heading, the first
line "topic - summary" gives the summary used by lists and the index.

All the pages are compressed together (an LZ77 of our own: see
src/man/man.c for the format and the decompressor) so the binary stays
small; the index of topics is kept as it is, for looking up without
decompressing. The entry "pycee" gets the index of every entry appended.

    python3 tools/gen_man.py           # writes src/man/man_data.c
    python3 tools/gen_man.py --check   # exit 1 if it is out of date
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAN_DIR = os.path.join(ROOT, "man")
OUT = os.path.join(ROOT, "src", "man", "man_data.c")
TOPIC = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)?$")
WIDTH = 79


def fail(msg):
    sys.exit("gen_man: " + msg)


def read_entries():
    entries = {}
    for name in sorted(os.listdir(MAN_DIR)):
        path = os.path.join(MAN_DIR, name)
        if not name.endswith(".txt") or not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as f:
            text = f.read()
        first, _, body = text.partition("\n")
        if not first.startswith("# "):
            fail(f"{name}: the first line must be '# <topic> [<topic>...]'")
        topics = first[2:].split()
        body = body.strip("\n") + "\n"
        for i, line in enumerate(body.split("\n"), 2):
            if len(line) > WIDTH:
                fail(f"{name}, line {i}: longer than {WIDTH} characters")
            if "\t" in line:
                fail(f"{name}, line {i}: a tab (use spaces)")
        summary = summary_of(name, topics[0], body)
        for topic in topics:
            if not TOPIC.match(topic):
                fail(f"{name}: '{topic}' is not a valid topic (name or type.name)")
            if topic in entries:
                fail(f"{name}: topic '{topic}' is repeated")
            entries[topic] = {"text": body, "summary": summary}
    if "pycee" not in entries:
        fail("there must be an entry 'pycee' (man() shows it)")
    return entries


def summary_of(name, topic, body):
    lines = body.split("\n")
    for i, line in enumerate(lines):
        if line.strip() == "NAME":
            for nxt in lines[i + 1:]:
                if nxt.strip():
                    head, sep, rest = nxt.strip().partition(" - ")
                    if not sep:
                        fail(f"{name}: under NAME, write '{topic} - <summary>'")
                    return rest.strip()
    fail(f"{name}: no NAME section")


def index_text(entries):
    width = max(len(t) for t in entries) + 2
    lines = ["", "INDEX", f"    Every entry ({len(entries)}): type man(topic) to read one.", ""]
    for topic in sorted(entries, key=lambda t: (t.lower(), t)):
        line = "    " + topic.ljust(width) + entries[topic]["summary"]
        if len(line) > WIDTH:
            line = line[:WIDTH - 3] + "..."
        lines.append(line)
    return "\n".join(lines) + "\n"


def compress(data):
    """Tokens: c < 0x80: c + 1 literal bytes follow; c >= 0x80: copy
    (c & 0x7f) + 4 bytes from offset o (2 bytes, little endian) back."""
    out = bytearray()
    lit = bytearray()
    table = {}
    i, n = 0, len(data)

    def flush():
        while lit:
            chunk = lit[:128]
            out.append(len(chunk) - 1)
            out.extend(chunk)
            del lit[:128]

    def remember(pos):
        if pos + 3 <= n:
            table.setdefault(bytes(data[pos:pos + 3]), []).append(pos)

    while i < n:
        best_len, best_off = 0, 0
        for j in reversed(table.get(bytes(data[i:i + 3]), [])[-64:]):
            off = i - j
            if off > 0xFFFF:
                break
            length = 0
            while length < 131 and i + length < n and data[j + length] == data[i + length]:
                length += 1
            if length > best_len:
                best_len, best_off = length, off
        if best_len >= 4:
            flush()
            out.extend((0x80 | (best_len - 4), best_off & 0xFF, best_off >> 8))
            for k in range(best_len):
                remember(i + k)
            i += best_len
        else:
            lit.append(data[i])
            remember(i)
            i += 1
    flush()
    return bytes(out)


def decompress(blob):
    out = bytearray()
    i = 0
    while i < len(blob):
        c = blob[i]
        if c < 0x80:
            out.extend(blob[i + 1:i + 2 + c])
            i += 2 + c
        else:
            length, off = (c & 0x7F) + 4, blob[i + 1] | (blob[i + 2] << 8)
            for _ in range(length):
                out.append(out[-off])
            i += 3
    return bytes(out)


def c_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate():
    entries = read_entries()
    entries["pycee"]["text"] += index_text(entries)
    topics = sorted(entries)
    text = bytearray()
    spans = {}
    placed = {}
    for t in topics:
        data = entries[t]["text"].encode("utf-8")
        if data not in placed:
            placed[data] = len(text)
            text.extend(data)
        spans[t] = (placed[data], len(data))
    blob = compress(bytes(text))
    if decompress(blob) != bytes(text):
        fail("the compressor is broken")
    lines = [
        "/* Generated by tools/gen_man.py from the man/ pages: do not edit. */",
        "",
        '#include "man/man.h"',
        "",
        f"/* {len(text)} bytes of text, {len(blob)} compressed. */",
        f"const size_t        g_man_text_len = {len(text)};",
        f"const size_t        g_man_blob_len = {len(blob)};",
        "const unsigned char g_man_blob[] = {",
    ]
    for k in range(0, len(blob), 16):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in blob[k:k + 16]) + ",")
    lines += ["};", "", "/* Sorted by topic (strcmp), for a binary search. */",
              "const t_man_entry   g_man_entries[] = {"]
    for t in topics:
        off, ln = spans[t]
        lines.append(f"    {{{c_string(t)}, {c_string(entries[t]['summary'])}, {off}, {ln}}},")
    lines += ["};", "", f"const int           g_man_count = {len(topics)};", ""]
    return "\n".join(lines), len(topics), len(text), len(blob)


def main():
    code, count, raw, packed = generate()
    if "--check" in sys.argv:
        try:
            with open(OUT, encoding="utf-8") as f:
                same = f.read() == code
        except OSError:
            same = False
        if not same:
            sys.exit("man: src/man/man_data.c is out of date: run python3 tools/gen_man.py")
        print(f"man: {count} entries, up to date")
        return
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(code)
    print(f"wrote src/man/man_data.c: {count} entries, {raw} bytes of text, {packed} compressed")


main()
