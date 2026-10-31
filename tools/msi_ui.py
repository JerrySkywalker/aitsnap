"""Native Windows Installer wizard; no wrapper process or downloaded UI runtime."""

def author_ui(db, main_file):
    tables = {
        'Dialog': '`Dialog` CHAR(72) NOT NULL, `HCentering` SHORT NOT NULL, `VCentering` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Attributes` LONG, `Title` CHAR(128) LOCALIZABLE, `Control_First` CHAR(72) NOT NULL, `Control_Default` CHAR(72), `Control_Cancel` CHAR(72) PRIMARY KEY `Dialog`',
        'Control': '`Dialog_` CHAR(72) NOT NULL, `Control` CHAR(72) NOT NULL, `Type` CHAR(20) NOT NULL, `X` SHORT NOT NULL, `Y` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Attributes` LONG, `Property` CHAR(72), `Text` CHAR(0) LOCALIZABLE, `Control_Next` CHAR(72), `Help` CHAR(50) LOCALIZABLE PRIMARY KEY `Dialog_`, `Control`',
        'ControlEvent': '`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(72) NOT NULL, `Event` CHAR(50) NOT NULL, `Argument` CHAR(255) NOT NULL, `Condition` CHAR(255), `Ordering` SHORT PRIMARY KEY `Dialog_`, `Control_`, `Event`, `Argument`, `Condition`',
        'ControlCondition': '`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(72) NOT NULL, `Action` CHAR(50) NOT NULL, `Condition` CHAR(255) NOT NULL PRIMARY KEY `Dialog_`, `Control_`, `Action`, `Condition`',
        'TextStyle': '`TextStyle` CHAR(72) NOT NULL, `FaceName` CHAR(32) NOT NULL, `Size` SHORT NOT NULL, `Color` LONG, `StyleBits` SHORT PRIMARY KEY `TextStyle`',
        'CheckBox': '`Property` CHAR(72) NOT NULL, `Value` CHAR(64) LOCALIZABLE PRIMARY KEY `Property`',
        'EventMapping': '`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(72) NOT NULL, `Event` CHAR(50) NOT NULL, `Attribute` CHAR(50) NOT NULL PRIMARY KEY `Dialog_`, `Control_`, `Event`',
    }
    for name, definition in tables.items(): db.table(name, definition)
    for name, size, bits in [('Normal', 10, 0), ('Heading', 17, 1)]:
        db.insert('TextStyle', 'TextStyle FaceName Size Color StyleBits', name, 'Microsoft YaHei UI', size, 0, bits)
    for key, value in [('DefaultUIFont', 'Normal'), ('ErrorDialog', 'ErrorDlg'), ('LAUNCH_AITSNAP', '1')]:
        db.insert('Property', 'Property Value', key, value)
    db.insert('CheckBox', 'Property Value', 'LAUNCH_AITSNAP', '1')

    def dialog(name, first='Next', default='Next', cancel='Cancel', attributes=3):
        db.insert('Dialog', 'Dialog HCentering VCentering Width Height Attributes Title Control_First Control_Default Control_Cancel',
                  name, 50, 50, 440, 300, attributes, 'AITSnap [ProductVersion] 安装向导', first, default, cancel)
    def control(d, name, kind, x, y, w, h, text=None, prop=None, next=None, attrs=3):
        db.insert('Control', 'Dialog_ Control Type X Y Width Height Attributes Property Text Control_Next Help',
                  d, name, kind, x, y, w, h, attrs, prop, text, next, None)
    def event(d, button, kind, argument, condition='1', order=1):
        db.insert('ControlEvent', 'Dialog_ Control_ Event Argument Condition Ordering', d, button, kind, argument, condition, order)
    def heading(d, title, body, body_height=110):
        control(d, 'Heading', 'Text', 24, 22, 392, 32, '{\\Heading}'+title)
        control(d, 'Body', 'Text', 24, 67, 392, body_height, body)
        control(d, 'Line', 'Line', 20, 252, 400, 0)
    def button(d, name, text, x, next=None):
        control(d, name, 'PushButton', x, 267, 84, 22, text, next=next)
    def cancel(d, next='Next'):
        button(d, 'Cancel', '取消', 332, next)
        event(d, 'Cancel', 'EndDialog', 'Exit')

    dialog('WelcomeDlg')
    heading('WelcomeDlg', '欢迎安装 AITSnap',
            '软件版本：[ProductVersion]\n\n多屏截图、托盘快捷操作、自由拼图与 Word 报告。\n\n支持 Windows 10 1809+ x64 / Windows 11 x64。Word 报告功能需要已安装 Microsoft Word。\n\n点击“下一步”继续。')
    button('WelcomeDlg', 'Next', '下一步 >', 240, 'Cancel'); cancel('WelcomeDlg')
    event('WelcomeDlg', 'Next', 'NewDialog', 'FolderDlg', 'NOT Installed')
    event('WelcomeDlg', 'Next', 'NewDialog', 'MaintenanceDlg', 'Installed')

    dialog('FolderDlg', 'Path', 'Next')
    heading('FolderDlg', '选择安装路径', '选择程序文件的安装目录。截图、配置和报告保存在独立位置。\n\n可直接输入完整路径，或点击“浏览”。', body_height=70)
    control('FolderDlg', 'Path', 'PathEdit', 24, 150, 302, 22, prop='INSTALLDIR', next='Browse')
    control('FolderDlg', 'Browse', 'PushButton', 336, 150, 80, 22, '浏览…', next='Back')
    event('FolderDlg', 'Browse', 'SpawnDialog', 'BrowseDlg')
    button('FolderDlg', 'Back', '< 上一步', 148, 'Next'); event('FolderDlg', 'Back', 'NewDialog', 'WelcomeDlg')
    button('FolderDlg', 'Next', '安装', 240, 'Cancel'); cancel('FolderDlg', 'Path')
    event('FolderDlg', 'Next', 'SetTargetPath', 'INSTALLDIR', order=1)
    event('FolderDlg', 'Next', 'EndDialog', 'Return', order=2)

    dialog('BrowseDlg', 'Path', 'OK', 'Cancel')
    control('BrowseDlg', 'Title', 'Text', 24, 16, 392, 24, '{\\Heading}浏览安装目录')
    control('BrowseDlg', 'Path', 'PathEdit', 24, 54, 392, 22, prop='INSTALLDIR', next='Location')
    control('BrowseDlg', 'Location', 'DirectoryCombo', 24, 86, 302, 22, prop='INSTALLDIR', next='Folders')
    control('BrowseDlg', 'Folders', 'DirectoryList', 24, 118, 392, 132, prop='INSTALLDIR', next='Up')
    control('BrowseDlg', 'Up', 'PushButton', 336, 86, 80, 22, '上级目录', next='OK')
    event('BrowseDlg', 'Up', 'DirectoryListUp', '0')
    button('BrowseDlg', 'OK', '确定', 240, 'Cancel'); button('BrowseDlg', 'Cancel', '返回', 332, 'Path')
    event('BrowseDlg', 'OK', 'SetTargetPath', 'INSTALLDIR', order=1)
    event('BrowseDlg', 'OK', 'EndDialog', 'Return', order=2)
    event('BrowseDlg', 'Cancel', 'EndDialog', 'Return')

    dialog('MaintenanceDlg', 'Repair', 'Repair')
    heading('MaintenanceDlg', 'AITSnap 已安装', '版本：[ProductVersion]\n\n可修复现有安装，或卸载程序。修复保留安装路径；截图、配置和报告不随卸载删除。')
    button('MaintenanceDlg', 'Repair', '修复', 148, 'Remove'); button('MaintenanceDlg', 'Remove', '卸载', 240, 'Cancel')
    control('MaintenanceDlg', 'Cancel', 'PushButton', 332, 267, 84, 22, '取消', next='Repair')
    event('MaintenanceDlg', 'Cancel', 'EndDialog', 'Exit')
    event('MaintenanceDlg', 'Repair', '[REINSTALL]', 'ALL', order=1)
    event('MaintenanceDlg', 'Repair', '[REINSTALLMODE]', 'omus', order=2)
    event('MaintenanceDlg', 'Repair', 'EndDialog', 'Return', order=3)
    event('MaintenanceDlg', 'Remove', '[REMOVE]', 'ALL', order=1)
    event('MaintenanceDlg', 'Remove', 'EndDialog', 'Return', order=2)

    dialog('ProgressDlg', 'Cancel', 'Cancel', 'Cancel', attributes=1)
    heading('ProgressDlg', '正在应用更改', 'Windows Installer 正在安装、修复或卸载 AITSnap。\n请等待完成提示。', body_height=80)
    control('ProgressDlg', 'Action', 'Text', 24, 170, 392, 28, '正在准备…')
    control('ProgressDlg', 'Progress', 'ProgressBar', 24, 210, 392, 16)
    control('ProgressDlg', 'Cancel', 'PushButton', 332, 267, 84, 22, '取消', next='Cancel')
    event('ProgressDlg', 'Cancel', 'EndDialog', 'Exit')
    for c, e, attr in [('Progress','SetProgress','Progress'), ('Action','ActionText','Text')]:
        db.insert('EventMapping', 'Dialog_ Control_ Event Attribute', 'ProgressDlg', c, e, attr)

    dialog('ExitDlg', 'Finish', 'Finish', 'Finish')
    heading('ExitDlg', '安装操作已完成', 'AITSnap [ProductVersion] 已完成安装或修复。\n\n安装目录：[INSTALLDIR]\n\n可以从开始菜单打开 AITSnap，或勾选下方选项后点击“完成”。')
    control('ExitDlg', 'Removed', 'Text', 24, 67, 392, 110, 'AITSnap 已卸载。\n\n安装目录之外的截图、配置和报告已保留。', attrs=1)
    control('ExitDlg', 'Launch', 'CheckBox', 24, 210, 392, 24, '立即启动 AITSnap', 'LAUNCH_AITSNAP', 'Finish')
    button('ExitDlg', 'Finish', '完成', 332, 'Launch')
    for c in ['Body','Launch']:
        for action in ['Hide','Disable']: db.insert('ControlCondition','Dialog_ Control_ Action Condition','ExitDlg',c,action,'REMOVE="ALL"')
    db.insert('ControlCondition','Dialog_ Control_ Action Condition','ExitDlg','Removed','Show','REMOVE="ALL"')
    db.insert('ControlCondition','Dialog_ Control_ Action Condition','ExitDlg','Removed','Hide','NOT (REMOVE="ALL")')
    event('ExitDlg', 'Finish', 'MsiLaunchApp', f'"[#{main_file}]"', 'LAUNCH_AITSNAP AND NOT (REMOVE="ALL")', 1)
    event('ExitDlg', 'Finish', 'EndDialog', 'Return', order=2)

    for d, title, body, seq in [
        ('FatalDlg','安装未完成','安装遇到错误，未成功完成。请保留 Windows Installer 错误信息和日志。',-3),
        ('UserExitDlg','安装已取消','安装操作已取消。可再次打开安装包重新开始。',-2)]:
        dialog(d,'Finish','Finish','Finish'); heading(d,title,body)
        button(d,'Finish','关闭',332,'Finish'); event(d,'Finish','EndDialog','Return')
        db.insert('InstallUISequence','Action Condition Sequence',d,None,seq)

    dialog('ErrorDlg','ErrorText',None,None,attributes=65539)
    control('ErrorDlg','ErrorText','Text',24,24,392,205,'[ErrorText]')
    for name, label in [('ErrorAbort','终止'),('ErrorCancel','取消'),('ErrorIgnore','忽略'),('ErrorNo','否'),('ErrorOk','确定'),('ErrorRetry','重试'),('ErrorYes','是')]:
        control('ErrorDlg',name,'PushButton',178,263,84,24,label)
        event('ErrorDlg',name,'EndDialog',name)
    for row in [('WelcomeDlg','NOT (REMOVE="ALL")',1100),('ProgressDlg',None,1250),('ExitDlg',None,-1)]:
        db.insert('InstallUISequence','Action Condition Sequence',*row)
