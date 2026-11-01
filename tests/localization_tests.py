"""Check shipped UI catalog coverage and Qt substitution tokens."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
catalog = json.loads((root / 'translations/en-US.json').read_text(encoding='utf-8'))
checked = set()
for path in (root / 'src').rglob('*'):
    if path.suffix not in {'.cpp', '.h'}:
        continue
    for raw in re.findall(r'uiText\("((?:\\.|[^"\\])*)"\)', path.read_text(encoding='utf-8')):
        source = json.loads('"' + raw + '"')
        if not re.search('[\u3400-\u9fff]', source):
            continue
        assert catalog.get(source), f'Missing English text: {path}: {source}'
        assert sorted(re.findall(r'%\d+', source)) == sorted(re.findall(r'%\d+', catalog[source])), source
        checked.add(source)
print(f'LOCALIZATION_PASS {len(checked)} source strings, catalog coverage and substitution tokens')
