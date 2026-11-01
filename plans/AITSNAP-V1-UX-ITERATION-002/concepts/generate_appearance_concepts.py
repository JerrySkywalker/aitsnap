"""Theme variants of authored SVG mockups; synthetic contents, not app evidence."""
from pathlib import Path
from xml.etree import ElementTree as ET
from generate_concepts import Diagram, OUT, INK, MUTED, BORDER, BLUE

def appearance_page():
    d = Diagram()
    d.shell('设置 · 外观', 'AITSnap 0.7.0 · 配色立即生效，语言更改在下次启动生效', 4)
    d.rect(212, 185, 244, 704, 'white', BORDER)
    d.text(234, 224, '设置分类', 22, INK, 700)
    for i, label in enumerate(['常用设置', '外观', '保存与命名', '快捷键与托盘', 'Word 报告', '元数据关联', '关于 AITSnap']):
        y = 249 + i * 58
        if i == 1: d.rect(226, y, 216, 44, '#e4edff', r=7)
        d.text(242, y + 29, label, 19, BLUE if i == 1 else MUTED, 600 if i == 1 else 400)
    d.rect(476, 185, 678, 704, 'white', BORDER)
    d.text(500, 224, '界面语言', 23, INK, 700)
    for x, label, selected in [(500, '跟随系统', True), (710, '简体中文', False), (920, 'English', False)]:
        d.rect(x, 253, 190, 56, '#e4edff' if selected else '#f8fafc', BLUE if selected else BORDER)
        d.text(x + 16, 289, ('● ' if selected else '○ ') + label, 19, BLUE if selected else INK)
    d.text(500, 347, '语言在下次启动生效，截图和已有文件名称保持原样。', 17, MUTED)
    d.line(500, 377, 1130, 377)
    d.text(500, 419, '色彩模式', 23, INK, 700)
    for x, label, fill, selected in [(500, '浅色', '#edf2f7', False), (710, '深色', '#151d29', False), (920, '跟随系统', '#e4edff', True)]:
        d.parts.append('<g data-fixed-colors="true">')
        d.rect(x, 447, 190, 130, fill, BLUE if selected else BORDER)
        d.rect(x + 15, 465, 160, 62, '#1e2938' if label == '深色' else 'white', r=6)
        d.rect(x + 26, 478, 38, 36, '#2364d2', r=5)
        d.text(x + 77, 503, 'Aa', 23, '#e6edf6' if label == '深色' else '#17283d', 700)
        d.text(x + 16, 558, ('● ' if selected else '○ ') + label, 18, '#e6edf6' if label == '深色' else '#17283d')
        d.parts.append('</g>')
    d.text(500, 622, '配色只改变软件界面，不改变截图或拼图输出的像素。', 17, MUTED)
    d.text(500, 661, '跟随系统会响应 Windows 应用颜色设置的变化。', 17, MUTED)
    d.rect(500, 707, 630, 122, '#edf5ff', r=8)
    d.text(520, 744, '同一套控件、同一个图标母版', 20, INK, 600)
    d.text(520, 780, '选中、焦点、错误与禁用状态在两种模式中保持可辨识。', 17, MUTED)
    d.rect(1174, 185, 400, 704, 'white', BORDER)
    d.text(1198, 224, '外观预览', 22, INK, 700)
    d.rect(1198, 254, 352, 344, '#f8fafc', BORDER)
    d.text(1218, 292, '#1 电压曲线', 22, INK, 600)
    d.mini(1218, 319, 312, 155, 0)
    d.text(1218, 514, '最近批次 · 1200 × 200', 17, MUTED)
    d.button(1218, 538, 312, '截图并保存', True)
    d.text(1198, 647, '截图示例保留原始颜色', 18, MUTED)
    d.text(1198, 687, '当前图为合成概念，非软件截图', 17, MUTED)
    d.button(1198, 762, 352, '应用设置', True)
    d.save('10-appearance-settings')

def dark_variant(path):
    palette = {'white': '#1e2938', '#edf2f7': '#151d29', '#f8fafc': '#192331',
               '#f8fbfd': '#243143', '#f6f8fb': '#253347', '#f7fafc': '#253347',
               '#eef4f9': '#253347', '#eef2f7': '#253347', '#f0f3f7': '#253347',
               '#edf5ff': '#223b5c', '#e1ebff': '#304e78', '#e4edff': '#304e78',
               '#d8e8ff': '#304e78', '#d9f1ed': '#23453f', '#d9e2eb': '#3b4b61',
               '#17283d': '#e6edf6', '#52677e': '#a5b4c7', '#2364d2': '#82b4ff',
               '#127f74': '#72d9c8', '#fff3d8': '#47391f', '#fff7e6': '#47391f',
               '#825b15': '#f2cd7f', '#9b6814': '#f2cd7f'}
    root = ET.fromstring(path.read_text(encoding='utf-8'))
    fixed = {child for group in root.iter() if group.get('data-fixed-colors') for child in group.iter()}
    for element in root.iter():
        if element in fixed: continue
        for attr in ('fill', 'stroke'):
            value = element.get(attr)
            if value in palette:
                # White text and the brand's blue remain legible on primary controls.
                if value == 'white' and (attr == 'stroke' or element.tag.endswith('text')): continue
                if attr == 'fill' and value == BLUE and not element.tag.endswith('text'): continue
                element.set(attr, palette[value])
    ET.register_namespace('', 'http://www.w3.org/2000/svg')
    path.with_name(path.stem + '-dark.svg').write_text(ET.tostring(root, encoding='unicode'), encoding='utf-8', newline='\n')

def board():
    files = ['01-workbench', '08-display-layout-default', '04-sources-tab', '05-scenes-tab',
             '06-batches-reports-tab', '07-settings-tab', '10-appearance-settings', '09-display-layout-change']
    options = ''.join(f'<option value="{name}">{name}</option>' for name in files)
    html = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>AITSnap 0.7.0 外观概念</title>
<style>body{margin:20px;background:#151d29;color:#e6edf6;font:16px sans-serif}select,button{padding:8px;margin-right:8px}img{display:block;width:100%;max-width:1600px;margin-top:16px}a{color:#82b4ff}</style>
<h1>AITSnap 0.7.0 浅色 / 深色概念</h1><p>合成示例、待审设计；不是产品运行或人工验收证据。</p>
<select id="page">''' + options + '''</select><select id="mode"><option value="">浅色</option><option value="-dark">深色</option></select>
<a href="tab-review-board.html">原导航审计面板</a><img id="preview" alt="AITSnap 外观概念图">
<script>function show(){preview.src=page.value+mode.value+'.png'}page.onchange=mode.onchange=show;show()</script></html>'''
    (OUT / 'appearance-review-board.html').write_text(html, encoding='utf-8', newline='\n')

if __name__ == '__main__':
    appearance_page()
    for path in OUT.glob('[0-9][0-9]-*.svg'):
        if not path.stem.endswith('-dark'): dark_variant(path)
    board()
