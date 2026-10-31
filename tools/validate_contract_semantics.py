"""Research-contract semantics beyond schema shape; fictional fixtures only."""
from copy import deepcopy
import json
from pathlib import Path, PureWindowsPath, PurePosixPath
from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'tests/fixtures/synthetic-ait'
def load(name):
    return json.loads((FIXTURES / (name + '.json')).read_text(encoding='utf-8'))

def unique(values, label):
    assert len(values) == len(set(values)), label

def catalog_check(catalog):
    commands = [c['commandId'] for c in catalog['commands']]
    unique(commands, 'duplicate command')
    sequences = [(s['sequenceId'], s['revision']) for s in catalog['sequences']]
    unique(sequences, 'duplicate sequence revision')
    for sequence in catalog['sequences']:
        unique([s['stepId'] for s in sequence['steps']], 'duplicate step')
        for step in sequence['steps']:
            if step['kind'] == 'command':
                assert step.get('commandId') in commands, 'unresolved command'
            if step['kind'] == 'subsequence':
                assert (step.get('subsequenceId'), step.get('subsequenceRevision')) in sequences, 'unresolved versioned subsequence'

def expect_invalid(fn, label):
    try:
        fn()
    except (AssertionError, KeyError):
        return
    raise AssertionError('Negative case unexpectedly valid: ' + label)

catalog = load('sequence-catalog')
catalog_check(catalog)
bad = deepcopy(catalog); bad['commands'].append(bad['commands'][0])
expect_invalid(lambda: catalog_check(bad), 'duplicate command')
bad = deepcopy(catalog); bad['sequences'][0]['steps'][0]['commandId'] = 'missing'
expect_invalid(lambda: catalog_check(bad), 'unknown command')
nested = deepcopy(catalog)
nested['sequences'].append(dict(sequenceId='child', revision='v2', name='Synthetic child', steps=[]))
nested['sequences'][0]['steps'].append(dict(stepId='nested', order=3, kind='subsequence', subsequenceId='child', subsequenceRevision='v2'))
catalog_check(nested)
nested['sequences'][0]['steps'][-1]['subsequenceRevision'] = 'v1'
expect_invalid(lambda: catalog_check(nested), 'wrong child revision')
schema = json.loads((ROOT / 'docs/contracts/sequence-catalog.schema.json').read_text(encoding='utf-8'))
validator = Draft202012Validator(schema)
bad = deepcopy(catalog); bad['sequences'][0]['steps'][0].pop('commandId')
assert list(validator.iter_errors(bad)), 'kind requires command reference'
batch, request, scene, job = (load(n) for n in ['capture-batch', 'capture-request', 'scene-template', 'report-job'])
regions = [c['regionId'] for c in batch['captures']]
unique(regions, 'duplicate captured region')
assert set(request['regionIds']) == set(regions)
unique([layer['layerId'] for layer in scene['layers']], 'duplicate layer')
assert all(layer['sourceRegionId'] in regions for layer in scene['layers'])
artifact_ids = [a['artifactId'] for a in batch['artifacts']]
unique(artifact_ids, 'duplicate artifact')
assert all(item['artifactId'] in artifact_ids for item in job['items'])
assert job['batchId'] == batch['batchId'] and request['requestId'] == batch['requestId']
for artifact in batch['artifacts']:
    path = artifact.get('relativePath', '')
    assert path and not PureWindowsPath(path).is_absolute() and not PurePosixPath(path).is_absolute()
    assert '..' not in PureWindowsPath(path).parts and '..' not in PurePosixPath(path).parts
print('CONTRACT_SEMANTICS_PASS references, uniqueness, versioned nesting, request/batch/scene/report linkage, relative paths, negative cases')
