import re

_IDENT = re.compile(r'[A-Za-z0-9_]')

def strip_c(text):
    out = []
    i = 0
    n = len(text)
    line_has_code = False
    pending_ws = ''
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if c == '\n':
            out.append('\n')
            line_has_code = False
            i += 1
            continue
        if c == '/' and nxt == '/':
            j = i
            while j < n:
                if text[j] == '\n' and text[j - 1] != '\\':
                    break
                j += 1
            i = j
            continue
        if c == '/' and nxt == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            newlines = text.count('\n', i, j)
            if line_has_code and newlines == 0:
                out.append(' ')
            elif line_has_code:
                out.append('\n' * newlines)
                line_has_code = False
            else:
                out.append('\n' * newlines)
            i = j
            continue
        if c == '"':
            if i > 0 and text[i - 1] == 'R':
                m = re.match(r'"([^()\\ ]{0,16})\(', text[i:])
                if m:
                    end = ')' + m.group(1) + '"'
                    j = text.find(end, i + m.end())
                    j = n if j < 0 else j + len(end)
                    out.append(text[i:j])
                    line_has_code = True
                    i = j
                    continue
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(text[i:j])
            line_has_code = True
            i = j
            continue
        if c == "'":
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(text[i:j])
            line_has_code = True
            i = j
            continue
        if c.isdigit() and not (i > 0 and _IDENT.match(text[i - 1])):
            j = i
            while j < n and (text[j].isalnum() or text[j] in "._" or (text[j] == "'" and j + 1 < n and text[j + 1].isalnum())):
                if text[j] in 'eEpP' and j + 1 < n and text[j + 1] in '+-':
                    j += 1
                j += 1
            out.append(text[i:j])
            line_has_code = True
            i = j
            continue
        if not c.isspace():
            line_has_code = True
        out.append(c)
        i += 1
    return _tidy(''.join(out))

def _tidy(text):
    lines = [ln.rstrip() for ln in text.split('\n')]
    result = []
    blank = 0
    for ln in lines:
        if ln == '':
            blank += 1
            if blank > 1 or not result:
                continue
        else:
            blank = 0
        result.append(ln)
    while result and result[-1] == '':
        result.pop()
    return '\n'.join(result) + '\n'

def strip_hash(text, shebang=True):
    result = []
    for number, line in enumerate(text.split('\n')):
        if shebang and number == 0 and line.startswith('#!'):
            result.append(line)
            continue
        quote = ''
        cut = -1
        for k, ch in enumerate(line):
            if quote:
                if ch == '\\':
                    continue
                if ch == quote:
                    quote = ''
                continue
            if ch in '"\'':
                quote = ch
            elif ch == '#' and (k == 0 or line[k - 1] in ' \t'):
                cut = k
                break
        result.append(line if cut < 0 else line[:cut])
    return _tidy('\n'.join(result))
