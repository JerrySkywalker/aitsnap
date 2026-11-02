"""Opt-in real Word text validation; invoke the ordinary helper on new synthetic jobs only.

Run only with explicit authorization for these new Office tests. This is not a
fallback for a rejected invocation; stop and retain evidence if tools deny it.
"""
import argparse
import hashlib
import json
import posixpath
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fixtures', type=Path, required=True)
parser.add_argument('--helper', type=Path, required=True)
args = parser.parse_args()
root = args.fixtures.resolve()
helper = args.helper.resolve()
read = lambda p: json.loads(p.read_text(encoding='utf-8-sig'))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
ns = {'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
matrix = read(root / 'matrix.json')
sources = {p.name: sha(p) for p in (root / 'original.png', root / 'composite.png')}
results = []

def inspect(target, case, drawings):
    with zipfile.ZipFile(target) as document:
        xml = ET.fromstring(document.read('word/document.xml'))
        text = '\n'.join(x.text or '' for x in xml.findall('.//w:t', ns))
        assert len(xml.findall('.//w:drawing', ns)) == drawings
        tables = len(xml.findall('.//w:tbl', ns))
        assert (tables > 0) == (case['layout'] == 'table')
        for marker, enabled in [('批次标题', case['preset'] in ('title', 'custom')),
                                ('图片说明', case['preset'] in ('caption', 'custom')),
                                ('上下文', case['preset'] == 'custom')]:
            assert (marker in text) == enabled, (case['name'], marker, text)
        if case['preset'] == 'images':
            assert not text.strip(), (case['name'], text)
        if case['preset'] in ('caption', 'custom'):
            assert 'ORIGINAL:renamed|3|' in text and 'COMPOSITE:拼图甲||3' in text
        if case['preset'] in ('title', 'custom'):
            assert '{固定} 试验A' in text and '批次标题 21' in text
            if drawings == 4:
                assert '批次标题 22' in text
        media = {hashlib.sha256(document.read(name)).hexdigest()
                 for name in document.namelist() if name.startswith('word/media/')}
        assert set(sources.values()).issubset(media), 'embedded images differ from synthetic sources'
        relationships = ET.fromstring(document.read('word/_rels/document.xml.rels'))
        image_targets = {rel.get('Id'): posixpath.normpath(posixpath.join('word', rel.get('Target', ''))).lstrip('/')
                         for rel in relationships if rel.get('Type', '').endswith('/image')}
        ordered = []
        for drawing in xml.findall('.//w:drawing', ns):
            blip = drawing.find('.//{http://schemas.openxmlformats.org/drawingml/2006/main}blip')
            key = blip.get('{http://schemas.openxmlformats.org/officeDocument/2006/relationships}embed')
            ordered.append(hashlib.sha256(document.read(image_targets[key])).hexdigest())
        assert ordered == [sources['original.png'], sources['composite.png']] * (drawings // 2), 'image order changed'
        settings = ET.fromstring(document.read('word/settings.xml'))
        committed = sum(node.get('{'+ns['w']+'}val', '').startswith('COMMITTED:')
                        for node in settings.findall('.//w:docVar', ns))
        assert committed == drawings // 2, 'commit markers do not match batches'
        return {'drawings': drawings, 'tables': tables, 'committedMarkers': committed, 'imageOrder': 'PASS'}

try:
    assert len(matrix['cases']) == 8
    for case in matrix['cases']:
        target = Path(case['target']).resolve()
        assert target.parent == root and not target.exists(), 'never overwrite an existing document'
        for index, job_file in enumerate(case['jobs']):
            job_path = Path(job_file).resolve()
            assert job_path.parent == root
            job = read(job_path)
            assert Path(job['target']['documentPath']).resolve() == target
            result = root / f"{case['name']}-{index}.result.json"
            assert not result.exists()
            run = subprocess.run([str(helper), '--job', str(job_path), '--result', str(result)],
                                 capture_output=True, timeout=90)
            assert run.returncode == 0, (case['name'], index, run.returncode, run.stderr)
            status = read(result)
            assert status['state'] == 'COMMITTED', status
            observed = inspect(target, case, 2 * (index + 1))
        before = sha(target)
        replay_result = root / f"{case['name']}-replay.result.json"
        replay = subprocess.run([str(helper), '--job', case['jobs'][-1], '--result', str(replay_result)],
                                capture_output=True, timeout=90)
        assert replay.returncode == 0 and read(replay_result)['state'] == 'COMMITTED'
        assert sha(target) == before, 'committed replay changed the document'
        assert all(sha(root / name) == value for name, value in sources.items())
        results.append({'case': case['name'], 'status': 'PASS', **observed,
                        'committedReplayUnchanged': True, 'sourcesUnchanged': True})
        print(json.dumps(results[-1], ensure_ascii=True), flush=True)
    receipt = {'status': 'PASS', 'helperSha256': sha(helper), 'cases': results,
               'scope': 'real current-host Word synthetic create/append/reopen/idempotent replay plus DOCX XML/media; not Word2010 or human layout acceptance'}
    (root / 'native-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
except Exception as error:
    (root / 'native-failure.json').write_text(json.dumps({'status': 'NOT_COMPLETED',
        'completed': results, 'error': str(error)}, indent=2)+'\n', encoding='utf-8')
    raise
