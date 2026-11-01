"""Build a local offline preview from existing Release products and Qt SDK. No installs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--qt', type=Path, required=True)
    p.add_argument('--crt', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    a = p.parse_args()
    target = a.destination.resolve()
    if target.exists():
        p.error('Destination must be new; no existing files are replaced')
    target.mkdir(parents=True)
    build = ROOT / 'out/build/windows-msvc-qt-sdk/Release'
    for name in ['aitsnap.exe', 'aitsnap-cli.exe', 'aitsnap-word-bridge.exe']:
        shutil.copy2(build / name, target / name)
    subprocess.run([str(a.qt / 'bin/windeployqt.exe'), '--release', '--no-translations',
                    '--no-compiler-runtime', '--no-system-d3d-compiler', '--no-system-dxc-compiler', '--no-opengl-sw',
                    '--skip-plugin-types', 'generic,iconengines,networkinformation,tls', '--exclude-plugins', 'qsvg',
                    str(target / 'aitsnap.exe')], check=True)
    for path in a.crt.glob('*.dll'):
        shutil.copy2(path, target / path.name)
    (target / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\n', encoding='utf-8')
    translations = target / 'translations'; translations.mkdir()
    shutil.copy2(a.qt / 'translations/qtbase_zh_CN.qm', translations / 'qtbase_zh_CN.qm')
    for name in ['install-preview.ps1', 'uninstall-preview.ps1', 'Launch-Preview.ps1']:
        shutil.copy2(ROOT / 'tools' / name, target / name)
    for name in ['LICENSE', 'README.md']:
        shutil.copy2(ROOT / name, target / name)
    shutil.copytree(ROOT / 'docs/preview', target / 'docs')
    shutil.copytree(ROOT / 'docs/architecture', target / 'docs/architecture')
    shutil.copytree(ROOT / 'docs/contracts', target / 'docs/contracts')
    shutil.copytree(ROOT / 'tests/fixtures/synthetic-ait', target / 'synthetic-demo')
    shutil.copy2(ROOT / 'examples/metadata_host.py', target / 'synthetic-demo/metadata_host.py')
    shutil.copytree(ROOT / 'third_party/licenses/qt', target / 'licenses')
    shutil.copy2(a.qt.parents[1] / 'Licenses/LICENSE', target / 'licenses/Qt-SDK-LICENSE.txt')
    shutil.copytree(a.qt / 'sbom', target / 'licenses/qt-sdk-sbom')
    provenance = []
    for path in sorted(target.rglob('*.dll')):
        relative = path.relative_to(target)
        candidates = [a.qt / 'bin' / path.name, a.qt / 'plugins' / relative, a.crt / path.name]
        source = next((x for x in candidates if x.is_file() and sha(x) == sha(path)), None)
        if source is None:
            raise RuntimeError(f'Unresolved DLL provenance: {relative}')
        provenance.append(dict(path=relative.as_posix(), sha256=sha(path), source=str(source)))
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    sources = {x: sha(ROOT / x) for x in tracked if x and (ROOT / x).is_file()}
    (target / 'build-provenance.json').write_text(json.dumps(dict(
        sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        sourceStatus=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
        sourceFileSha256=sources, dlls=provenance,
        qualification='Windows10 1809+ x64 / Win11 candidate; native Win10, Win7, Word2010 and Owner acceptance HOLD'), indent=2) + '\n', encoding='utf-8')
    manifest = dict(product='AITSnap modern local preview', files=[dict(path=x.relative_to(target).as_posix(), sha256=sha(x)) for x in sorted(target.rglob('*')) if x.is_file()])
    (target / 'package-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'PACKAGE_CREATED {target} files={len(manifest["files"])}')
if __name__ == '__main__':
    main()
