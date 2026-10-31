"""Read-only checks against the authored MSI's actual wizard tables (Windows only)."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_msi import function, ok, open_db, open_view, execute, close
fetch = function('MsiViewFetch', [W.UINT, C.POINTER(W.UINT)])
get_string = function('MsiRecordGetStringW', [W.UINT, W.UINT, W.LPWSTR, C.POINTER(W.DWORD)])
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
try:
    controls = rows('Control', ['Dialog_','Control','Type','Text','Control_Next'])
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
    assert properties['ProductVersion'] == '0.6.1'
    assert any(r[0]=='WelcomeDlg' and '[ProductVersion]' in r[3] and '多屏截图' in r[3] for r in controls)
    assert any(r[0]=='ExitDlg' and '立即启动 AITSnap' in r[3] for r in controls)
    sequence = {r[0]:r[1] for r in rows('InstallUISequence',['Action','Sequence'])}
    assert sequence['ExitDlg']=='-1' and sequence['FatalDlg']=='-3' and sequence['UserExitDlg']=='-2'
    events = rows('ControlEvent',['Dialog_','Control_','Event','Argument','Condition','Ordering'])
    assert any(r[:3]==['FolderDlg','Next','SetTargetPath'] for r in events)
    assert any(r[:3]==['ExitDlg','Finish','MsiLaunchApp'] and 'LAUNCH_AITSNAP' in r[4] and 'REMOVE' in r[4] for r in events)
    assert not any(r[0] in ['MsiLaunchApp','LaunchAITSnap'] for r in rows('InstallExecuteSequence',['Action']))
    print('MSI_WIZARD_TABLES_PASS dialogs='+str(len(dialogs))+' controls='+str(len(controls))+' tabcycles/localization/path/finish/launch/quiet-boundary')
finally: close(db)
