"""Four tab-specific synthetic UI concepts, using the original design primitives."""
from pathlib import Path
from generate_concepts import Diagram, OUT, INK, MUTED, BORDER, BLUE, MINT, names, BRAND

AMBER='#825b15'

def field(d,x,y,w,label,value):
    d.text(x,y,label,16,MUTED);d.rect(x,y+12,w,38,'white',BORDER,r=6);d.text(x+12,y+38,value,18)

def tabs(d,x,y,items,selected=0):
    xx=x
    for i,(label,w) in enumerate(items):
        d.rect(xx,y,w,38,'#e4edff' if i==selected else '#f8fafc',BORDER,r=7)
        d.text(xx+12,y+25,label,17,BLUE if i==selected else MUTED,600 if i==selected else 400);xx+=w+8

def letterbox(d,x,y,w,h,kind,source_w,source_h):
    d.rect(x,y,w,h,'#e4eaf1',r=6)
    scale=min(w/source_w,h/source_h);ww=source_w*scale;hh=source_h*scale
    d.mini(x+(w-ww)/2,y+(h-hh)/2,ww,hh,kind)

def collage(d,x,y,w,h,style=0,labels=False):
    d.rect(x,y,w,h,'#e6edf4',r=5);gap=8
    if style==1:
        frames=[(x+gap,y+gap,(w-3*gap)/2,h-2*gap,0,1200,200),(x+(w+gap)/2,y+gap,(w-3*gap)/2,h-2*gap,1,600,480)]
    else:
        frames=[(x+gap,y+gap,w-2*gap,h*.25,0,1200,200),
            (x+gap,y+h*.25+2*gap,w*.46-1.5*gap,h*.75-3*gap,1,600,480),
            (x+w*.46+.5*gap,y+h*.25+2*gap,w*.54-1.5*gap,h*.44-2*gap,2,1280,720),
            (x+w*.46+.5*gap,y+h*.69+gap,w*.54-1.5*gap,h*.31-2*gap,3,600,400)]
    for xx,yy,ww,hh,k,sw,sh in frames:
        letterbox(d,xx,yy,ww,hh,k,sw,sh)
        if labels:
            d.rect(xx+5,yy+5,160,34,'white',BORDER,r=5);d.badge(xx+8,yy+7,k+1);d.text(xx+46,yy+28,names[k],16)

def sources_page():
    d=Diagram();d.shell('源区域 · 看见每一个截图范围','当前配置：试验记录 · 3 个显示器 · 4 个区域全部启用',1)
    d.rect(212,184,1362,66,'white',BORDER);d.button(230,196,186,'试验记录  ▾');d.button(434,196,160,'屏幕框选 / 拖动')
    d.button(610,196,148,'确认屏幕布局');d.text(1110,224,'最近图像 10:48:59 · 非实时',17,MUTED)
    d.rect(212,270,292,620,'white',BORDER);d.text(232,308,'来源卡片',22,INK,700)
    for i,n in enumerate(names):
        y=327+i*133;d.rect(228,y,260,120,'#f8fbfd',BLUE if i==0 else BORDER)
        d.badge(240,y+12,i+1,BLUE if i==0 else MINT);d.text(281,y+34,n,20,INK,600)
        letterbox(d,240,y+48,92,56,i,*[(1200,200),(600,480),(1280,720),(600,400)][i])
        d.text(344,y+69,['显示器 A','显示器 A','显示器 B','显示器 C'][i],16,MUTED);d.text(344,y+96,'✓ 启用',16,MINT)
    d.rect(524,270,698,620,'white',BORDER);d.text(546,308,'#1 电压曲线',23,INK,700)
    tabs(d,546,326,[('区域预览',116),('显示器布局',136)])
    letterbox(d,546,385,654,238,0,1200,200)
    d.text(560,410,'源图等比显示 · 四周空白属于预览空间',16,MUTED)
    d.text(546,657,'位置与屏幕对应',19,INK,600)
    d.rect(546,674,280,175,'#eef3f8','#8da2b7',r=6);d.text(560,699,'A · 1920×1080',16,MUTED)
    d.rect(560,719,250,42,'#d8e8ff',BLUE,r=3);d.badge(567,725,1);d.text(607,747,'电压曲线',17,BLUE)
    d.rect(560,777,112,51,'#d9f1ed',MINT,r=3);d.badge(567,783,2,MINT)
    d.text(848,708,'源图实际：1200×200',18);d.text(848,742,'当前框选：1200×200',18)
    d.text(848,778,'采集于批次 0042',17,MUTED);d.text(848,816,'选择卡片会同步定位屏幕区域',16,MUTED)
    d.rect(1242,270,332,620,'white',BORDER);d.text(1264,308,'区域属性',22,INK,700)
    field(d,1264,345,288,'区域名称','电压曲线')
    d.text(1264,430,'显示器 A   ·   区域编号 #1',18,BLUE,600)
    field(d,1264,467,134,'X（物理像素）','60');field(d,1418,467,134,'Y（物理像素）','80')
    field(d,1264,540,134,'宽','1200');field(d,1418,540,134,'高','200')
    d.text(1264,638,'☑ 启用此区域',19,MINT,600);d.text(1264,678,'复制目标：电压曲线  ▾',18)
    d.line(1264,708,1552,708);d.button(1264,730,288,'在屏幕上调整此区域')
    d.text(1264,804,'区域名称用于预览和未来文件名',16,MUTED)
    d.rect(212,910,1362,66,'white',BORDER);d.button(230,922,140,'+ 添加区域');d.button(388,922,140,'删除所选')
    d.text(550,950,'变更保存后生效；已有批次保留原图',17,MUTED);d.button(1114,922,166,'截图并保存');d.button(1296,922,254,'应用区域设置',True)
    d.save('04-sources-tab')

def scenes_page():
    d=Diagram();d.shell('拼图场景 · 选一份布局，直接看到输出','复用现有画布、图层属性与预设 · 试验总览 / 修订 3',2)
    d.rect(212,185,280,702,'white',BORDER);d.text(232,223,'我的场景',22,INK,700)
    for i,(name,sub) in enumerate([('试验总览','☑ 用于下一次截图'),('曲线与日志','☑ 用于下一次截图')]):
        y=246+i*217;d.rect(226,y,252,203,'#f8fbfd',BLUE if i==0 else BORDER)
        d.text(241,y+30,name,21,INK,600);collage(d,240,y+47,224,112,i)
        d.text(241,y+184,sub,16,MINT)
    d.button(230,706,244,'+ 新建场景');d.button(230,764,116,'导入');d.button(360,764,116,'导出')
    d.text(232,852,'2 个场景 · 原图仍独立保存',16,MUTED)
    d.rect(512,185,746,702,'white',BORDER);d.text(534,223,'试验总览',23,INK,700)
    d.button(534,241,130,'网格预设 ▾');d.button(674,241,110,'应用预设');d.button(794,241,84,'撤销');d.button(888,241,84,'重做')
    d.text(992,270,'☑ 10px 吸附',16,MUTED);d.button(1120,241,116,'适合窗口')
    collage(d,534,300,702,395,labels=True)
    d.text(534,729,'可复用源区域',17,MUTED);d.rect(1052,706,184,32,'white',BORDER,r=6);d.text(1065,728,'+ 添加到画布',16,BLUE,600)
    for i in range(4):
        x=534+i*178;d.rect(x,749,164,110,'#f7fafc',BORDER,r=7);letterbox(d,x+8,757,148,54,i,*[(1200,200),(600,480),(1280,720),(600,400)][i])
        d.text(x+10,842,f'#{i+1} {names[i]}',16,INK,600)
    d.rect(1278,185,296,702,'white',BORDER);d.text(1298,223,'选中图层：#1',22,INK,700)
    field(d,1298,255,256,'源区域','电压曲线  ▾')
    field(d,1298,326,118,'X','20');field(d,1436,326,118,'Y','20')
    field(d,1298,397,118,'宽','1540');field(d,1436,397,118,'高','260')
    field(d,1298,468,118,'旋转','0°');field(d,1436,468,118,'层级 Z','1')
    d.text(1298,568,'☑ 可见    □ 锁定',18);d.text(1298,608,'☑ 等比适配',18,MINT,600)
    d.text(1298,642,'身份文字独立显示，不写入原图',15,MUTED)
    d.button(1298,675,118,'左对齐');d.button(1436,675,118,'等距排列')
    d.button(1298,729,118,'组合');d.button(1436,729,118,'取消组合')
    d.text(1298,811,'高级：裁剪 / 画布 / 缺源处理',15,MUTED)
    d.rect(212,909,1362,67,'white',BORDER);d.text(232,950,'1600×900 · 4 个源 · 编号标签仅用于编辑',18,MUTED)
    d.button(1000,922,178,'导出派生 PNG');d.button(1196,922,142,'取消修改');d.button(1356,922,196,'保存场景',True)
    d.save('05-scenes-tab')

def batches_page():
    d=Diagram();d.shell('批次与报告 · 浏览已保存的证据','原图和拼图先保存；Word 写入结果独立跟踪',3)
    d.rect(212,184,1362,65,'white',BORDER);d.button(230,196,180,'打开批次目录');d.text(437,223,'试验记录 / 今天',18,MUTED)
    tabs(d,1110,197,[('全部批次',126),('含待核对',140)])
    d.rect(212,269,286,619,'white',BORDER);d.text(232,307,'今天 · 3 个批次',21,INK,700)
    for i,(time,number,status) in enumerate([('10:48:59','0042','原图 4 · 拼图 2'),('10:46:18','0041','原图 4 · 拼图 2'),('10:42:05','0040','原图 4 · 拼图 1')]):
        y=327+i*176;d.rect(228,y,254,160,'#f8fbfd',BLUE if i==0 else BORDER)
        d.text(241,y+28,'批次 '+number,20,INK,600);d.text(367,y+28,time,16,MUTED)
        collage(d,241,y+43,228,72,0 if i<2 else 1)
        d.text(241,y+143,status,16,MUTED)
    d.rect(518,269,714,619,'white',BORDER);d.text(540,307,'批次 0042 · 试验总览',22,INK,700)
    tabs(d,540,327,[('拼图（2）',126),('原图（4）',126),('批次信息',116)])
    collage(d,540,384,670,377)
    d.text(540,791,'拼图 / 01_试验总览_r3.png',17);d.text(540,820,'1600×900 · 来自本批原图 · 不会自动替换为当前画面',16,MUTED)
    d.button(540,839,144,'打开所在目录');d.button(702,839,158,'用本批重排')
    d.rect(1252,269,322,619,'white',BORDER);d.text(1274,307,'Word 报告任务',21,INK,700)
    d.rect(1270,327,286,209,'#f7fbf9',BORDER);d.rect(1284,342,38,44,BLUE,r=6);d.text(1292,373,'W',24,'white',700)
    d.text(1336,363,'试验记录.docx',19,INK,600);d.text(1336,392,'批次 0042 · 已提交',16,MINT,600)
    d.mini(1284,410,120,69,0);d.mini(1418,410,120,69,2);d.text(1284,511,'本批已追加 6 张图',17,MINT)
    d.rect(1270,554,286,200,'#fff7e6','#ead3a3');d.text(1286,587,'补充记录.docx',19,INK,600)
    d.text(1286,620,'另一个报告 · 待核对',17,AMBER,600);d.text(1286,653,'先核对文档标记，再决定恢复',16,AMBER)
    d.button(1286,687,252,'查看任务 / 核对标记')
    d.text(1274,796,'报告卡展示引用图片和任务状态',15,MUTED);d.text(1274,825,'文档实际排版请在 Word 查看',15,MUTED)
    d.rect(212,909,1362,67,'white',BORDER);d.text(232,948,'本批 6 个文件已保存 · 报告任务不会改动原图',18,MUTED)
    d.rect(1032,922,214,42,'#f0f3f7',BORDER,r=8);d.text(1048,950,'本批已加入报告',18,MUTED,600);d.button(1264,922,288,'查看 Word 任务',True)
    d.save('06-batches-reports-tab')

def settings_page():
    d=Diagram();d.shell('设置 · 调整选项，同时看清结果','当前配置：试验记录 · 设置保存后应用到后续操作',4)
    d.rect(212,185,244,704,'white',BORDER);d.text(234,224,'设置分类',22,INK,700)
    for i,t in enumerate(['常用设置','保存与命名','快捷键与托盘','Word 报告','元数据关联','关于 AITSnap']):
        y=249+i*58
        if i==0:d.rect(226,y,216,44,'#e4edff',r=7)
        d.text(242,y+29,t,19,BLUE if i==0 else MUTED,600 if i==0 else 400)
    d.text(234,791,'所有内容仅保存在本机',16,MUTED)
    d.rect(476,185,678,704,'white',BORDER);d.text(500,224,'保存与常用操作',23,INK,700)
    field(d,500,260,480,'保存目录','D:\\试验证据');d.button(998,272,132,'浏览目录')
    field(d,500,334,194,'图片格式','PNG  ▾');field(d,716,334,414,'文件命名','{sequence}_{region}')
    field(d,500,411,630,'复制哪张图','区域 #1 · 电压曲线  ▾')
    d.line(500,480,1130,480)
    d.text(500,515,'快捷键',19,INK,600)
    for y,a,b in [(551,'截图并保存','Ctrl + Alt + S'),(596,'复制选中图片','Ctrl + Alt + C'),(641,'保存并复制','Ctrl + Alt + A')]:
        d.text(500,y+22,a,17,MUTED);d.rect(794,y-1,336,35,'#f6f8fb',BORDER,r=6);d.text(811,y+23,b,18)
    d.text(500,721,'□ 启动应用时直接进入托盘',18)
    d.text(500,766,'□ 保存截图后自动追加 Word',18)
    d.text(500,813,'Word 路径和插入方式可在“Word 报告”中调整',16,MUTED)
    d.rect(1174,185,400,704,'white',BORDER);d.text(1198,224,'配置效果预览',22,INK,700)
    d.text(1198,261,'保存后的文件示例',17,MUTED);d.rect(1198,280,352,174,'#f7fafc',BORDER)
    d.text(1214,309,'试验证据 / 批次 0043 / 原图',16,MUTED)
    letterbox(d,1214,326,320,65,0,1200,200);d.text(1214,427,'01_电压曲线.png',19,INK,600)
    d.text(1198,501,'剪贴板内容',17,MUTED);letterbox(d,1198,518,352,101,0,1200,200)
    d.text(1198,651,'按 Ctrl + Alt + C 复制该区域',17,BLUE,600)
    d.rect(1198,691,352,139,'#edf5ff',r=8)
    d.parts.append(f'<g transform="translate(1214 708) scale(.50)">{BRAND}</g>')
    d.text(1260,731,'AITSnap 托盘行为',18,INK,600)
    d.text(1214,772,'关闭窗口后仍可从通知区域打开',16,MUTED);d.text(1214,803,'“进入托盘”不表示开机自动启动',15,MUTED)
    d.rect(212,909,1362,67,'white',BORDER);d.text(232,950,'已保存的批次和报告不会随设置修改而改变',18,MUTED)
    d.button(1154,922,164,'放弃修改');d.button(1336,922,216,'保存并应用',True)
    d.save('07-settings-tab')

PAGES=[('01-workbench','工作台'),('04-sources-tab','源区域'),('05-scenes-tab','拼图场景'),('06-batches-reports-tab','批次与报告'),('07-settings-tab','设置')]

def board():
    html='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AITSnap 五个导航页概念审计</title><style>
    body{margin:0;background:#edf2f7;color:#17283d;font:16px "Microsoft YaHei UI",sans-serif}header{padding:20px 28px;background:white;border-bottom:1px solid #d9e2eb}h1{margin:0;font-size:25px}p{color:#52677e;margin:10px 0}nav{display:flex;gap:10px;flex-wrap:wrap}button{font:inherit;padding:10px 20px;border:1px solid #b8c8da;border-radius:7px;background:white;color:#17345a;cursor:pointer}button[aria-selected=true]{color:white;background:#2364d2;border-color:#2364d2}main{padding:16px}img{display:block;width:100%;height:auto;border-radius:10px}section[hidden]{display:none}footer{padding:0 28px 24px;color:#52677e}</style>
    <header><h1>AITSnap · 五个导航页概念审计</h1><p>沿用已获方向性肯定的工作台风格。以下为合成示例与待审布局，未改动软件。点击上方页签或图内左侧导航切换；其他按钮仅为绘制示意。</p><nav role="tablist">'''
    for i,(_,label) in enumerate(PAGES):html+=f'<button type="button" role="tab" id="tab{i}" aria-controls="panel{i}" aria-selected="{str(i==0).lower()}" onclick="show({i})">{label}</button>'
    html+='</nav></header><main>'
    # Image navigation hit regions are based on the common, explicit 1600x1000 shell geometry.
    for i,(name,label) in enumerate(PAGES):
        html+=f'<section role="tabpanel" id="panel{i}" aria-labelledby="tab{i}"'+(' hidden' if i else '')+f'><img id="image{i}" src="{name}.svg" alt="AITSnap {label}概念图" onclick="imageNav(event,this)"></section>'
    html+='''</main><footer>源区域、场景编辑和 Word 队列沿用现有能力；缩略图列表、浏览联动及效果预览为新增展示。真实 Word 页排版不在此处伪造。</footer><script>
    const total=5;function show(n){for(let i=0;i<total;i++){document.getElementById('panel'+i).hidden=i!==n;document.getElementById('tab'+i).setAttribute('aria-selected',String(i===n));}}
    function imageNav(e,img){const r=img.getBoundingClientRect(),x=(e.clientX-r.left)*1600/r.width,y=(e.clientY-r.top)*1000/r.height;if(x>=12&&x<=172){for(let i=0;i<5;i++)if(y>=100+i*60&&y<=146+i*60){show(i);document.getElementById('tab'+i).focus();return;}}}
    document.querySelector('nav').addEventListener('keydown',e=>{if(e.key!=='ArrowRight'&&e.key!=='ArrowLeft')return;e.preventDefault();const current=Array.from(document.querySelectorAll('nav button')).findIndex(b=>b.getAttribute('aria-selected')==='true');const n=(current+(e.key==='ArrowRight'?1:total-1))%total;show(n);document.getElementById('tab'+n).focus();});
    </script></html>'''
    (OUT/'tab-review-board.html').write_text(html,encoding='utf-8',newline='\n')

if __name__=='__main__':
    sources_page();scenes_page();batches_page();settings_page();board()
    print('Generated four tab SVG concepts and five-tab offline review board')
