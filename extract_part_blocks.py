import re, pathlib, collections
p = pathlib.Path(r'e:\wangjunhua\Project\AvrProgrammer\avrdude\src\avrdude.conf.in')
lines = p.read_text(encoding='utf-8', errors='ignore').splitlines()

parts = []
current = None
for line in lines:
    stripped = line.strip()
    if re.match(r'^part\s*$', stripped):
        current = []
        continue
    if current is not None:
        if stripped == ';':
            body = '\n'.join(current)
            desc_match = re.search(r'(?m)^\s*desc\s*=\s*"([^"]*)"', body)
            id_match = re.search(r'(?m)^\s*id\s*=\s*"([^"]*)"', body)
            if desc_match or id_match:
                parts.append((desc_match.group(1) if desc_match else '', id_match.group(1) if id_match else ''))
            current = None
        else:
            current.append(line)

print('PART_COUNT', len(parts))
for desc, id_ in parts[:120]:
    print(f'{id_}\t{desc}')

prefix_counter = collections.Counter()
for _, id_ in parts:
    if id_:
        m = re.match(r'([A-Za-z]+)', id_)
        if m:
            prefix_counter[m.group(1)] += 1
        else:
            prefix_counter['<other>'] += 1
print('\nPREFIXES')
for k, v in sorted(prefix_counter.items(), key=lambda x:(-x[1], x[0])):
    print(k, v)
