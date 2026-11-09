"""Build a local offline preview from existing Release products and Qt SDK. No installs."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
APP_FILES = {'aitsnap.exe', 'aitsnap-cli.exe', 'aitsnap-word-bridge.exe'}
ROOT_FILES = APP_FILES | {'qt.conf', 'install-preview.ps1', 'uninstall-preview.ps1',
                         'launch-preview.ps1', 'license', 'readme.md', 'readme.zh-cn.md',
                         'build-provenance.json'}
CRT_FILES = {'concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
             'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll', 'vccorlib140.dll',
             'vcruntime140.dll', 'vcruntime140_1.dll', 'vcruntime140_threads.dll'}
QT_FILES = {'qt6core.dll', 'qt6gui.dll', 'qt6widgets.dll', 'platforms/qwindows.dll',
            'imageformats/qgif.dll', 'imageformats/qico.dll', 'imageformats/qjpeg.dll',
            'styles/qmodernwindowsstyle.dll'}
REQUIRED_PAYLOAD = APP_FILES | {'qt.conf', 'qt6core.dll', 'qt6gui.dll', 'qt6widgets.dll',
                                'platforms/qwindows.dll', 'translations/qtbase_zh_cn.qm', 'build-provenance.json'}
LICENSE_FILES = {'licenses/gpl-3.0-only.txt', 'licenses/lgpl-3.0-only.txt',
                 'licenses/qt-sdk-license.txt', 'licenses/readme.md'}


def validate_production_paths(paths):
    seen = set()
    for name in paths:
        path = PurePosixPath(name)
        if path.is_absolute() or name != path.as_posix() or any(part in {'.', '..'} or ':' in part or '\\' in part
                                                                for part in path.parts):
            raise ValueError(f'Invalid package path: {name}')
        lower = name.casefold()
        if lower in seen:
            raise ValueError(f'Duplicate package path: {name}')
        allowed = (lower in ROOT_FILES | CRT_FILES | QT_FILES | LICENSE_FILES
                   or lower == 'translations/qtbase_zh_cn.qm'
                   or re.fullmatch(r'docs/(?:architecture/)?[^/]+\.md', lower)
                   or re.fullmatch(r'docs/contracts/(?:readme\.md|[^/]+\.schema\.json)', lower)
                   or re.fullmatch(r'licenses/qt-sdk-sbom/qt[\w-]+-\d+\.\d+\.\d+\.(?:cdx\.json|(?:source\.)?spdx(?:\.json)?)', lower))
        if not allowed:
            raise ValueError(f'File outside production payload: {name}')
        seen.add(lower)
    missing = REQUIRED_PAYLOAD - seen
    if missing:
        raise ValueError('Required production payload missing: ' + ', '.join(sorted(missing)))


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
    for name in sorted(APP_FILES):
        shutil.copy2(build / name, target / name)
    subprocess.run([str(a.qt / 'bin/windeployqt.exe'), '--release', '--no-translations',
                    '--no-compiler-runtime', '--no-system-d3d-compiler', '--no-system-dxc-compiler', '--no-opengl-sw',
                    '--skip-plugin-types', 'generic,iconengines,networkinformation,tls', '--exclude-plugins', 'qsvg',
                    str(target / 'aitsnap.exe')], check=True)
    for path in a.crt.glob('*.dll'):
        if path.name.casefold() in CRT_FILES:
            shutil.copy2(path, target / path.name)
    (target / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\n', encoding='utf-8')
    translations = target / 'translations'; translations.mkdir()
    shutil.copy2(a.qt / 'translations/qtbase_zh_CN.qm', translations / 'qtbase_zh_CN.qm')
    for name in ['install-preview.ps1', 'uninstall-preview.ps1', 'Launch-Preview.ps1']:
        shutil.copy2(ROOT / 'tools' / name, target / name)
    for name in ['LICENSE', 'README.md', 'README.zh-CN.md']:
        shutil.copy2(ROOT / name, target / name)
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    doc_destinations = {'docs/preview': 'docs', 'docs/architecture': 'docs/architecture',
                        'docs/contracts': 'docs/contracts', 'third_party/licenses/qt': 'licenses'}
    for source in filter(None, tracked):
        relative = PurePosixPath(source)
        destination = doc_destinations.get(relative.parent.as_posix())
        if destination is None:
            continue
        if relative.suffix == '.md' or (
                destination == 'docs/contracts' and relative.name.endswith('.schema.json')
                or destination == 'licenses' and relative.suffix == '.txt'):
            output = target / destination / relative.name
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / source, output)
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
    sources = {x: sha(ROOT / x) for x in tracked if x and (ROOT / x).is_file()}
    (target / 'build-provenance.json').write_text(json.dumps(dict(
        sourceCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        sourceStatus=subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
        sourceFileSha256=sources, dlls=provenance,
        qualification='Windows10 1809+ x64 / Win11 candidate; native Win10, Win7, Word2010 and Owner acceptance HOLD'), indent=2) + '\n', encoding='utf-8')
    files = [dict(path=x.relative_to(target).as_posix(), sha256=sha(x))
             for x in sorted(target.rglob('*')) if x.is_file()]
    validate_production_paths(file['path'] for file in files)
    manifest = dict(product='AITSnap modern local preview', files=files)
    (target / 'package-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'PACKAGE_CREATED {target} files={len(manifest["files"])}')
if __name__ == '__main__':
    main()
