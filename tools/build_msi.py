"""Author a per-user x64 MSI using existing Windows Installer/CAB facilities."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
from pathlib import Path
import subprocess
import uuid
from msi_ui import author_ui

UPGRADE = uuid.UUID('476109b6-03ef-4f9d-a55d-e796ae147379')
msi = C.WinDLL('msi')
def function(name, types, result=W.UINT):
    fn = getattr(msi, name); fn.argtypes = types; fn.restype = result; return fn
open_db = function('MsiOpenDatabaseW', [W.LPCWSTR,C.c_void_p,C.POINTER(W.UINT)])
open_view = function('MsiDatabaseOpenViewW', [W.UINT,W.LPCWSTR,C.POINTER(W.UINT)])
execute = function('MsiViewExecute', [W.UINT,W.UINT])
create_record = function('MsiCreateRecord', [W.UINT])
set_string = function('MsiRecordSetStringW', [W.UINT,W.UINT,W.LPCWSTR])
set_integer = function('MsiRecordSetInteger', [W.UINT,W.UINT,C.c_int])
set_stream = function('MsiRecordSetStreamW', [W.UINT,W.UINT,W.LPCWSTR])
close = function('MsiCloseHandle', [W.UINT])
commit = function('MsiDatabaseCommit', [W.UINT])
get_summary = function('MsiGetSummaryInformationW',[W.UINT,W.LPCWSTR,W.UINT,C.POINTER(W.UINT)])
summary_property = function('MsiSummaryInfoSetPropertyW',[W.UINT,W.UINT,W.UINT,C.c_int,C.c_void_p,W.LPCWSTR])
summary_persist = function('MsiSummaryInfoPersist',[W.UINT])
get_file_version = function('MsiGetFileVersionW',[W.LPCWSTR,W.LPWSTR,C.POINTER(W.DWORD),W.LPWSTR,C.POINTER(W.DWORD)])
import_table = function('MsiDatabaseImportW',[W.UINT,W.LPCWSTR,W.LPCWSTR])
def ok(code, operation):
    if code: raise RuntimeError(f'{operation}: Windows Installer error {code}')
def guid(value): return '{'+str(value).upper()+'}'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def file_version(path):
    if path.suffix.lower() not in {'.exe','.dll'}: return None
    version=C.create_unicode_buffer(256); size=W.DWORD(len(version))
    status=get_file_version(str(path),version,C.byref(size),None,None)
    if status in {1006,1812,1813}: return None
    ok(status,'file version '+str(path)); return version.value or None
class Database:
    def __init__(self, path):
        self.handle = W.UINT(); ok(open_db(str(path), C.c_void_p(3), C.byref(self.handle)), 'create MSI')
    def sql(self, statement, values=(), streams=()):
        view = W.UINT(); ok(open_view(self.handle, statement, C.byref(view)), statement)
        record = create_record(len(values)) if values else 0
        try:
            for i, value in enumerate(values, 1):
                if i in streams: ok(set_stream(record,i,str(value)),'stream')
                elif isinstance(value,int): ok(set_integer(record,i,value),'integer')
                elif value is not None: ok(set_string(record,i,str(value)),'string')
            ok(execute(view,record),statement)
        finally:
            if record: close(record)
            close(view)
    def table(self, name, definition): self.sql(f'CREATE TABLE `{name}` ({definition})')
    def insert(self, table, columns, *values, streams=()):
        self.sql(f'INSERT INTO `{table}` ({",".join("`"+x+"`" for x in columns.split())}) VALUES ({",".join("?" for _ in values)})',values,streams)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--version',default='0.6.1')
    a=p.parse_args(); package=a.package.resolve(); output=a.destination.resolve()
    if output.exists(): p.error('Choose a new destination directory')
    manifest=json.loads((package/'package-manifest.json').read_text(encoding='utf-8'))
    for file in manifest['files']:
        if not (package/file['path']).resolve().is_relative_to(package): raise RuntimeError('Package path leaves its root')
        if sha(package/file['path']) != file['sha256']: raise RuntimeError('Package hash mismatch: '+file['path'])
    sources={Path(x['path']):package/x['path'] for x in manifest['files']}
    sources[Path('package-manifest.json')]=package/'package-manifest.json'
    product=guid(uuid.uuid5(UPGRADE,a.version))
    output.mkdir(parents=True); cabinet=output/'payload.cab'
    marker=output/'msi-install.json'; marker.write_text(json.dumps(dict(installer='Windows Installer',productCode=product,version=a.version,sourcePackageManifestSha256=sha(package/'package-manifest.json')),indent=2)+'\n',encoding='utf-8')
    sources[Path('msi-install.json')]=marker; files=sorted(sources)
    ddf=['.OPTION EXPLICIT','.Set CabinetNameTemplate=payload.cab',f'.Set DiskDirectoryTemplate="{output}"','.Set CompressionType=MSZIP','.Set Cabinet=on','.Set Compress=on','.Set MaxDiskSize=0']
    ddf += [f'"{sources[file]}" F{i:05}' for i,file in enumerate(files,1)]
    (output/'payload.ddf').write_text('\n'.join(ddf)+'\n',encoding='utf-8')
    subprocess.run(['makecab.exe','/F',str(output/'payload.ddf')],check=True,cwd=output)
    package_code=guid(uuid.uuid4()); path=output/f'AITSnap-{a.version}-windows-x64.msi'; db=Database(path)
    try:
        # Set the string pool before inserting localized Chinese UI strings.
        (output/'_ForceCodepage.idt').write_bytes(b'\r\n\r\n936\t_ForceCodepage\r\n')
        ok(import_table(db.handle,str(output),'_ForceCodepage.idt'),'set Chinese codepage')
        db.table('Property','`Property` CHAR(72) NOT NULL, `Value` CHAR(0) LOCALIZABLE PRIMARY KEY `Property`')
        db.table('Directory','`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`')
        db.table('Component','`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`')
        db.table('Feature','`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`')
        db.table('FeatureComponents','`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`')
        db.table('File','`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`')
        db.table('Media','`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`')
        db.table('Registry','`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`')
        db.table('Shortcut','`Shortcut` CHAR(72) NOT NULL, `Directory_` CHAR(72) NOT NULL, `Name` CHAR(128) NOT NULL LOCALIZABLE, `Component_` CHAR(72) NOT NULL, `Target` CHAR(255) NOT NULL, `Arguments` CHAR(255), `Description` CHAR(255) LOCALIZABLE, `Hotkey` SHORT, `Icon_` CHAR(72), `IconIndex` SHORT, `ShowCmd` SHORT, `WkDir` CHAR(72) PRIMARY KEY `Shortcut`')
        db.table('RemoveFile','`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) LOCALIZABLE, `DirProperty` CHAR(72) NOT NULL, `InstallMode` SHORT NOT NULL PRIMARY KEY `FileKey`')
        db.table('AppSearch','`Property` CHAR(72) NOT NULL, `Signature_` CHAR(72) NOT NULL PRIMARY KEY `Property`, `Signature_`')
        db.table('Signature','`Signature` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `MinVersion` CHAR(20), `MaxVersion` CHAR(20), `MinSize` LONG, `MaxSize` LONG, `MinDate` LONG, `MaxDate` LONG, `Languages` CHAR(255) PRIMARY KEY `Signature`')
        db.table('RegLocator','`Signature_` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255), `Type` SHORT PRIMARY KEY `Signature_`')
        db.table('LaunchCondition','`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Condition`')
        db.table('Upgrade','`UpgradeCode` CHAR(38) NOT NULL, `VersionMin` CHAR(20), `VersionMax` CHAR(20), `Language` CHAR(255), `Attributes` SHORT NOT NULL, `Remove` CHAR(255), `ActionProperty` CHAR(72) NOT NULL PRIMARY KEY `UpgradeCode`, `VersionMin`, `VersionMax`, `Language`, `Attributes`')
        for table in ['InstallExecuteSequence','InstallUISequence']:
            db.table(table,'`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`')
        props=dict(ProductName='AITSnap Preview',ProductVersion=a.version,ProductCode=product,UpgradeCode=guid(UPGRADE),ProductLanguage='2052',Manufacturer='AITSnap contributors',INSTALLLEVEL='1',ARPNOMODIFY='1',ARPURLINFOABOUT='https://github.com/JerrySkywalker/aitsnap',SecureCustomProperties='OLDPRODUCTS;NEWERPRODUCTS;INSTALLDIR',MsiLogging='v')
        for key,value in props.items(): db.insert('Property','Property Value',key,value)
        for row in [('TARGETDIR',None,'SourceDir'),('LocalAppDataFolder','TARGETDIR','.'),('ProgramsDir','LocalAppDataFolder','Programs'),('INSTALLDIR','ProgramsDir','AITSnap'),('ProgramMenuFolder','TARGETDIR','.'),('APPMENU','ProgramMenuFolder','AITSnap')]: db.insert('Directory','Directory Directory_Parent DefaultDir',*row)
        directories={Path('.'):'INSTALLDIR'}
        for file in files:
            for parent in reversed(file.parents):
                if parent not in directories:
                    key=f'D{len(directories):05}'; db.insert('Directory','Directory Directory_Parent DefaultDir',key,directories[parent.parent],f'{key}|{parent.name}'); directories[parent]=key
        db.insert('Feature','Feature Feature_Parent Title Description Display Level Directory_ Attributes','Main',None,'AITSnap','Capture, scenes and Word reporting',1,1,'INSTALLDIR',0)
        main_component=None; main_file=None
        for i,file in enumerate(files,1):
            relative=file; identifier=f'F{i:05}'; component=f'C{i:05}'
            db.insert('Component','Component ComponentId Directory_ Attributes Condition KeyPath',component,guid(uuid.uuid5(UPGRADE,relative.as_posix())),directories[relative.parent],256,None,identifier)
            db.insert('FeatureComponents','Feature_ Component_','Main',component)
            db.insert('File','File Component_ FileName FileSize Version Language Attributes Sequence',identifier,component,f'{identifier}|{file.name}',sources[file].stat().st_size,file_version(sources[file]),None,512,i)
            if relative.as_posix()=='aitsnap.exe': main_component,main_file=component,identifier
        assert main_component
        db.insert('Media','DiskId LastSequence DiskPrompt Cabinet VolumeLabel Source',1,len(files),None,'#payload.cab',None,None)
        db.sql('INSERT INTO `_Streams` (`Name`,`Data`) VALUES (?,?)',('payload.cab',cabinet),streams=(2,))
        for name,value in [('AITSNAP_OS_MAJOR','CurrentMajorVersionNumber'),('AITSNAP_OS_BUILD','CurrentBuildNumber'),('AITSNAP_INSTALL_TYPE','InstallationType')]:
            db.insert('AppSearch','Property Signature_',name,name)
            db.insert('RegLocator','Signature_ Root Key Name Type',name,2,r'SOFTWARE\Microsoft\Windows NT\CurrentVersion',value,18)
        db.insert('LaunchCondition','Condition Description','Installed OR (VersionNT64 AND AITSNAP_OS_MAJOR = "#10" AND AITSNAP_OS_BUILD >= 17763 AND AITSNAP_INSTALL_TYPE = "Client")','AITSnap requires Windows 10 1809 or later, x64 (Windows 11 supported).')
        db.insert('LaunchCondition','Condition Description','NOT NEWERPRODUCTS','A newer AITSnap version is already installed.')
        db.insert('Upgrade','UpgradeCode VersionMin VersionMax Language Attributes Remove ActionProperty',guid(UPGRADE),None,a.version,None,1,None,'OLDPRODUCTS')
        db.insert('Upgrade','UpgradeCode VersionMin VersionMax Language Attributes Remove ActionProperty',guid(UPGRADE),a.version,None,None,2,None,'NEWERPRODUCTS')
        db.insert('Registry','Registry Root Key Name Value Component_','InstallLocation',1,r'Software\AITSnap\Installer','InstallDir','[INSTALLDIR]',main_component)
        db.insert('Shortcut','Shortcut Directory_ Name Component_ Target Arguments Description Hotkey Icon_ IconIndex ShowCmd WkDir','AITSnapStart','APPMENU','AIT|AITSnap',main_component,f'[#{main_file}]',None,'Multi-screen capture, scenes and Word report',None,None,None,1,'INSTALLDIR')
        for i,directory in enumerate([*directories.values(),'APPMENU']): db.insert('RemoveFile','FileKey Component_ FileName DirProperty InstallMode',f'R{i:05}',main_component,None,directory,2)
        actions=[('FindRelatedProducts',None,25),('AppSearch','NOT Installed',50),('LaunchConditions',None,100),('CostInitialize',None,800),('FileCost',None,900),('CostFinalize',None,1000),('MigrateFeatureStates',None,1200),('InstallValidate',None,1400),('InstallInitialize',None,1500),('RemoveExistingProducts','OLDPRODUCTS',1501),('ProcessComponents',None,1600),('UnpublishFeatures',None,1800),('RemoveRegistryValues',None,2600),('RemoveShortcuts',None,3200),('RemoveFiles',None,3500),('RemoveFolders',None,3600),('CreateFolders',None,3700),('InstallFiles',None,4000),('CreateShortcuts',None,4500),('WriteRegistryValues',None,5000),('RegisterUser',None,6000),('RegisterProduct',None,6100),('PublishFeatures',None,6300),('PublishProduct',None,6400),('InstallFinalize',None,6600)]
        for row in actions: db.insert('InstallExecuteSequence','Action Condition Sequence',*row)
        for row in [('FindRelatedProducts',None,25),('AppSearch','NOT Installed',50),('LaunchConditions',None,100),('CostInitialize',None,800),('FileCost',None,900),('CostFinalize',None,1000),('ExecuteAction',None,1300)]: db.insert('InstallUISequence','Action Condition Sequence',*row)
        author_ui(db,main_file)
        info=W.UINT(); ok(get_summary(db.handle,None,20,C.byref(info)),'summary')
        try:
            for key,value in {2:'Installation Database',3:'AITSnap Windows x64 installer',4:'AITSnap contributors',7:'x64;2052',9:package_code,14:500,15:10,18:'AITSnap build_msi.py'}.items(): ok(summary_property(info,key,3 if isinstance(value,int) else 30,value if isinstance(value,int) else 0,None,None if isinstance(value,int) else value),'summary property')
            ok(summary_persist(info),'save summary')
        finally: close(info)
        ok(commit(db.handle),'commit MSI')
    finally: close(db.handle)
    receipt=dict(productCode=product,upgradeCode=guid(UPGRADE),packageCode=package_code,version=a.version,msi=str(path),sha256=sha(path),packageManifestSha256=sha(package/'package-manifest.json'),files=len(files),nativeWin10Acceptance='HOLD until actual target run')
    receipt['filesSha256']={file.as_posix():sha(sources[file]) for file in files}
    (output/'installer-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8'); print(json.dumps({k:v for k,v in receipt.items() if k!='filesSha256'}))
if __name__=='__main__': main()
