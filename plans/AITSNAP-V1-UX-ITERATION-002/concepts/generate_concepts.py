"""Reproducible, synthetic UI review diagrams. No product or user screenshot inputs."""
from pathlib import Path
from html import escape

OUT=Path(__file__).resolve().parent
INK='#17283d'; MUTED='#52677e'; BORDER='#d9e2eb'; BLUE='#2364d2'; MINT='#127f74'; BG='#edf2f7'
BRAND='<rect width="64" height="64" rx="16" fill="#2364d2"/><g fill="none" stroke="white" stroke-width="4" stroke-linejoin="round"><path d="M15 25V15h10M39 15h10v10M49 39v10H39M25 49H15V39"/><rect x="23" y="23" width="18" height="18" rx="3"/></g>'

class Diagram:
    def __init__(self):
        self.parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1000" viewBox="0 0 1600 1000"><style>text{font-family:"Microsoft YaHei UI","Microsoft YaHei",sans-serif}</style>']
    def rect(self,x,y,w,h,fill='white',stroke=None,r=12):
        self.parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}"'+(f' stroke="{stroke}"' if stroke else '')+'/>')
    def text(self,x,y,s,size=20,color=INK,weight=400):
        self.parts.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" font-weight="{weight}">{escape(s)}</text>')
    def line(self,x1,y1,x2,y2,color=BORDER,width=1,dash=False):
        self.parts.append(f'<path d="M{x1} {y1}L{x2} {y2}" stroke="{color}" stroke-width="{width}"'+(' stroke-dasharray="6 5"' if dash else '')+'/>')
    def badge(self,x,y,n,color=BLUE):
        self.rect(x,y,30,30,color,r=9); self.text(x+9,y+22,str(n),18,'white',700)
    def button(self,x,y,w,s,primary=False):
        self.rect(x,y,w,42,BLUE if primary else 'white',None if primary else BORDER,r=8)
        self.text(x+16,y+28,s,18,'white' if primary else INK,600)
    def mini(self,x,y,w,h,kind=0):
        self.rect(x,y,w,h,'#eaf5f4',r=6)
        if kind==0:
            self.line(x+12,y+h-15,x+w-10,y+h-15,'#b4d2d0')
            points=[(0,.65),(.15,.65),(.20,.2),(.3,.8),(.45,.35),(.6,.43),(.75,.23),(.87,.57),(1,.4)]
            v=' '.join(f'{x+12+(w-24)*a},{y+12+(h-30)*b}' for a,b in points)
            self.parts.append(f'<polyline points="{v}" fill="none" stroke="{MINT}" stroke-width="3"/>')
        elif kind==1:
            for i,a in enumerate([.83,.55,.9,.68]): self.rect(x+12,y+13+i*(h-20)/5,(w-24)*a,4,'#829bb0',r=2)
        elif kind==2:
            self.rect(x+w*.18,y+h*.18,w*.64,h*.64,'#c9d9e8',r=4)
            self.rect(x+w*.38,y+h*.30,w*.25,h*.37,'#849db6',r=4)
        else:
            for i,a in enumerate([.5,.85,.65,.95]): self.rect(x+12+i*(w-24)/4,y+h*(1-a)*.7+10,(w-35)/5,h*a*.7,'#40a99b',r=3)
    def shell(self,title,subtitle,nav=0):
        self.rect(0,0,1600,1000,BG,r=0); self.rect(0,0,1600,74,'white',r=0)
        self.parts.append(f'<g transform="translate(22 15) scale(.68)">{BRAND}</g>')
        self.text(80,46,'AITSnap',27,INK,700); self.text(250,45,'多屏证据工作台',20,MUTED)
        self.text(1110,44,'概念草案 / 待审 · 合成示例 · 未实现',17,MUTED)
        self.rect(0,76,184,924,'#f8fafc',r=0)
        for i,n in enumerate(['工作台','源区域','拼图场景','批次与报告','设置']):
            if i==nav:self.rect(12,100+i*60,160,46,'#e1ebff',r=8)
            self.text(32,131+i*60,n,20,BLUE if i==nav else MUTED,600 if i==nav else 400)
        self.text(23,903,'图标为同一母版草案',13,MUTED)
        self.text(23,930,'并非最终品牌批准',13,MUTED)
        self.text(214,122,title,30,INK,700); self.text(214,155,subtitle,18,MUTED)
    def save(self,name):
        (OUT/(name+'.svg')).write_text(''.join(self.parts)+'</svg>',encoding='utf-8',newline='\n')

names=['电压曲线','串口日志','相机画面','电源状态']

def overview():
    d=Diagram();d.shell('看清来源，再确认输出','示例：3 个显示器 · 4 个启用区域 · 输出 4 张原图 + 2 张拼图')
    d.rect(212,185,292,674,'white',BORDER);d.text(232,222,'源区域  4',22,INK,700)
    for i,n in enumerate(names):
        y=241+i*137;d.rect(228,y,260,124,'#f8fbfd',BLUE if i==0 else BORDER)
        d.badge(240,y+12,i+1);d.text(280,y+35,n,20,INK,600)
        d.mini(240,y+52,88,57,i);d.text(340,y+74,['显示器 A','显示器 A','显示器 B','显示器 C'][i],16,MUTED)
        d.text(340,y+99,['1200×200','600×480','1280×720','600×400'][i],16,MUTED)
    d.rect(524,185,698,674,'white',BORDER);d.text(548,222,'显示器布局',22,INK,700)
    d.text(548,254,'点击屏幕内区域，与左侧源卡片和右侧输出联动高亮',17,MUTED)
    d.rect(554,309,290,188,'#eef4f9','#7f95aa',r=8);d.text(569,339,'A · 1920×1080',17,MUTED)
    d.rect(573,353,251,46,'#d8e8ff',BLUE,r=3);d.badge(578,360,1);d.text(617,382,'电压曲线',17,BLUE,600)
    d.rect(573,414,143,65,'#d9f1ed',MINT,r=3);d.badge(580,420,2,MINT);d.text(615,445,'串口日志',16,MINT)
    d.rect(864,289,322,208,'#eef4f9','#7f95aa',r=8);d.text(880,320,'B · 2560×1440',17,MUTED)
    d.rect(885,342,280,130,'#d9f1ed',MINT,r=3);d.badge(894,353,3,MINT);d.text(934,377,'相机画面',17,MINT,600)
    d.rect(713,522,141,245,'#eef4f9','#7f95aa',r=8);d.text(725,548,'C · 竖屏',17,MUTED)
    d.rect(725,576,118,104,'#d9f1ed',MINT,r=3);d.badge(733,585,4,MINT);d.text(734,651,'电源状态',16,MINT)
    d.text(884,600,'当前选中',16,MUTED);d.text(884,635,'#1 电压曲线',23,BLUE,700)
    d.text(884,668,'可定位到屏幕、重命名或框选',16,MUTED)
    d.button(884,693,180,'定位 / 修改区域')
    d.rect(548,795,650,42,'#fff3d8',r=8);d.text(562,823,'区域有修改 · 最近批次仍是旧尺寸 · 重新采集后更新预览',16,'#825b15')
    d.rect(1242,185,332,674,'white',BORDER);d.text(1264,222,'下一次输出',22,INK,700)
    d.text(1264,257,'✓ 原图 4 张',19,MINT,600);d.text(1264,286,'✓ 拼图 2 张',19,MINT,600)
    for i,(title,sub) in enumerate([('试验总览','1600×900 · 4 个源'),('曲线与日志','1600×900 · 2 个源')]):
        y=312+i*207;d.rect(1258,y,300,190,'#f8fbfd',BORDER);d.text(1274,y+29,title,20,INK,600)
        if i==0:
            for j,(xx,yy) in enumerate([(1274,y+44),(1408,y+44),(1274,y+94),(1408,y+94)]):
                d.mini(xx,yy,126,39,j);d.rect(xx+4,yy+3,19,18,'white',r=4);d.text(xx+9,yy+17,str(j+1),12,BLUE,700)
        else:
            d.mini(1274,y+44,126,86,0);d.mini(1408,y+44,132,86,1)
        d.text(1274,y+159,sub,16,MUTED)
    d.text(1262,772,'文件名预览',17,MUTED);d.text(1262,803,'原图 / 01_电压曲线.png',16,INK)
    d.rect(212,879,1362,94,'white',BORDER);d.text(232,915,'当前配置：试验记录   ·   Word 自动追加：关闭',18,MUTED)
    d.text(232,947,'最近真实批次 10:48:59 · 不会自动更新旧图',16,MUTED)
    d.button(966,903,166,'编辑输出布局');d.button(1148,903,142,'保存并复制');d.button(1306,903,242,'截图并保存  Ctrl+Alt+S',True)
    d.save('01-workbench')

def regions():
    d=Diagram();d.shell('屏幕框选 · 名称与编号直接对应','示例显示器 A · 所有标签位于编辑覆盖层，不写入截图',1)
    d.rect(212,185,302,716,'white',BORDER);d.text(234,223,'这块屏幕上的区域',22,INK,700)
    for i in range(2):
        y=249+i*144;d.rect(228,y,270,128,'#edf4ff' if i==0 else '#f7fafc',BLUE if i==0 else BORDER)
        d.badge(241,y+14,i+1);d.text(281,y+37,names[i],22,INK,600)
        d.text(243,y+74,['1200×200  ·  横向','600×480  ·  近方形'][i],17,MUTED)
        d.text(243,y+103,'启用  ·  该编号跨场景保持一致',16,MINT)
    d.text(236,589,'选中区域名称',17,MUTED);d.rect(230,608,266,46,'white',BORDER,r=7);d.text(243,638,'电压曲线',21)
    d.button(230,678,266,'+ 添加区域')
    d.text(233,767,'高级：X / Y / 宽 / 高',17,MUTED)
    d.text(233,800,'坐标参数保留为折叠面板',17,MUTED)
    d.rect(534,185,1040,716,'#dae3ed',BORDER);d.text(557,223,'显示器 A   1920×1080   ·   150% 缩放示例',18,MUTED)
    d.rect(556,246,996,581,'#c8d4e2',r=6)
    for i in range(6): d.line(576,282+i*84,1528,282+i*84,'#bbc9d8',1)
    d.rect(579,265,944,159,'#e1edfd',BLUE,r=3)
    d.rect(590,276,364,43,'#174c9b',r=7);d.badge(596,282,1);d.text(635,305,'电压曲线   ·   1200×200',20,'white',600)
    d.text(1128,305,'标签在框内避让，不越过屏幕顶边',16,BLUE)
    d.mini(603,333,896,67,0)
    d.rect(650,472,398,318,'#e0f2ed',MINT,r=3);d.rect(662,484,349,43,MINT,r=7);d.badge(668,490,2,'#075f57');d.text(707,513,'串口日志   ·   600×480',20,'white',600)
    d.mini(675,551,348,208,1)
    for x,y in [(579,265),(1523,265),(579,424),(1523,424)]:d.rect(x-6,y-6,12,12,BLUE,r=2)
    d.text(1138,541,'编号 + 名称',25,INK,700);d.text(1138,581,'来源卡片和框内标签一致',18,MUTED)
    d.text(1138,617,'清晰的选择、拖动与调整',18,MUTED);d.text(1138,653,'小框 / 顶部 / 跨屏有避让',18,MUTED)
    d.text(557,864,'拖动空白处新建 · 点击选中 · Tab 切换区域 · Esc 取消',18,MUTED)
    d.button(1106,926,146,'取消修改');d.button(1272,926,302,'应用 2 个区域',True)
    d.save('02-region-selection')

def composer():
    d=Diagram();d.shell('拼图场景 · 先摆图片，再决定输出标题','试验总览 · 1600×900 · 4 个图层 · 编辑器标签与图片独立',2)
    d.rect(212,185,286,710,'white',BORDER);d.text(232,222,'可复用来源',22,INK,700)
    for i,n in enumerate(names):
        y=245+i*112;d.rect(226,y,258,100,'#f8fbfd',BLUE if i==0 else BORDER);d.badge(238,y+11,i+1);d.text(278,y+34,n,19,INK,600)
        d.mini(238,y+46,98,42,i);d.text(350,y+68,'拖入画布',16,MUTED)
    d.text(232,754,'来源：最近真实批次',17,MUTED);d.text(232,783,'采集时间 10:48:59',17,MUTED)
    d.text(232,815,'没有真实源时显示缺源占位',15,MUTED)
    d.rect(518,185,740,710,'white',BORDER);d.text(540,222,'画布预览',22,INK,700)
    d.button(540,241,110,'行排列');d.button(660,241,110,'网格');d.button(780,241,110,'撤销');d.button(1104,241,128,'适合窗口')
    d.rect(538,305,700,395,'#dfe7f0',r=4)
    d.mini(551,323,674,110,0);d.rect(550,322,676,112,'none',BLUE,r=0)
    d.rect(560,332,211,36,'white',BLUE,r=6);d.badge(564,335,1);d.text(604,357,'电压曲线',18,BLUE,600)
    d.mini(552,451,318,232,1);d.mini(888,451,337,135,2);d.mini(888,602,337,82,3)
    for n,x,y in [(2,560,460),(3,898,460),(4,898,612)]:
        d.rect(x,y,170,35,'white',BORDER,r=6);d.badge(x+4,y+3,n);d.text(x+42,y+25,names[n-1],17)
    d.rect(540,726,692,67,'#e8f5f2',r=8);d.text(556,754,'编辑器标签：独立清晰，跟随图层定位，不随图片拉伸',17,MINT,600)
    d.text(556,780,'以上编号框仅用于编辑；默认不会写入导出 PNG',16,MINT)
    d.text(540,845,'缩放 50%   ·   文字保持可读   ·   原图不会被改写',17,MUTED)
    d.rect(1278,185,296,710,'white',BORDER);d.text(1298,222,'#1 电压曲线',22,INK,700)
    for y,a,b in [(264,'源图实际尺寸','1200×200'),(326,'当前框选尺寸','1200×200'),(388,'目标布局框','1540×260')]:
        d.text(1298,y,a,16,MUTED);d.text(1298,y+29,b,21)
    d.text(1298,465,'✓ 等比适配',20,MINT,600);d.text(1298,498,'保留图片比例，必要时留白',16,MUTED)
    d.text(1298,556,'编辑器身份标签',19,INK,600);d.text(1298,587,'始终清晰 · 不导出',17,MUTED)
    d.text(1298,650,'输出标题',19,INK,600);d.text(1298,683,'□ 将区域名称作为标题',17,MUTED)
    d.text(1298,713,'启用后在最终分辨率排版',16,MUTED)
    d.text(1298,801,'高级：裁剪 / 旋转 / Z',16,MUTED)
    d.button(840,925,170,'预览导出文件');d.button(1026,925,168,'取消');d.button(1210,925,364,'保存场景：试验总览',True)
    d.save('03-scene-composer')

def generate_originals():
    overview();regions();composer()
    (OUT/'brandmark-proposal.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">'+BRAND+'</svg>',encoding='utf-8',newline='\n')

    html='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>AITSnap 下一轮界面概念审计</title>
    <meta name="viewport" content="width=device-width,initial-scale=1"><style>body{margin:0;background:#edf2f7;color:#17283d;font:16px "Microsoft YaHei UI",sans-serif}header{padding:22px 28px;background:white;border-bottom:1px solid #d9e2eb}h1{margin:0 0 9px;font-size:25px}p{margin:8px 0;color:#52677e}nav{display:flex;gap:10px;margin-top:16px}button{font:inherit;border:1px solid #b8c8da;border-radius:7px;background:white;color:#17345a;padding:10px 20px;cursor:pointer}button[aria-selected=true]{background:#2364d2;color:white;border-color:#2364d2}main{padding:16px}img{display:block;width:100%;height:auto;background:white;border-radius:10px}section[hidden]{display:none}a{color:#2364d2}</style>
    <header><h1>AITSnap 下一轮 UI 概念审计</h1><p>合成示例 · 尚未实现或获得 Owner 批准。三屏四区域仅表达设计关系，不是现场监控截图。</p><p>审计重点：来源与输出是否清晰、编号名称能否对应、文字是否独立、是否接受稳定编号与高级参数折叠。</p><nav role="tablist">'''
    for i,(n,label) in enumerate([('01-workbench','工作台总览'),('02-region-selection','多屏框选'),('03-scene-composer','拼图编辑')]):
        html+=f'<button role="tab" id="tab{i}" aria-controls="panel{i}" aria-selected="{str(i==0).lower()}" onclick="show({i})">{label}</button>'
    html+='</nav></header><main>'
    for i,n in enumerate(['01-workbench','02-region-selection','03-scene-composer']):
        html+=f'<section role="tabpanel" id="panel{i}" aria-labelledby="tab{i}"'+(' hidden' if i else '')+f'><img src="{n}.svg" alt="AITSnap概念图：{n}"></section>'
    html+='''</main><script>function show(n){for(let i=0;i<3;i++){document.getElementById('panel'+i).hidden=i!==n;document.getElementById('tab'+i).setAttribute('aria-selected',String(i===n));}}document.querySelector('nav').addEventListener('keydown',e=>{if(e.key!=='ArrowRight'&&e.key!=='ArrowLeft')return;const current=Array.from(document.querySelectorAll('button')).findIndex(b=>b.getAttribute('aria-selected')==='true');const n=(current+(e.key==='ArrowRight'?1:2))%3;show(n);document.getElementById('tab'+n).focus();});</script></html>'''
    (OUT/'review-board.html').write_text(html,encoding='utf-8',newline='\n')
    print('Generated three SVG diagrams, one shared draft brandmark and local review board')

if __name__ == "__main__":
    generate_originals()
