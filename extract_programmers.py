import re, pathlib, collections
p = pathlib.Path(r'e:\wangjunhua\Project\AvrProgrammer\avrdude\src\avrdude.conf.in')
lines = p.read_text(encoding='utf-8', errors='ignore').splitlines()

entries = []
current = None
for line in lines:
    s = line.strip()
    if re.match(r'^(programmer|serialadapter)\b', s):
        if current is not None:
            body = '\n'.join(current)
            desc_match = re.search(r'(?im)^\s*desc\s*=\s*"([^"]*)"', body)
            ids = re.findall(r'(?im)^\s*id\s*=\s*"([^"]*)"', body)
            if desc_match or ids:
                entries.append((desc_match.group(1) if desc_match else '', ids))
        current = [line]
    elif current is not None:
        if s == ';':
            body = '\n'.join(current)
            desc_match = re.search(r'(?im)^\s*desc\s*=\s*"([^"]*)"', body)
            ids = re.findall(r'(?im)^\s*id\s*=\s*"([^"]*)"', body)
            if desc_match or ids:
                entries.append((desc_match.group(1) if desc_match else '', ids))
            current = None
        else:
            current.append(line)

if current is not None:
    body = '\n'.join(current)
    desc_match = re.search(r'(?im)^\s*desc\s*=\s*"([^"]*)"', body)
    ids = re.findall(r'(?im)^\s*id\s*=\s*"([^"]*)"', body)
    if desc_match or ids:
        entries.append((desc_match.group(1) if desc_match else '', ids))

print('ENTRY_COUNT', len(entries))
for desc, ids in entries:
    print(desc + '\t' + ', '.join(ids))

c = collections.Counter()
for _, ids in entries:
    for id_ in ids:
        m = re.match(r'([A-Za-z]+)', id_)
        if m:
            c[m.group(1)] += 1
        else:
            c['<other>'] += 1
print('PREFIXES')
for k, v in sorted(c.items(), key=lambda x: (-x[1], x[0])):
    print(k, v)
