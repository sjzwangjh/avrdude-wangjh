import re, pathlib, collections
p = pathlib.Path(r'e:\wangjunhua\Project\AvrProgrammer\avrdude\src\avrdude.conf.in')
text = p.read_text(encoding='utf-8', errors='ignore')

# Matches both programmer and part blocks, capturing their names and bodies.
pattern = re.compile(r'(?ms)^(programmer|part)\s+(\S+)\s*\{(.*?)^\}', re.M)
entries = []
for m in pattern.finditer(text):
    block_type = m.group(1)
    name = m.group(2)
    body = m.group(3)
    desc_match = re.search(r'(?m)^\s*desc\s+"([^"]*)"', body)
    id_matches = re.findall(r'(?m)^\s*id\s+"([^"]*)"', body)
    if desc_match or id_matches:
        entries.append((block_type, name, desc_match.group(1) if desc_match else '', id_matches))

print('TOTAL_ENTRIES', len(entries))
print('BLOCK_TYPES', collections.Counter(e[0] for e in entries))

# Prefix summary for all IDs (taking first alphabetic run)
prefix_counter = collections.Counter()
for _, _, _, ids in entries:
    for id_ in ids:
        m = re.match(r'([A-Za-z]+)', id_)
        if m:
            prefix_counter[m.group(1)] += 1
        else:
            prefix_counter['<other>'] += 1
print('PREFIXES')
for k, v in sorted(prefix_counter.items(), key=lambda x: (-x[1], x[0])):
    print(k, v)

# Print a compact list of the first 200 entries to inspect content
print('\nSAMPLE_ENTRIES')
for block_type, name, desc, ids in entries[:200]:
    print(f'{block_type} {name} | desc={desc} | ids={ids}')
