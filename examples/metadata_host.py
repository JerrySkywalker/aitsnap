"""Fictional read-only AIT metadata adapter; never sends instrument commands."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--pipe-name')
    fixtures = Path(__file__).resolve().parents[1] / 'tests/fixtures/synthetic-ait'
    parser.add_argument('--catalog', type=Path, default=fixtures / 'sequence-catalog.json')
    parser.add_argument('--event', type=Path, default=fixtures / 'execution-event.json')
    parser.add_argument('--capture', action='store_true', help='Request capture on first receipt; configured rules may also capture')
    args = parser.parse_args()

    def send(action, *options):
        command = [str(args.cli.resolve()), action, *map(str, options)]
        if args.pipe_name:
            command += ['--pipe-name', args.pipe_name]
        completed = subprocess.run(command, capture_output=True, text=True, encoding='utf-8',
                                   timeout=25, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        if completed.returncode:
            raise SystemExit(completed.stdout or completed.stderr or 'Local host request failed')
        reply = json.loads(completed.stdout)
        print(json.dumps(reply, ensure_ascii=False))
        return reply

    catalog_key = 'catalog-' + hashlib.sha256(args.catalog.read_bytes()).hexdigest()
    send('import-catalog', '--file', args.catalog.resolve(), '--key', catalog_key)
    event = json.loads(args.event.read_text(encoding='utf-8'))
    identity = json.dumps([event['sourceSystem'], event['eventId']], separators=(',', ':')).encode()
    event_key = 'event-' + hashlib.sha256(identity).hexdigest()
    capture = ['--capture'] if args.capture else []
    send('submit-event', '--file', args.event.resolve(), '--key', event_key, *capture)
    send('query-status')


if __name__ == '__main__':
    main()
