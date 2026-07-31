#!/usr/bin/env python3
"""
conf2sfd.py — convert an AROS genmodule .conf into an .sfd (for sfdc) and/or a
C funcTable[] in LVO order. Reused for poseidon, usbclass and the 30 classes.

Usage:
  conf2sfd.py <file.conf> --modname <name> --base <_XxxBase> [--mode sfd|functable]

The functionlist body is copied VERBATIM (same `name(args) (REGS)` syntax sfdc
wants). Trailing `##begin class` (HIDD) blocks are dropped. The cdef vararg
stubs (`... __stackparm`) are NOT emitted (client tagcall conveniences — TODO).
"""
import sys, argparse, re

def parse(path):
    cfg, includes, funcs, varargs = {}, [], [], []
    section = None
    for raw in open(path):
        line = raw.rstrip("\n")
        s = line.strip()
        if s.startswith("##begin"):
            section = s.split(None, 1)[1] if len(s.split()) > 1 else "?"
            # stop entirely once the HIDD class blocks begin
            if section == "class":
                break
            continue
        if s.startswith("##end"):
            section = None
            continue
        if section == "config":
            if s and not s.startswith("#"):
                k, _, v = s.partition(" ")
                cfg[k] = v.strip()
        elif section == "cdef":
            if s.startswith("#include"):
                includes.append(s[len("#include"):].strip())
            elif "..." in s and "(" in s:                 # vararg convenience stub
                d = s.replace("__stackparm", "").rstrip("; ").strip()
                varargs.append(d)
        elif section == "functionlist":
            if s and not s.startswith("#"):
                funcs.append(s)            # verbatim "ret name(args) (REGS)"
    return cfg, includes, funcs, varargs

NAME_RE = re.compile(r'\b(\w+)\s*\(')

FUNC_RE = re.compile(r'^\s*(.+?)\b(\w+)\s*\(.*\)\s*\(([^)]*)\)\s*$')

def emit_sfd(cfg, includes, funcs, varargs, modname, base):
    # map: A-variant name -> the vararg declaration that wraps it (name+"A")
    va_for = {}
    for v in varargs:
        m = NAME_RE.search(v)
        if m:
            va_for[m.group(1) + "A"] = v
    out = []
    out.append(f"==id $Id: {modname}.sfd {cfg.get('version','1.0')} $")
    out.append(f'* "{modname}.library"')
    out.append(f"==base {base}")
    out.append(f"==basetype struct Library *")
    out.append(f"==libname {modname}.library")
    out.append(f"==bias 30")
    out.append(f"==public")
    for inc in includes:
        out.append(f"==include {inc}")
    for f in funcs:
        out.append(f)
        m = FUNC_RE.match(f)
        if m and m.group(2) in va_for:
            regs = m.group(3)                       # reuse the A-variant's regs
            out.append("==varargs")
            out.append(f"{va_for[m.group(2)]} ({regs})")
    out.append("==end")
    return "\n".join(out) + "\n"

def emit_functable(cfg, funcs, modname):
    """Emit the C funcTable[] body: the 96 entries in LVO order (the 4 std
       vectors + (APTR)-1 are added by hand in *_main.c)."""
    names = []
    for f in funcs:
        m = FUNC_RE.match(f)
        if not m:
            sys.stderr.write(f"WARN: unparsed functionlist line: {f}\n"); continue
        names.append(m.group(2))
    lines = [f"/* generated from {modname}.conf functionlist — LVO order = ABI */"]
    for n in names:
        lines.append(f"    (APTR){n},")
    return "\n".join(lines) + f"\n/* {len(names)} functions */\n"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("conf")
    ap.add_argument("--modname", required=True)
    ap.add_argument("--base", required=True)
    ap.add_argument("--mode", choices=["sfd", "functable"], default="sfd")
    a = ap.parse_args()
    cfg, includes, funcs, varargs = parse(a.conf)
    if a.mode == "sfd":
        sys.stdout.write(emit_sfd(cfg, includes, funcs, varargs, a.modname, a.base))
    else:
        sys.stdout.write(emit_functable(cfg, funcs, a.modname))

if __name__ == "__main__":
    main()
