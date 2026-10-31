"""Local cumulative wall-clock accounting. Ledger and raw receipts stay in out/."""
import argparse
import datetime as dt
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'plans/AITSNAP-V1-GOAL-TRAIN-001/GOAL-MANIFEST.json'
LEDGER = ROOT / 'out/AITSNAP-V1-GOAL-TRAIN-001/ledger.json'

def now():
    return dt.datetime.now(dt.timezone.utc)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['start', 'checkpoint', 'goal', 'stop', 'status'])
    p.add_argument('--goal')
    p.add_argument('--status', choices=['ACTIVE', 'PASS', 'PARTIAL', 'BLOCKED', 'OWNER_HOLD'])
    p.add_argument('--note', default='')
    p.add_argument('--pr')
    p.add_argument('--merge')
    p.add_argument('--startup-seconds', type=float, default=0)
    a = p.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    t = now()
    if LEDGER.exists():
        data = json.loads(LEDGER.read_text(encoding='utf-8'))
    else:
        if a.action != 'start':
            p.error('Initialize with start; an existing ledger is never reset.')
        data = dict(programId=manifest['programId'], budgetSeconds=120 * 3600,
                    startUtc=t.isoformat(), startSha=subprocess.check_output(
                        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                    elapsedSeconds=a.startup_seconds, active=False, lastTickUtc=t.isoformat(),
                    startupAllowanceSeconds=a.startup_seconds, currentGoal='R00', history=[],
                    goals={g['id']: dict(status='NOT_STARTED', elapsedSeconds=0,
                                        budgetSeconds=g['budgetHours'] * 3600)
                           for g in manifest['goals']})
    if data['active']:
        seconds = max(0, (t - dt.datetime.fromisoformat(data['lastTickUtc'])).total_seconds())
        data['elapsedSeconds'] += seconds
        data['goals'][data['currentGoal']]['elapsedSeconds'] += seconds
    data['lastTickUtc'] = t.isoformat()
    if a.action == 'start':
        data['active'] = True
    if a.action == 'goal':
        if a.goal not in data['goals']:
            p.error('Unknown goal')
        if a.status == 'ACTIVE':
            data['currentGoal'] = a.goal
        record = data['goals'][a.goal]
        if a.status:
            record['status'] = a.status
        if a.pr:
            record['pr'] = a.pr
        if a.merge:
            record['mergeSha'] = a.merge
        if a.note:
            record['note'] = a.note
    if a.action == 'stop' or data['elapsedSeconds'] >= data['budgetSeconds']:
        data['active'] = False
    data['budgetLeftSeconds'] = max(0, data['budgetSeconds'] - data['elapsedSeconds'])
    if a.action != 'status':
        data['history'].append(dict(utc=t.isoformat(), action=a.action, goal=a.goal,
                                    status=a.status, note=a.note, elapsedSeconds=data['elapsedSeconds']))
    LEDGER.parent.mkdir(parents=True, exist_ok=True)
    temp = LEDGER.with_suffix('.tmp')
    temp.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    temp.replace(LEDGER)
    print(json.dumps({k: data[k] for k in ['programId', 'active', 'currentGoal',
                                          'elapsedSeconds', 'budgetLeftSeconds', 'goals']}, indent=2))
    if data['budgetLeftSeconds'] == 0:
        raise SystemExit('BUDGET_EXHAUSTED: stop execution')
    if data['goals'][data['currentGoal']]['elapsedSeconds'] >= data['goals'][data['currentGoal']]['budgetSeconds']:
        raise SystemExit('GOAL_BUDGET_EXHAUSTED: retain partial work and select another eligible goal')

if __name__ == '__main__':
    main()
