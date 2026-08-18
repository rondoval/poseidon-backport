#!/usr/bin/env python3
"""Verify the GUI fleet stays inside the MUI 3.8 subset.

Why this exists
---------------
We COMPILE against the MUI 5 SDK (the 3.8 SDK is gcc-2.x `a6@' asm and bebbo's gcc
cannot parse it) but we RUN on muimaster.library 19 and up -- include/mui_compat.h
lowers MUIMASTER_VMIN from the SDK's 20 to 19 for exactly that reason.  Nothing
enforces the gap: the compiler happily accepts MUIA_Application_UsedClasses, and the
result is a tag muimaster 19 has never heard of, silently ignored on a machine we do
not have to test on.  This script is that enforcement.

What it checks
--------------
Three hard classes, any of which fails the build:

  * a MUI identifier reachable from our sources that MUI 3.8 does not define;
  * one the MUI 5 SDK annotates V20 or later (MUI 4.0 is V20, MUI 5.x is V21+);
  * an OpenLibrary(MUIMASTER_NAME, <literal>) that bypasses the mui_compat.h floor.

And one soft class -- warn only, `--strict' promotes it -- because its call-site
detection is a regex over one line and a false positive must never break a build:

  * an attribute whose access flags NARROWED in 3.8, used in the direction that was
    lost.  Symbol presence does not cover this: MUIA_Cycle_Entries exists in both
    SDKs but is `is.' on MUI 5 and `i..' on 3.8, so setting it is a no-op there.

Reachability, not occurrence
----------------------------
Nearly all GUI construction goes through SDK macros -- ScrollgroupObject, VGroup,
RegisterGroup, Label, PopButton -- so the MUI identifiers that actually matter never
appear in our sources at all.  A plain token scan sees `ScrollgroupObject' and misses
everything it expands to.  So we take the transitive closure: seed with every
identifier in the tree, then repeatedly pull in the identifiers named by the body of
any MUI 5 SDK macro reached, to fixpoint.  Real macro expansion is unnecessary
(arguments are never SDK names); the closure over-approximates, which is the safe
direction for a compatibility gate.

Intersecting the closure with "names the MUI 5 SDK defines, and we don't" is also what
keeps this free of a hand-maintained exemption list: the ~120 project-private
MUIM_Action_* / MUIM_DevWin_* / MUIM_PoPo_* tags, MUI_LPR_FULLDROP and the
MUIMASTER_BASE_NAME plumbing all fall out on their own.  (The corollary: hardcoding a
V20 tag's hex value under your own name would hide it from this check.  Don't.)

Comments and string literals are stripped before the scan, so naming a MUI 5 attribute
in prose is not a dependency.

There is deliberately no escape hatch.  The tree is the MUI 3.8 subset; if something
newer is ever genuinely needed, whoever needs it adds the runtime gating then, as a
considered change rather than a comment marker.

The oracle
----------
The MUI 5 side is read live from the SDK the build already uses ($MUI_INCLUDE_DIR),
never frozen -- it must track whatever the container ships.  The MUI 3.8 side is
vendored as scripts/mui38-symbols.tsv, because the 3.8 developer archive is not in the
container.  That asymmetry is deliberate: the 3.8 inventory is a property of a 1997
release and can never change, whereas a frozen MUI-5 side would silently stop covering
symbols a newer SDK adds.  Regenerate the vendored side with:

    python3 scripts/check-mui38.py --gen-inventory <mui38-SDK-root> > scripts/mui38-symbols.tsv

where <mui38-SDK-root> is an extracted mui38dev.lha (the tree containing
MUI/Developer/C/Include).

Encoding
--------
Every read is latin-1.  These sources are ISO-8859, and a UTF-8 read or a plain grep
does not fail on them -- it returns *nothing*, which turns "I found no violations"
into a confident lie. 

Usage:  scripts/check-mui38.py [--src ROOT] [--mui-sdk DIR] [--inventory TSV]
                               [--strict] [-v] [--gen-inventory MUI38_ROOT]
Exit status is 1 when the tree left the 3.8 subset.
"""

import argparse
import hashlib
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIRS = {".git", "dist", "presets", "docs"}

MUI38_VER = 19          # muimaster.library shipped with MUI 3.8
MUI4_VER = 20           # first version we may NOT rely on

# A version annotation only counts when the macro body is a bare hex tag value.
# libraries/mui.h embeds a verbatim Textinput.mcc header whose /* V19 */ /* V23 */
# /* V29 */ comments are that CLASS's version, not muimaster's; those all sit on
# MCC_TI_ID(n) bodies, so this rule alone excludes them.  We skip the block by name
# as well, belt and braces.
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)(\([^)]*\))?[ \t]*(.*)$", re.M)
ANNOT = re.compile(r"^(0x[0-9a-fA-F]{8})\b.*?/\*\s*V(\d+)\s+([isg.]{3})?", re.S)
ANNOT_M = re.compile(r"^(0x[0-9a-fA-F]{8})\b.*?/\*\s*V(\d+)\s*\*/", re.S)
PROTO = re.compile(r"^[A-Za-z_][\w \t*]*?\b(MUI_\w+)[ \t]*\(", re.M)
FDFUNC = re.compile(r"^(\w+)\s*\(", re.M)
IDENT = re.compile(r"\b([A-Za-z_]\w*)\b")
MUI_NAME = re.compile(r"^MUI[A-Z]?[A-Za-z0-9]*_\w+$|^MUI_\w+$")

MCC_BLOCK = ("TEXTINPUT_MCC_H",)


def read(path):
    """Latin-1, always.  See the module docstring."""
    return io.open(path, encoding="latin-1").read()


def strip_mcc(text):
    """Drop embedded third-party MCC headers, whose /* Vnn */ mean a class version."""
    for guard in MCC_BLOCK:
        m = re.search(r"^[ \t]*#[ \t]*ifndef[ \t]+" + guard + r"\b", text, re.M)
        if not m:
            continue
        depth, i, end = 0, m.start(), len(text)
        for cond in re.finditer(r"^[ \t]*#[ \t]*(ifn?def|if|endif)\b", text[i:], re.M):
            depth += 1 if cond.group(1) != "endif" else -1
            if depth == 0:
                end = i + cond.end()
                break
        text = text[:m.start()] + text[end:]
    return text


def headers(root):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if fn.endswith((".h", ".fd")):
                yield os.path.join(dirpath, fn)


def scan_sdk(root):
    """-> (bodies {name: macro body}, ver {name: V}, flags {name: 'isg'})

    Covers every header in the tree plus any .fd, so muimaster's own functions --
    which live in clib/ and inline/ rather than libraries/mui.h -- are inventoried
    alongside the tags.  (Getting this wrong costs two false positives, MUI_MakeObject
    and MUI_Request, both of which are inline wrappers rather than #defines.)
    """
    bodies, ver, flags = {}, {}, {}
    for path in headers(root):
        text = read(path)
        if path.endswith(".fd"):
            for m in FDFUNC.finditer(text):
                bodies.setdefault(m.group(1), "")
            continue
        clean = strip_mcc(text)
        for m in DEFINE.finditer(clean):
            name, body = m.group(1), m.group(3)
            bodies[name] = body
            a = ANNOT.match(body) or ANNOT_M.match(body)
            if a:
                ver[name] = int(a.group(2))
                if a.lastindex >= 3 and a.group(3):
                    flags[name] = a.group(3)
        for m in PROTO.finditer(clean):
            bodies.setdefault(m.group(1), "")
        for m in re.finditer(r"^extern\s+char\s+(MUIC_\w+)", clean, re.M):
            bodies.setdefault(m.group(1), "")
    return bodies, ver, flags


def project_sources(src):
    for dirpath, dirnames, filenames in os.walk(src):
        dirnames[:] = [d for d in dirnames
                       if d not in SKIP_DIRS and not d.startswith("build")]
        for fn in filenames:
            if fn.endswith((".c", ".h")):
                yield os.path.join(dirpath, fn)


def decomment(text):
    """Strip C comments and string/char literals.

    A MUI attribute named in a comment is documentation, not a dependency.
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
            out.append(" ")
        elif c in "\"'":
            quote, j = c, i + 1
            while j < n and text[j] != quote:
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            out.append(" ")
        else:
            out.append(c)
            i += 1
    return "".join(out)


def reachable(src, bodies, exclude):
    """Transitive closure of identifiers, expanding MUI 5 SDK macro bodies."""
    seen, work = set(), []
    for path in project_sources(src):
        for m in IDENT.finditer(decomment(read(path))):
            name = m.group(1)
            if name not in seen and name not in exclude:
                seen.add(name)
                work.append(name)
    while work:
        name = work.pop()
        body = bodies.get(name)
        if not body:
            continue
        for m in IDENT.finditer(body):
            sub = m.group(1)
            if sub not in seen:
                seen.add(sub)
                work.append(sub)
    return seen


def load_inventory(path):
    """-> (names, ver, flags) from the vendored MUI 3.8 TSV."""
    names, ver, flags = set(), {}, {}
    for line in io.open(path, encoding="latin-1"):
        if line.startswith("#") or not line.strip():
            continue
        parts = line.rstrip("\n").split("\t")
        name = parts[0]
        names.add(name)
        if len(parts) > 1 and parts[1] != "-":
            ver[name] = int(parts[1])
        if len(parts) > 2 and parts[2] != "-":
            flags[name] = parts[2]
    return names, ver, flags


def gen_inventory(mui38_root):
    bodies, ver, flags = scan_sdk(mui38_root)
    src = os.path.join(os.path.dirname(mui38_root.rstrip("/")), "")
    digest = "unknown"
    for cand in (os.path.join(ROOT, "..", "mui38dev.lha"),
                 os.path.expanduser("~/amiga/mui38dev.lha")):
        if os.path.exists(cand):
            digest = hashlib.sha256(open(cand, "rb").read()).hexdigest()
            break
    out = []
    out.append("# MUI 3.8 symbol inventory -- the oracle for scripts/check-mui38.py.")
    out.append("# Source: mui38dev.lha (MUI 3.8 developer archive, (c) 1993-1997 Stefan Stuntz),")
    out.append("#         sha256 %s" % digest)
    out.append("# Regenerate: python3 scripts/check-mui38.py --gen-inventory <mui38-SDK-root>"
               " > scripts/mui38-symbols.tsv")
    out.append("# Columns: name<TAB>version-introduced<TAB>access-flags  ('-' = not annotated)")
    out.append("# MUI 3.8 is muimaster.library 19; its headers annotate nothing above V18.")
    for name in sorted(bodies):
        out.append("%s\t%s\t%s" % (name,
                                   ver.get(name, "-"),
                                   flags.get(name, "-")))
    return "\n".join(out) + "\n"


SETTER = re.compile(r"\b(?:nnset|set|SetAttrs|SetSuperAttrs|SetAttrsA)\s*\(")
GETTER = re.compile(r"\b(?:get|GetAttr)\s*\(")
OPENLIB = re.compile(r"OpenLibrary\s*\(\s*MUIMASTER_NAME\s*,\s*(\d+)\s*\)")


def scan_call_sites(src, flags5, flags38):
    """-> (narrowing warnings, hard OpenLibrary findings)"""
    narrowed = {n: (flags5[n], flags38[n])
                for n in flags5 if n in flags38 and flags5[n] != flags38[n]}
    warn, hard = [], []
    for path in project_sources(src):
        rel = os.path.relpath(path, src)
        for lineno, line in enumerate(io.open(path, encoding="latin-1"), 1):
            m = OPENLIB.search(line)
            if m:
                hard.append("%s:%d: OpenLibrary(MUIMASTER_NAME, %s) hardcodes a version, "
                            "bypassing the mui_compat.h floor -- use MUIMASTER_VMIN"
                            % (rel, lineno, m.group(1)))
            for name, (f5, f38) in narrowed.items():
                if name not in line:
                    continue
                if "s" in f5 and "s" not in f38 and SETTER.search(line):
                    warn.append("%s:%d: %s is settable on MUI 5 (%s) but init-only on "
                                "MUI 3.8 (%s) -- this set is ignored there"
                                % (rel, lineno, name, f5, f38))
                if "g" in f5 and "g" not in f38 and GETTER.search(line):
                    warn.append("%s:%d: %s is gettable on MUI 5 (%s) but not on "
                                "MUI 3.8 (%s) -- this get returns nothing there"
                                % (rel, lineno, name, f5, f38))
    return warn, hard


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build", nargs="?", help="ignored; accepted so this can be chained "
                                             "after check-regargs.py")
    ap.add_argument("--src", default=ROOT, help="source root (default: repo root)")
    ap.add_argument("--mui-sdk", default=os.environ.get("MUI_INCLUDE_DIR"),
                    help="MUI 5 SDK include dir (default: $MUI_INCLUDE_DIR)")
    ap.add_argument("--inventory", default=os.path.join(ROOT, "scripts", "mui38-symbols.tsv"))
    ap.add_argument("--strict", action="store_true",
                    help="treat access-flag narrowing as a failure too")
    ap.add_argument("--gen-inventory", metavar="MUI38_ROOT",
                    help="print a fresh MUI 3.8 inventory and exit")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if args.gen_inventory:
        sys.stdout.write(gen_inventory(args.gen_inventory))
        return 0

    sdk = args.mui_sdk
    if not sdk or not os.path.isdir(sdk):
        print("check-mui38: no MUI 5 SDK found (set MUI_INCLUDE_DIR or --mui-sdk), skipping",
              file=sys.stderr)
        return 0
    if not os.path.exists(args.inventory):
        print("check-mui38: no MUI 3.8 inventory at %s, skipping" % args.inventory,
              file=sys.stderr)
        return 0

    bodies5, ver5, flags5 = scan_sdk(sdk)
    names38, _ver38, flags38 = load_inventory(args.inventory)

    # Anything the project #defines itself is ours, not a dependency on the SDK's
    # version of it -- MUIMASTER_BASE_NAME, MUI_LPR_FULLDROP, the private tag bases.
    ours = set()
    for path in project_sources(args.src):
        for m in DEFINE.finditer(read(path)):
            ours.add(m.group(1))

    scope = reachable(args.src, bodies5, ours) & (set(bodies5) - ours)

    hard, warn = [], []
    for name in sorted(scope):
        if not MUI_NAME.match(name):
            continue                       # exec/intuition/utility names pulled in via bodies
        v = ver5.get(name)
        if v is not None and v >= MUI4_VER:
            hard.append("%s is V%d -- MUI 3.8 is V%d, so it does not exist there"
                        % (name, v, MUI38_VER))
        elif name not in names38:
            hard.append("%s is not defined by the MUI 3.8 SDK" % name)

    w, h = scan_call_sites(args.src, flags5, flags38)
    warn += w
    hard += h

    if args.verbose:
        print("check-mui38: %d MUI 5 SDK identifiers, %d reachable and in scope, "
              "%d in the MUI 3.8 inventory" % (len(bodies5), len(scope), len(names38)))

    for w_ in warn:
        print("check-mui38: WARN %s" % w_)
    for h_ in hard:
        print("check-mui38: FAIL %s" % h_)

    failed = len(hard) + (len(warn) if args.strict else 0)
    print("check-mui38: %d symbol(s) outside the MUI 3.8 subset, %d access-flag warning(s)"
          % (len(hard), len(warn)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
