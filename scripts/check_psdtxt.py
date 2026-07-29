#!/usr/bin/env python3
"""Audit every psdTxt() pair in the tree.

psdTxt(plain, flavour) picks one of two format strings, but the two share a
single argument list and psdAddErrorMsg() carries no format attribute (the sfd
cannot express one). A swapped %s/%ld is therefore a wild pointer dereference in
pPutChar(), not a cosmetic bug, and the compiler will not say a word.

The rule this enforces:

    the plain variant's format-specifier sequence must be an exact PREFIX of the
    flavour's -- same specifiers, same order, dropping only from the end.

It also rejects nested psdTxt()/PSD_*_TXT() calls, which are always a
double-wrap mistake rather than something anyone means.

Run it before a release build:  python3 scripts/check_psdtxt.py
Exits non-zero on any violation.  Not wired into CMake -- it parses text, and a
false positive should never be able to break a build.
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIRS = {"build", ".git", "AROS-BASELINE"}

# The shared wrappers in classes/common.h, with the plain half they expand to.
SHARED = {
    "PSD_BOUND_TXT":    "Bound '%s' to %s unit %ld.",
    "PSD_BOUND1_TXT":   "Bound '%s'.",
    "PSD_RELEASED_TXT": "Released '%s'.",
    "PSD_OK_TXT":       "OK",
}

CALL = re.compile(r"\b(psdTxt|" + "|".join(SHARED) + r")\s*\(")
SPEC = re.compile(r"%[-+ #0-9.*]*(?:h|hh|l|ll|L|z|j|t)?([diouxXeEfgGaAcspn%])")


def strings_at(src, i):
    """Collect the top-level C string literals of the call whose '(' is at i.

    Returns (list_of_strings_per_argument, index_just_past_the_closing_paren,
    nested_flag). Adjacent literals are concatenated, exactly as the compiler
    does; anything that is not a literal contributes None for that argument.
    """
    depth = 0
    args, cur, lit = [], [], None
    nested = False
    n = len(src)
    while i < n:
        c = src[i]
        if c == '"':
            j, buf = i + 1, []
            while j < n:
                if src[j] == "\\":
                    buf.append(src[j:j + 2]); j += 2; continue
                if src[j] == '"':
                    break
                buf.append(src[j]); j += 1
            lit = (lit or "") + "".join(buf)
            i = j + 1
            continue
        if c == "'":                      # char literal -- skip it whole
            j = i + 1
            while j < n and src[j] != "'":
                j += 2 if src[j] == "\\" else 1
            i = j + 1
            continue
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                cur.append(lit); args.append(cur)
                return args, i + 1, nested
        elif c == "," and depth == 1:
            cur.append(lit); args.append(cur); cur, lit = [], None
            i += 1
            continue
        elif c.isalpha() or c == "_":
            j = i
            while j < n and (src[j].isalnum() or src[j] == "_"):
                j += 1
            if depth >= 1 and CALL.match(src, i):
                nested = True
            i = j
            continue
        i += 1
    return args, n, nested


def specs(s):
    return "".join(m.group(1) for m in SPEC.finditer(s) if m.group(1) != "%")


def main():
    problems = []
    checked = 0
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if not fn.endswith((".c", ".h")):
                continue
            path = os.path.join(dirpath, fn)
            rel = os.path.relpath(path, ROOT)
            if rel == os.path.join("classes", "common.h"):
                continue                  # where the shared macros are defined
            src = io.open(path, encoding="latin-1").read()
            for m in CALL.finditer(src):
                name = m.group(1)
                args, _, nested = strings_at(src, m.end() - 1)
                line = src.count("\n", 0, m.start()) + 1
                where = "%s:%d" % (rel, line)
                if nested:
                    problems.append("%s: nested %s() -- double wrap" % (where, name))
                    continue
                flat = [a[0] if a else None for a in args]
                if name == "psdTxt":
                    if len(flat) != 2:
                        continue          # not a literal pair; nothing to check
                    plain, flavour = flat[0], flat[1]
                else:
                    if len(flat) != 1:
                        continue
                    plain, flavour = SHARED[name], flat[0]
                if plain is None or flavour is None:
                    continue
                checked += 1
                sp, sf = specs(plain), specs(flavour)
                if not sf.startswith(sp):
                    problems.append(
                        "%s: %s specifiers '%s' are not a prefix of '%s'\n"
                        "        plain:   %s\n        flavour: %s"
                        % (where, name, sp, sf, plain[:78], flavour[:78]))

    for p in problems:
        print("FAIL " + p)
    print("checked %d psdTxt() pairs, %d problem(s)" % (checked, len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
