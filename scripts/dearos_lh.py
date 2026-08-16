#!/usr/bin/env python3
"""
dearos_lh.py — rewrite AROS genmodule calling-convention macros to plain bebbo-C
register-argument functions. Reused for poseidon, usbclass and the 30 classes.

  AROS_LH2(STRPTR, psdCopyStrFmtA,
           AROS_LHA(CONST_STRPTR, fmtstr, A0),
           AROS_LHA(RAWARG, fmtdata, A1),
           LIBBASETYPEPTR, ps, 68, psd)
  {  AROS_LIBFUNC_INIT  ...body...  AROS_LIBFUNC_EXIT  }
->
  STRPTR psdCopyStrFmtA(CONST_STRPTR fmtstr asm("a0"), RAWARG fmtdata asm("a1"),
                        struct PsdBase *ps asm("a6"))
  {  ...body...  }

AROS_UFHn (hook callbacks) -> same, but no libbase/a6 trailer.
Also: drop *_INIT/*_EXIT lines & ADD2*LIB lines; GM_UNIQUENAME(x)->x;
LIBBASETYPEPTR-><libbasetype>; inject MOD_NAME_STRING/VERSION_STRING defines.
"""
import sys, re, argparse

def split_top(s):
    """split on top-level commas (depth 0)"""
    out, depth, cur = [], 0, ""
    for c in s:
        if c in "([": depth += 1
        elif c in ")]": depth -= 1
        if c == "," and depth == 0:
            out.append(cur); cur = ""
        else:
            cur += c
    if cur.strip() or out:
        out.append(cur)
    return [t.strip() for t in out]

def match_paren(text, open_idx):
    """given index of '(', return index of matching ')'"""
    depth = 0
    for i in range(open_idx, len(text)):
        if text[i] == "(": depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0: return i
    raise ValueError("unbalanced parens")

def parse_arg(lha):
    """AROS_LHA(type, name, REG) or AROS_UFHA(...) -> 'type name asm("reg")'"""
    inside = lha[lha.index("(")+1:lha.rindex(")")]
    parts = split_top(inside)
    typ, name, reg = parts[0], parts[1], parts[2]
    return f'{typ} {name} asm("{reg.lower()}")'

def transform(text, libbasetype):
    macro_re = re.compile(r'\bAROS_(LH|UFH|UFP)(\d+)\s*\(')
    out, pos = [], 0
    for m in macro_re.finditer(text):
        out.append(text[pos:m.start()])
        kind, n = m.group(1), int(m.group(2))
        op = m.end() - 1                      # index of the '('
        cl = match_paren(text, op)
        inner = text[op+1:cl]
        toks = split_top(inner)
        ret, name = toks[0], toks[1]
        args = [parse_arg(t) for t in toks[2:2+n]]
        if kind == "LH":
            # trailer: LIBBASETYPEPTR, <basevar>, <lvo>, <basename>
            basevar = toks[2+n+1]
            args.append(f'{libbasetype} {basevar} asm("a6")')
            # Parenthesise the LVO function name so it does NOT expand the
            # same-named inline macro (from <proto/poseidon.h>) that the library
            # uses for INTERNAL calls. `<ret> (name)(args)` still defines `name`.
            name = f"({name})"
        sig = f'{ret} {name}(' + ", ".join(args) + ")"
        out.append(sig)
        pos = cl + 1
    out.append(text[pos:])
    text = "".join(out)

    # drop INIT/EXIT marker lines
    text = re.sub(r'^[ \t]*AROS_(LIBFUNC|USERFUNC)_(INIT|EXIT)[ \t]*\n', '', text, flags=re.M)
    # drop ADD2*LIB registration lines
    text = re.sub(r'^[ \t]*ADD2(INIT|OPEN|CLOSE|EXPUNGE)LIB\([^\n]*\n', '', text, flags=re.M)
    # GM_UNIQUENAME(x) -> x
    text = re.sub(r'GM_UNIQUENAME\(\s*(\w+)\s*\)', r'\1', text)
    # LIBBASETYPEPTR -> struct PsdBase *  (remaining, non-macro sites)
    text = text.replace("LIBBASETYPEPTR", libbasetype)
    return text

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--libbasetype", default="struct PsdBase *")
    ap.add_argument("--inplace", action="store_true")
    a = ap.parse_args()
    # latin-1 round-trips any byte (some AROS sources are ISO-8859, not UTF-8).
    src = open(a.file, encoding="latin-1").read()
    res = transform(src, a.libbasetype)
    if a.inplace:
        open(a.file, "w", encoding="latin-1").write(res)
        sys.stderr.write(f"rewrote {a.file}\n")
    else:
        sys.stdout.write(res)

if __name__ == "__main__":
    main()
