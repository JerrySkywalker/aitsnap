"""Production packages accept runtime, documentation, and licenses only."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from package_preview import REQUIRED_PAYLOAD, validate_production_paths

payload = sorted(REQUIRED_PAYLOAD) + [
    'docs/OPERATOR-MANUAL.zh-CN.md',
    'docs/contracts/capture-batch.schema.json',
    'licenses/qt-sdk-sbom/qtbase-6.12.0.source.spdx',
    'imageformats/qjpeg.dll',
]
validate_production_paths(payload)
for invalid in ('synthetic-demo/capture-batch.json', 'settings-v1.json',
                'docs/fixtures/demo.md', 'docs/../readme.md', 'tests/synthetic.png',
                'Qt6Core.dll'):
    try:
        validate_production_paths([*payload, invalid])
    except ValueError:
        continue
    raise AssertionError(f'Unexpected production file accepted: {invalid}')
try:
    validate_production_paths(payload[1:])
except ValueError:
    pass
else:
    raise AssertionError('A missing required production file was accepted')
print('PRODUCTION_PAYLOAD_PASS fixtures, settings, noncanonical and duplicate paths rejected')
