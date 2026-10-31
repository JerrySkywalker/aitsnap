"""Validate the program DAG, local document links and six public schema fixtures."""
import json
from pathlib import Path
import re
from jsonschema import Draft202012Validator, FormatChecker

ROOT = Path(__file__).resolve().parents[1]
plan = ROOT / 'plans/AITSNAP-V1-GOAL-TRAIN-001'
m = json.loads((plan / 'GOAL-MANIFEST.json').read_text(encoding='utf-8'))
goals = {g['id']: g for g in m['goals']}
assert len(goals) == len(m['goals']) == m['goalCount'] == 16
assert sum(g['budgetHours'] for g in goals.values()) == m['totalBudgetHours'] == 120
visited, active = set(), set()
def visit(key):
    assert key not in active, f'Cycle: {key}'
    if key in visited:
        return
    active.add(key)
    g = goals[key]
    text = (ROOT / g['path']).read_text(encoding='utf-8')
    for heading in ['Scope', 'Work checklist', 'Deliverables', 'Verification and acceptance', 'Explicit holds']:
        assert f'## {heading}' in text, (key, heading)
    for dep in g['dependencies']:
        visit(dep)
    active.remove(key)
    visited.add(key)
for key in goals:
    visit(key)
for schema_path in (ROOT / 'docs/contracts').glob('*.schema.json'):
    schema = json.loads(schema_path.read_text(encoding='utf-8'))
    Draft202012Validator.check_schema(schema)
    fixture = ROOT / 'tests/fixtures/synthetic-ait' / schema_path.name.replace('.schema', '')
    Draft202012Validator(schema, format_checker=FormatChecker()).validate(
        json.loads(fixture.read_text(encoding='utf-8')))
    print(f'SCHEMA_FIXTURE_PASS {fixture.name}')
for file in [ROOT / 'docs/ARCHITECTURE-INDEX.md', *plan.glob('*.md')]:
    for link in re.findall(r'\]\(([^)#]+)(?:#[^)]*)?\)', file.read_text(encoding='utf-8')):
        if '://' not in link:
            assert (file.parent / link).exists(), (file, link)
print('PLAN_PASS: 16 goals, 120h, acyclic dependencies, goal sections, local links')
