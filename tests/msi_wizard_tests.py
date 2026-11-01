"""Read-only checks against the authored MSI's actual wizard tables (Windows only)."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import sys
import hashlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_msi import function, ok, open_db, open_view, execute, close
fetch = function('MsiViewFetch', [W.UINT, C.POINTER(W.UINT)])
get_string = function('MsiRecordGetStringW', [W.UINT, W.UINT, W.LPWSTR, C.POINTER(W.DWORD)])
read_stream = function('MsiRecordReadStream', [W.UINT, W.UINT, C.c_void_p, C.POINTER(W.DWORD)])
db = W.UINT()
ok(open_db(str(Path(sys.argv[1]).resolve()), None, C.byref(db)), 'open read-only MSI')
def rows(table, columns):
    view = W.UINT(); result = []
    ok(open_view(db, 'SELECT '+','.join('`'+x+'`' for x in columns)+' FROM `'+table+'`', C.byref(view)), 'query '+table)
    try:
        ok(execute(view, 0), 'execute')
        while True:
            record = W.UINT(); status = fetch(view, C.byref(record))
            if status == 259: break
            ok(status, 'fetch')
            try:
                row = []
                for index in range(1,len(columns)+1):
                    buffer = C.create_unicode_buffer(8192); count = W.DWORD(len(buffer))
                    ok(get_string(record,index,buffer,C.byref(count)), 'read'); row.append(buffer.value)
                result.append(row)
            finally: close(record)
    finally: close(view)
    return result
def icon_hash(table):
    view = W.UINT(); record = W.UINT()
    ok(open_view(db, f"SELECT `Data` FROM `{table}` WHERE `Name` = 'AITSnapIcon'", C.byref(view)), 'query icon')
    try:
        ok(execute(view, 0), 'execute icon'); ok(fetch(view, C.byref(record)), 'fetch icon')
        digest = hashlib.sha256()
        while True:
            buffer = C.create_string_buffer(32768); size = W.DWORD(len(buffer))
            ok(read_stream(record, 1, buffer, C.byref(size)), 'read icon stream')
            if not size.value: return digest.hexdigest()
            digest.update(buffer.raw[:size.value])
    finally:
        if record.value: close(record)
        close(view)
try:
    controls = rows('Control', ['Dialog_','Control','Type','Text','Control_Next','X','Y','Width','Height'])
    dialogs = rows('Dialog', ['Dialog','Control_First','Control_Default','Control_Cancel'])
    for dialog, first, default, cancel in dialogs:
        own = {r[1]:r for r in controls if r[0] == dialog}
        assert first in own and (not default or default in own) and (not cancel or cancel in own), dialog
        if dialog == 'ErrorDlg': continue
        links = {k:r[4] for k,r in own.items() if r[4]}
        seen = set(); start = next(iter(links)); cursor = start
        while cursor not in seen:
            seen.add(cursor); cursor = links[cursor]
        assert cursor == start and seen == set(links), 'Broken tab cycle: '+dialog
        assert {k for k,r in own.items() if r[2] in ['PushButton','CheckBox','PathEdit','DirectoryList','DirectoryCombo']} <= seen, dialog
    properties = dict(rows('Property',['Property','Value']))
    # Text rectangles must not cover interactive controls on the first paint.
    for text in [r for r in controls if r[2]=='Text']:
        tx,ty,tw,th = map(int,text[5:9])
        for target in [r for r in controls if r[0]==text[0] and r[2] in ['PathEdit','PushButton','CheckBox','DirectoryList','DirectoryCombo','ProgressBar']]:
            x,y,w,h = map(int,target[5:9])
            assert not (tx<x+w and tx+tw>x and ty<y+h and ty+th>y), 'Overlapping text/input: '+text[0]+'/'+text[1]+' and '+target[1]
    progress = {r[1]:r for r in controls if r[0]=='ProgressDlg'}
    assert int(progress['Body'][6])+int(progress['Body'][8]) <= int(progress['Action'][6]), 'Progress body obscures action text'
    assert properties['ProductVersion'] in ['0.6.1','0.6.2','0.7.0']
    assert any(r[0]=='WelcomeDlg' and '[ProductVersion]' in r[3] and '多屏截图' in r[3] for r in controls)
    assert any(r[0]=='ExitDlg' and '立即启动 AITSnap' in r[3] for r in controls)
    sequence = {r[0]:r[1] for r in rows('InstallUISequence',['Action','Sequence'])}
    assert sequence['ExitDlg']=='-1' and sequence['FatalDlg']=='-3' and sequence['UserExitDlg']=='-2'
    events = rows('ControlEvent',['Dialog_','Control_','Event','Argument','Condition','Ordering'])
    assert any(r[:3]==['FolderDlg','Next','SetTargetPath'] for r in events)
    assert any(r[:3]==['ExitDlg','Finish','MsiLaunchApp'] and 'LAUNCH_AITSNAP' in r[4] and 'REMOVE' in r[4] for r in events)
    assert not any(r[0] in ['MsiLaunchApp','LaunchAITSnap'] for r in rows('InstallExecuteSequence',['Action']))
    if properties['ProductVersion'] == '0.7.0':
        main_key = next(r[0] for r in rows('File',['File','FileName']) if r[1].split('|')[-1] == 'aitsnap.exe')
        assert any(r[:4] == ['ExitDlg','Finish','MsiLaunchApp',f'[#{main_key}] --show'] for r in events), 'Documented file token and explicit foreground intent'
        for dialog,button in [('FolderDlg','Next'),('MaintenanceDlg','Repair'),('MaintenanceDlg','Remove')]:
            assert any(r[:4] == [dialog,button,'NewDialog','ProgressDlg'] for r in events), 'Preparation must be visible before UI sequence resumes'
        assert properties['ARPPRODUCTICON'] == 'AITSnapIcon'
        assert any(r[0] == 'AITSnapIcon' for r in rows('Icon',['Name']))
        assert any(r[0] == 'AITSnapIcon' for r in rows('Binary',['Name']))
        assert all(r[1] == 'AITSnapIcon' for r in rows('Shortcut',['Shortcut','Icon_']))
        expected = hashlib.sha256((Path(__file__).resolve().parents[1] / 'assets/brand/aitsnap.ico').read_bytes()).hexdigest()
        assert icon_hash('Icon') == icon_hash('Binary') == expected, 'MSI ARP/shortcut/wizard use identical production ICO bytes'
        conditions = [r[0] for r in rows('LaunchCondition',['Condition'])]
        if 'table fixture' in properties['ProductName'].lower():
            assert '0' in conditions and properties['UpgradeCode'] != '{476109B6-03EF-4F9D-A55D-E796AE147379}', 'Fixture cannot install or share release upgrade identity'
        else:
            assert '0' not in conditions, 'Release package must not contain the fixture-only launch block'
    print('MSI_WIZARD_TABLES_PASS dialogs='+str(len(dialogs))+' controls='+str(len(controls))+' tabcycles/localization/path/finish/launch/quiet-boundary')
finally: close(db)
