#!/usr/bin/env python3
"""Convert a WinForms Form1.Designer.cs into a Qt6 .ui file plus a C++ MainWindow skeleton.

Keeps the C# control names so handler code can be ported line by line.
Usage: wf2qt.py Form1.Designer.cs outdir [--ui-only]

--ui-only regenerates mainwindow.ui only. Use it from now on: mainwindow.h/.cpp are
hand-maintained since the Slots tab was ported and would be overwritten otherwise.
"""
import re, sys, os, html
from collections import OrderedDict

src = open(sys.argv[1], encoding='utf-8', errors='replace').read()
out = sys.argv[2]
os.makedirs(out, exist_ok=True)

types = OrderedDict()
for n, t in re.findall(r'^\s*(\w+) = new ([\w\.]+)\((?:components)?\);', src, re.M):
    if n not in types:
        types[n] = t.split('.')[-1]

props = {}
for n, p, v in re.findall(r'^\s*(\w+)\.(\w+) = (.+?);\s*$', src, re.M):
    props.setdefault(n, {})[p] = v
# form-level (no prefix)
formprops = dict(re.findall(r'^\s{12}(\w+) = (.+?);\s*$', src, re.M))

items = {}
for n, body in re.findall(r'^\s*(\w+)\.Items\.AddRange\(new object\[\] \{(.*?)\}\);', src, re.M | re.S):
    items[n] = re.findall(r'"((?:[^"\\]|\\.)*)"', body)

children = OrderedDict()
for parent, child in re.findall(r'^\s*(\w+)\.Controls\.Add\((\w+)\);', src, re.M):
    children.setdefault(parent, []).append(child)
top = re.findall(r'^\s{12}Controls\.Add\((\w+)\);', src, re.M)

events = []
for n, ev, h in re.findall(r'^\s*(\w+)\.(\w+) \+= (\w+);', src, re.M):
    events.append((n, ev, h))
for ev, h in re.findall(r'^\s{12}(\w+) \+= (\w+);', src, re.M):
    events.append(('this', ev, h))

columns = [(n, re.sub(r'^"|"$', '', props[n].get('HeaderText', '""')), int(props[n].get('Width', '100')))
           for n, t in types.items() if t == 'DataGridViewTextBoxColumn']


def unq(v):
    m = re.match(r'^"(.*)"$', v, re.S)
    if not m:
        return None
    s = m.group(1)
    s = s.replace('\\r\\n', '\n').replace('\\n', '\n').replace('\\r', '').replace('\\t', '\t')
    s = s.replace('\\"', '"').replace("\\'", "'").replace('\\\\', '\\')
    return s


def pt(v, kind):
    m = re.search(r'\((-?\d+),\s*(-?\d+)\)', v or '')
    return (int(m.group(1)), int(m.group(2))) if m else None


def colour(v):
    if not v:
        return None
    m = re.search(r'FromArgb\((\d+),\s*(\d+),\s*(\d+)\)', v)
    if m:
        return 'rgb(%s, %s, %s)' % m.groups()
    m = re.search(r'Color\.(\w+)$', v)
    if m and m.group(1) not in ('Transparent', 'Empty'):
        return m.group(1).lower()
    return None  # SystemColors -> leave as default


def font(v):
    m = re.search(r'Font\("([^"]+)",\s*([\d\.]+)F(.*)\)', v or '')
    if not m:
        return None
    return m.group(1), float(m.group(2)), 'Bold' in m.group(3), 'Italic' in m.group(3)


QT = {
    'Label': 'QLabel', 'Button': 'QPushButton', 'CheckBox': 'QCheckBox', 'ListBox': 'QListWidget',
    'ComboBox': 'QComboBox', 'TextBox': 'QLineEdit', 'GroupBox': 'QGroupBox', 'Panel': 'QFrame',
    'TrackBar': 'QSlider', 'MonthCalendar': 'QCalendarWidget', 'DataGridView': 'QTableView',
    'VolumeMeter': 'QProgressBar', 'TabPage': 'QWidget', 'TabControl': 'QTabWidget',
}
hidden, disabled_note, unmapped = [], [], []


def qtclass(n):
    t = types.get(n, '?')
    p = props.get(n, {})
    if t == 'TextBox' and p.get('Multiline') == 'true':
        return 'QPlainTextEdit'
    if t == 'DateTimePicker':
        return 'QTimeEdit' if 'H' in (unq(p.get('CustomFormat', '""')) or '') and 'yy' not in (unq(p.get('CustomFormat', '""')) or '') else 'QDateEdit'
    if t not in QT:
        unmapped.append((n, t))
        return 'QWidget'
    return QT[t]


def esc(s):
    return html.escape(s, quote=False)


def prop(name, inner, ind):
    return f'{ind}<property name="{name}">{inner}</property>\n'


emitted = set()


def widget_xml(n, ind):
    emitted.add(n)
    t = types.get(n, '?')
    p = props.get(n, {})
    cls = qtclass(n)
    x = f'{ind}<widget class="{cls}" name="{n}">\n'
    i2 = ind + ' '
    loc = pt(p.get('Location'), 'p') or (0, 0)
    size = pt(p.get('Size'), 's')
    if t == 'ComboBox' and 'Simple' in p.get('DropDownStyle', '') and size:
        size = (size[0], 22)   # Simple style (always-open list) has no Qt equivalent -> normal editable combo
    if t == 'MonthCalendar' and not size:
        size = (248, 170)
    if size:
        x += prop('geometry', f'<rect><x>{loc[0]}</x><y>{loc[1]}</y><width>{size[0]}</width><height>{size[1]}</height></rect>', i2)
    f = font(p.get('Font'))
    if f:
        x += prop('font', f'<font><family>{f[0]}</family><pointsize>{round(f[1])}</pointsize>'
                  + ('<bold>true</bold>' if f[2] else '') + ('<italic>true</italic>' if f[3] else '') + '</font>', i2)
    css = []
    bg, fg = colour(p.get('BackColor')), colour(p.get('ForeColor'))
    if bg:
        css.append(f'background-color: {bg};')
    if fg:
        css.append(f'color: {fg};')
    if css:
        x += prop('styleSheet', f'<string notr="true">{cls}#{n} {{ {" ".join(css)} }}</string>', i2)
    if p.get('Visible') == 'false':
        hidden.append(n)
    if p.get('Enabled') == 'false':
        x += prop('enabled', '<bool>false</bool>', i2)
    text = unq(p.get('Text', '')) if 'Text' in p else None
    if text is not None:
        if cls == 'QLabel':
            x += prop('text', f'<string>{esc(text.replace("&&", "&"))}</string>', i2)
        elif cls in ('QPushButton', 'QCheckBox'):
            x += prop('text', f'<string>{esc(text)}</string>', i2)
        elif cls == 'QGroupBox':
            x += prop('title', f'<string>{esc(text)}</string>', i2)
        elif cls in ('QLineEdit',):
            x += prop('text', f'<string>{esc(text)}</string>', i2)
        elif cls == 'QPlainTextEdit':
            x += prop('plainText', f'<string>{esc(text)}</string>', i2)
    if cls in ('QCheckBox',) and p.get('Checked') == 'true':
        x += prop('checked', '<bool>true</bool>', i2)
    if cls in ('QLineEdit', 'QPlainTextEdit') and p.get('ReadOnly') == 'true':
        x += prop('readOnly', '<bool>true</bool>', i2)
    if cls == 'QLineEdit' and ('PasswordChar' in p or p.get('UseSystemPasswordChar') == 'true'):
        x += prop('echoMode', '<enum>QLineEdit::EchoMode::Password</enum>', i2)
    if cls == 'QComboBox':
        ds = p.get('DropDownStyle', '')
        if 'DropDownList' not in ds:
            x += prop('editable', '<bool>true</bool>', i2)
    if cls == 'QSlider':
        vert = 'Vertical' in p.get('Orientation', '')
        x += prop('orientation', f'<enum>Qt::Orientation::{"Vertical" if vert else "Horizontal"}</enum>', i2)
        for k, q in (('Minimum', 'minimum'), ('Maximum', 'maximum'), ('Value', 'value')):
            if k in p and re.match(r'^-?\d+$', p[k]):
                x += prop(q, f'<number>{p[k]}</number>', i2)
        x += prop('tickPosition', '<enum>QSlider::TickPosition::TicksBelow</enum>', i2)
    if cls == 'QProgressBar':
        vert = size and size[1] > size[0]
        x += prop('orientation', f'<enum>Qt::Orientation::{"Vertical" if vert else "Horizontal"}</enum>', i2)
        x += prop('value', '<number>0</number>', i2)
        x += prop('textVisible', '<bool>false</bool>', i2)
    if cls == 'QFrame':
        x += prop('frameShape', '<enum>QFrame::Shape::StyledPanel</enum>', i2)
    if cls == 'QTimeEdit':
        x += prop('displayFormat', '<string>HH:mm</string>', i2)
    if cls == 'QDateEdit':
        x += prop('displayFormat', '<string>yyyy-MM-dd</string>', i2)
        x += prop('calendarPopup', '<bool>true</bool>', i2)
    if cls == 'QCalendarWidget':
        x += prop('gridVisible', '<bool>false</bool>', i2)
        x += prop('verticalHeaderFormat', '<enum>QCalendarWidget::VerticalHeaderFormat::NoVerticalHeader</enum>', i2)
        x += prop('horizontalHeaderFormat', '<enum>QCalendarWidget::HorizontalHeaderFormat::ShortDayNames</enum>', i2)
        x += prop('styleSheet', '<string notr="true">QCalendarWidget QAbstractItemView { background-color: white; }</string>', i2)
    if cls == 'QTableView':
        x += prop('selectionMode', '<enum>QAbstractItemView::SelectionMode::SingleSelection</enum>', i2)
        x += prop('selectionBehavior', '<enum>QAbstractItemView::SelectionBehavior::SelectItems</enum>', i2)
        x += prop('editTriggers', '<set>QAbstractItemView::EditTrigger::NoEditTriggers</set>', i2)
        x += prop('contextMenuPolicy', '<enum>Qt::ContextMenuPolicy::CustomContextMenu</enum>', i2)
        x += prop('alternatingRowColors', '<bool>false</bool>', i2)
    if cls == 'QListWidget' or cls == 'QComboBox':
        for it in items.get(n, []):
            s = unq('"' + it + '"')
            x += f'{i2}<item><property name="text"><string>{esc(s)}</string></property></item>\n'
    # WinForms: first child added is front-most; Qt: last child is front-most -> reverse
    for c in reversed(children.get(n, [])):
        x += widget_xml(c, i2)
    x += f'{ind}</widget>\n'
    return x


tabctl = [c for c in top if types.get(c) == 'TabControl'][0]
fsize = pt(formprops.get('ClientSize'), 's') or (1185, 677)
title = unq(formprops.get('Text', '"MainWindow"'))
formbg = colour(formprops.get('BackColor'))
ffont = font(formprops.get('Font'))

ui = '<?xml version="1.0" encoding="UTF-8"?>\n<ui version="4.0">\n <class>MainWindow</class>\n'
ui += ' <widget class="QMainWindow" name="MainWindow">\n'
ui += prop('geometry', f'<rect><x>0</x><y>0</y><width>{fsize[0]}</width><height>{fsize[1]}</height></rect>', '  ')
ui += prop('windowTitle', f'<string>{esc(title)}</string>', '  ')
if ffont:
    ui += prop('font', f'<font><family>{ffont[0]}</family><pointsize>{round(ffont[1])}</pointsize></font>', '  ')
ui += '  <widget class="QWidget" name="centralwidget">\n'
ui += '   <layout class="QVBoxLayout" name="centralLayout">\n'
for m in ('leftMargin', 'topMargin', 'rightMargin', 'bottomMargin'):
    ui += prop(m, '<number>0</number>', '    ')
ui += '    <item>\n'
ui += f'     <widget class="QTabWidget" name="{tabctl}">\n'
pagecss = f'QWidget#%s {{ background-color: {formbg or "rgb(240,240,240)"}; }}'
ui += prop('currentIndex', '<number>0</number>', '      ')
for page in children[tabctl]:
    pp = props.get(page, {})
    ptitle = unq(pp.get('Text', '""')).replace('&', '&&')
    ui += f'      <widget class="QWidget" name="{page}">\n'
    ui += f'       <attribute name="title"><string>{esc(ptitle)}</string></attribute>\n'
    pbg = colour(pp.get('BackColor')) or formbg
    if pbg:
        ui += prop('styleSheet', f'<string notr="true">QWidget#{page} {{ background-color: {pbg}; }}</string>', '       ')
    if pp.get('Visible') == 'false':
        hidden.append(page)
    for c in reversed(children.get(page, [])):
        ui += widget_xml(c, '       ')
    ui += '      </widget>\n'
ui += '     </widget>\n    </item>\n'
formlevel = sorted([c for c in top if types.get(c) != 'TabControl'],
                   key=lambda c: (pt(props.get(c, {}).get('Location'), 'p') or (0, 0))[0])
if formlevel:
    ui += '    <item>\n     <widget class="QWidget" name="cornerWidget">\n'
    ui += '      <layout class="QHBoxLayout" name="cornerLayout">\n'
    for m in ('leftMargin', 'topMargin', 'bottomMargin'):
        ui += prop(m, '<number>0</number>', '       ')
    ui += prop('rightMargin', '<number>6</number>', '       ')
    ui += prop('spacing', '<number>8</number>', '       ')
    for c in formlevel:
        props.setdefault(c, {}).pop('Location', None)
        props[c].pop('Size', None)
        ui += '       <item>\n' + widget_xml(c, '        ') + '       </item>\n'
    ui += '      </layout>\n     </widget>\n    </item>\n'
ui += '   </layout>\n  </widget>\n </widget>\n <resources/>\n <connections/>\n</ui>\n'
open(os.path.join(out, 'mainwindow.ui'), 'w', encoding='utf-8').write(ui)

# ---------------- C++ skeleton ----------------
connects, slots_decl, stubs, filter_cases, notes = [], [], [], [], []
seen_handlers = set()
kp_objs, filter_objs = [], set()


def slot(sig, handler, comment):
    if handler in seen_handlers:
        return
    seen_handlers.add(handler)
    slots_decl.append(f'    void {handler}({sig});')
    stubs.append(f'// C#: {comment}\nvoid MainWindow::{handler}({sig})\n{{\n    // TODO: port from Form1.cs\n}}\n')


timers = []
for n, ev, h in events:
    t = types.get(n, 'Form' if n == 'this' else '?')
    cls = qtclass(n) if n in types and types[n] not in ('Timer', 'FolderBrowserDialog') else ''
    c = f'{n}.{ev} += {h}'
    if n == 'this' and ev == 'KeyPress':
        slot('QKeyEvent *e', h, c + '  (KeyPreview; return true to swallow, like e.Handled = true)')
        slots_decl[-1] = slots_decl[-1].replace('void ', 'bool ')
        stubs[-1] = stubs[-1].replace(f'void MainWindow::{h}', f'bool MainWindow::{h}').replace('// TODO: port from Form1.cs\n', '// TODO: port from Form1.cs\n    return false;\n')
        continue
    if t == 'Button' and ev == 'Click':
        connects.append(f'    connect(ui->{n}, &QPushButton::clicked, this, &MainWindow::{h});')
        slot('', h, c)
    elif t == 'CheckBox' and ev == 'CheckedChanged':
        connects.append(f'    connect(ui->{n}, &QCheckBox::toggled, this, &MainWindow::{h});')
        slot('bool checked', h, c)
    elif t == 'ComboBox' and ev in ('SelectedIndexChanged', 'SelectedValueChanged'):
        connects.append(f'    connect(ui->{n}, &QComboBox::currentIndexChanged, this, &MainWindow::{h});')
        slot('int index', h, c)
    elif t == 'ComboBox' and ev == 'TextChanged':
        connects.append(f'    connect(ui->{n}, &QComboBox::currentTextChanged, this, &MainWindow::{h});')
        slot('const QString &text', h, c)
    elif t == 'ListBox' and ev == 'SelectedIndexChanged':
        connects.append(f'    connect(ui->{n}, &QListWidget::currentRowChanged, this, &MainWindow::{h});')
        slot('int row', h, c)
    elif t == 'ListBox' and ev in ('MouseDoubleClick',):
        connects.append(f'    connect(ui->{n}, &QListWidget::itemDoubleClicked, this, &MainWindow::{h});')
        slot('QListWidgetItem *item', h, c)
    elif t == 'ListBox' and ev == 'MouseClick':
        connects.append(f'    connect(ui->{n}, &QListWidget::itemClicked, this, &MainWindow::{h});')
        slot('QListWidgetItem *item', h, c)
    elif t == 'TextBox' and ev == 'TextChanged':
        sig = 'QPlainTextEdit::textChanged' if cls == 'QPlainTextEdit' else 'QLineEdit::textChanged'
        connects.append(f'    connect(ui->{n}, &{sig}, this, &MainWindow::{h});')
        slot('' if cls == 'QPlainTextEdit' else 'const QString &text', h, c)
    elif t == 'TrackBar' and ev == 'ValueChanged':
        connects.append(f'    connect(ui->{n}, &QSlider::valueChanged, this, &MainWindow::{h});')
        slot('int value', h, c)
    elif t == 'DateTimePicker' and ev == 'ValueChanged':
        if cls == 'QTimeEdit':
            connects.append(f'    connect(ui->{n}, &QTimeEdit::timeChanged, this, &MainWindow::{h});')
            slot('QTime time', h, c)
        else:
            connects.append(f'    connect(ui->{n}, &QDateEdit::dateChanged, this, &MainWindow::{h});')
            slot('QDate date', h, c)
    elif t == 'MonthCalendar' and ev == 'DateChanged':
        connects.append(f'    connect(ui->{n}, &QCalendarWidget::selectionChanged, this, &MainWindow::{h});')
        slot('', h, c)
    elif t == 'MonthCalendar' and ev == 'DateSelected':
        connects.append(f'    connect(ui->{n}, &QCalendarWidget::clicked, this, &MainWindow::{h});')
        slot('QDate date', h, c)
    elif t == 'TabControl' and ev == 'SelectedIndexChanged':
        connects.append(f'    connect(ui->{n}, &QTabWidget::currentChanged, this, &MainWindow::{h});')
        slot('int index', h, c)
    elif t == 'DataGridView':
        if ev == 'CellMouseClick':
            connects.append(f'    connect(ui->{n}, &QTableView::clicked, this, [this](const QModelIndex &i) {{ {h}(i, Qt::LeftButton); }});')
            connects.append(f'    connect(ui->{n}, &QTableView::customContextMenuRequested, this, [this](const QPoint &p) {{ {h}(ui->{n}->indexAt(p), Qt::RightButton); }});')
            slot('const QModelIndex &index, Qt::MouseButton button', h, c)
        elif ev == 'CellMouseDoubleClick':
            connects.append(f'    connect(ui->{n}, &QTableView::doubleClicked, this, &MainWindow::{h});')
            slot('const QModelIndex &index', h, c)
        elif ev == 'RowHeaderMouseClick':
            connects.append(f'    connect(ui->{n}->verticalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::{h});')
            slot('int row', h, c)
        elif ev == 'RowHeaderMouseDoubleClick':
            connects.append(f'    connect(ui->{n}->verticalHeader(), &QHeaderView::sectionDoubleClicked, this, &MainWindow::{h});')
            slot('int row', h, c)
        elif ev == 'SelectionChanged':
            connects.append(f'    connect(ui->{n}->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {{ {h}(); }});')
            slot('', h, c)
    elif t == 'Timer' and ev == 'Tick':
        p = props.get(n, {})
        timers.append((n, p.get('Interval', '100'), p.get('Enabled') == 'true', h))
        connects.append(f'    connect({n}, &QTimer::timeout, this, &MainWindow::{h});')
        slot('', h, c)
    elif ev == 'KeyPress' or (ev == 'MouseDown') or (t == 'Label' and ev in ('Click', 'DoubleClick')) or (t == 'ComboBox' and ev == 'MouseDoubleClick'):
        target = f'ui->{n}->viewport()' if (t == 'ListBox' and ev == 'MouseDown') else f'ui->{n}'
        filter_objs.add(target)
        if ev == 'KeyPress':
            objtest = f'(obj == ui->{n} || obj == ui->{n}->lineEdit())' if t == 'ComboBox' else f'obj == ui->{n}'
            filter_cases.append(f'    if ({objtest} && e->type() == QEvent::KeyPress)\n        return {h}(static_cast<QKeyEvent *>(e));')
            slot('QKeyEvent *e', h, c + '  (return true to swallow the key, like e.Handled = true)')
            slots_decl[-1] = slots_decl[-1].replace('void ', 'bool ') if slots_decl[-1].endswith(f'{h}(QKeyEvent *e);') else slots_decl[-1]
            stubs[-1] = stubs[-1].replace(f'void MainWindow::{h}', f'bool MainWindow::{h}').replace('// TODO: port from Form1.cs\n', '// TODO: port from Form1.cs\n    return false;\n')
        elif ev == 'MouseDown':
            filter_cases.append(f'    if (obj == {target} && e->type() == QEvent::MouseButtonPress)\n        {h}(static_cast<QMouseEvent *>(e));')
            slot('QMouseEvent *e', h, c)
        elif ev == 'DoubleClick' or ev == 'MouseDoubleClick':
            filter_cases.append(f'    if (obj == ui->{n} && e->type() == QEvent::MouseButtonDblClick)\n        {h}();')
            slot('', h, c)
        else:
            filter_cases.append(f'    if (obj == ui->{n} && e->type() == QEvent::MouseButtonRelease)\n        {h}();')
            slot('', h, c)
    elif n == 'this':
        if ev == 'Load':
            notes.append(f'    {h}();   // Form.Load')
            slot('', h, c)
        elif ev == 'Shown':
            slot('', h, c + '  (called once from showEvent)')
        elif ev == 'FormClosing':
            slot('QCloseEvent *e', h, c + '  (e->ignore() == e.Cancel = true)')
        elif ev == 'KeyDown':
            slot('QKeyEvent *e', h, c + '  (KeyPreview: sees keys before child widgets; return true to swallow)')
            slots_decl[-1] = slots_decl[-1].replace('void ', 'bool ')
            stubs[-1] = stubs[-1].replace(f'void MainWindow::{h}', f'bool MainWindow::{h}').replace('// TODO: port from Form1.cs\n', '// TODO: port from Form1.cs\n    return false;\n')
    else:
        unmapped.append((n, f'{t}.{ev} -> {h} (not wired; usually an empty designer stub - check Form1.cs)'))

form_ev = {ev: h for n, ev, h in events if n == 'this'}

timer_decl = '\n'.join(f'    QTimer *{n} = nullptr;' for n, *_ in timers)
timer_init = '\n'.join(
    f'    {n} = new QTimer(this);\n    {n}->setInterval({iv});' + (f'\n    {n}->start();' if en else '') for n, iv, en, h in timers)

hdr = f'''#pragma once
// Generated from Form1.Designer.cs - control and handler names kept from C#.
#include <QMainWindow>
#include <QModelIndex>
#include <QDate>
#include <QTime>

QT_BEGIN_NAMESPACE
namespace Ui {{ class MainWindow; }}
QT_END_NAMESPACE
class QTimer;
class QStandardItemModel;
class QListWidgetItem;
class QKeyEvent;
class QMouseEvent;

class MainWindow : public QMainWindow
{{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private slots:
{chr(10).join(slots_decl)}

private:
    void setupSlotGrid();

    Ui::MainWindow *ui;
    QStandardItemModel *slotModel = nullptr;
    bool m_shownOnce = false;
{timer_decl}
}};
'''

colinit = '\n'.join(f'        {{"{t}", {w}}},' for _, t, w in columns)
filters = '\n'.join(f'    {o}->installEventFilter(this);' for o in sorted(filter_objs))
if any(n == 'this' and ev == 'KeyDown' for n, ev, h in events):
    filters = '    // (no per-widget filters needed: the application-wide filter below sees every widget\'s events)'
hides = '\n'.join(f'    ui->{n}->hide();' for n in hidden)
autosz = [n for n, t in types.items() if t in ('Label', 'CheckBox') and props.get(n, {}).get('AutoSize') == 'true' and n in emitted and n not in formlevel]
autosize_code = '    // WinForms AutoSize = true: grow to fit text (Qt fonts measure differently)\n    for (QWidget *w : {' + ', '.join(f'static_cast<QWidget *>(ui->{n})' for n in autosz) + '})\n        w->adjustSize();' if autosz else ''
kd = form_ev.get('KeyDown')
kp = form_ev.get('KeyPress')
if kp and not kd:
    kd = kp
elif kp:
    filter_cases.insert(0, f'    if (e->type() == QEvent::KeyPress && obj == QApplication::focusWidget() && isAncestorOf(static_cast<QWidget *>(obj))\n        && {kp}(static_cast<QKeyEvent *>(e)))\n        return true;')
cpp = f'''// Generated from Form1.Designer.cs - control and handler names kept from C#.
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QStandardItemModel>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{{
    ui->setupUi(this);
{"    // Form-level labels (clock etc.) sat beside the tab headers in WinForms" + chr(10) + "    ui->" + tabctl + "->setCornerWidget(ui->cornerWidget, Qt::TopRightCorner);" if formlevel else ""}
    setupSlotGrid();

{autosize_code}

    // Controls that were Visible = false in the designer
{hides}

    // Timers (System.Windows.Forms.Timer -> QTimer)
{timer_init}

    // Events that need an event filter (Label clicks, KeyPress, MouseDown)
{filters}
{"    qApp->installEventFilter(this);   // emulates Form.KeyPreview for " + kd if kd else ""}

    // Event wiring (C# += handlers)
{chr(10).join(connects)}

{chr(10).join(notes)}
}}

MainWindow::~MainWindow()
{{
{"    qApp->removeEventFilter(this);   // before ui is deleted" if kd else ""}
    delete ui;
}}

void MainWindow::setupSlotGrid()
{{
    // DataGridView columns -> model headers. Replace with a custom QAbstractTableModel later.
    const QList<QPair<QString, int>> cols = {{
{colinit}
    }};
    slotModel = new QStandardItemModel(0, cols.size(), this);
    for (int i = 0; i < cols.size(); ++i)
        slotModel->setHeaderData(i, Qt::Horizontal, cols[i].first);
    ui->dataGridView1->setModel(slotModel);
    for (int i = 0; i < cols.size(); ++i)
        ui->dataGridView1->setColumnWidth(i, cols[i].second);
    ui->dataGridView1->verticalHeader()->setDefaultSectionSize(20);
}}

bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{{
{"    // KeyPreview: only the first delivery (to the focus widget), and only for this window" + chr(10) + "    if (e->type() == QEvent::KeyPress && obj == QApplication::focusWidget() && isAncestorOf(static_cast<QWidget *>(obj))" + chr(10) + "        && " + kd + "(static_cast<QKeyEvent *>(e)))" + chr(10) + "        return true;" if kd else ""}
{chr(10).join(filter_cases)}
    return QMainWindow::eventFilter(obj, e);
}}

void MainWindow::showEvent(QShowEvent *e)
{{
    QMainWindow::showEvent(e);
    if (!m_shownOnce) {{
        m_shownOnce = true;
{"        " + form_ev["Shown"] + "();" if "Shown" in form_ev else ""}
    }}
}}

void MainWindow::closeEvent(QCloseEvent *e)
{{
{"    " + form_ev["FormClosing"] + "(e);" + chr(10) + "    if (!e->isAccepted())" + chr(10) + "        return;" if "FormClosing" in form_ev else ""}
    QMainWindow::closeEvent(e);
}}

// ---------------------------------------------------------------------------
// Handler stubs - one per C# event handler
// ---------------------------------------------------------------------------

{chr(10).join(stubs)}'''

if '--ui-only' not in sys.argv:
    open(os.path.join(out, 'mainwindow.h'), 'w').write(hdr)
    open(os.path.join(out, 'mainwindow.cpp'), 'w').write(cpp)
with open(os.path.join(out, 'conversion_report.txt'), 'w') as r:
    r.write(f'controls: {sum(1 for t in types.values() if t in QT or t in ("DateTimePicker",))}\n')
    r.write(f'handlers: {len(seen_handlers)}\nhidden at start: {len(hidden)}\n\nNot converted / needs attention:\n')
    for n, t in unmapped:
        r.write(f'  {n}: {t}\n')
print(open(os.path.join(out, 'conversion_report.txt')).read())
