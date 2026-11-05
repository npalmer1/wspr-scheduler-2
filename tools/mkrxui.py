# Writes rxform.ui (layout version of RXForm.Designer.cs, C# control names kept)
import html, sys
out = sys.argv[1]

def prop(name, kind, val):
    if kind == 'string': return f'<property name="{name}"><string>{html.escape(val)}</string></property>'
    if kind == 'bool': return f'<property name="{name}"><bool>{"true" if val else "false"}</bool></property>'
    if kind == 'number': return f'<property name="{name}"><number>{val}</number></property>'
    if kind == 'enum': return f'<property name="{name}"><enum>{val}</enum></property>'
    if kind == 'raw': return f'<property name="{name}">{val}</property>'

def w(cls, name, *props, items=None):
    s = f'<widget class="{cls}" name="{name}">' + ''.join(props)
    for it in items or []:
        s += f'<item><property name="text"><string>{html.escape(it)}</string></property></item>'
    return s + '</widget>'

def label(name, text, tip=None, *extra):
    p = [prop('text', 'string', text)]
    if tip: p.append(prop('toolTip', 'string', tip))
    return w('QLabel', name, *p, *extra)

def check(name, text, tip=None):
    p = [prop('text', 'string', text)]
    if tip: p.append(prop('toolTip', 'string', tip))
    return w('QCheckBox', name, *p)

def button(name, text):
    return w('QPushButton', name, prop('text', 'string', text))

def cell(row, col, widget, rs=1, cs=1):
    a = f' rowspan="{rs}"' if rs > 1 else ''
    a += f' colspan="{cs}"' if cs > 1 else ''
    return f'<item row="{row}" column="{col}"{a}>{widget}</item>'

def spacer(name, horiz=True):
    o = 'Qt::Orientation::Horizontal' if horiz else 'Qt::Orientation::Vertical'
    return (f'<spacer name="{name}"><property name="orientation"><enum>{o}</enum></property>'
            '<property name="sizeHint" stdset="0"><size><width>20</width><height>10</height></size></property></spacer>')

bands = ["All", "LF", "MF", "160m", "80m", "60m", "40m", "30m", "22m", "20m", "17m", "15m", "12m", "10m", "8m", "6m", "4m", "2m", "70cm", "23cm"]
watts = ["5", "10", "15", "20", "30", "50", "70", "100", "200", "300", "500", "700", "1000", "1500", "2000"]
bold = '<property name="font"><font><bold>true</bold></font></property>'

top = ''.join([
    cell(0, 0, label('label1', 'Current RX MHz:')),
    cell(0, 1, label('RXFlabel', '-')),
    cell(0, 2, label('label10', 'My locator:')),
    cell(0, 3, label('myloclabel', '--')),
    cell(0, 4, w('QLabel', 'Timelabel', prop('text', 'string', 'Time'), bold)),
    cell(0, 5, spacer('topSpacer'), rs=3),
    cell(0, 6, check('DeepcheckBox', 'Deep search', 'wsprd -d: slower, a few more decodes')),
    cell(0, 7, label('label3', 'OSD depth:')),
    cell(0, 8, w('QComboBox', 'OSDlistBox', prop('toolTip', 'string', 'wsprd -o: ordered statistics decoder depth'), items=["Off", "1", "2", "3", "4", "5"])),
    cell(0, 9, button('button1', 'Show previous decodes'), cs=2),
    cell(1, 0, label('label9', 'Status:')),
    cell(1, 1, label('statuslabel', '--')),
    cell(1, 2, w('QLabel', 'RXofflabel', prop('text', 'string', 'RX Running'), prop('autoFillBackground', 'bool', True),
                 prop('styleSheet', 'string', 'background-color: peachpuff; padding: 1px 3px;'),
                 prop('sizePolicy', 'raw', '<sizepolicy hsizetype="Maximum" vsizetype="Preferred"><horstretch>0</horstretch><verstretch>0</verstretch></sizepolicy>')), cs=2),
    cell(1, 6, check('QuickcheckBox', 'Quick search', 'wsprd -q: faster, misses weak signals')),
    cell(1, 7, label('OSDlabel', 'OSD off'), cs=2),
    cell(1, 9, check('uploadcheckBox', 'Upload spots', 'Send decodes to wsprnet.org')),
    cell(1, 10, check('owncheckBox', 'allow upload \nown call'), rs=2),
    cell(2, 0, label('label11', 'Own call:')),
    cell(2, 1, label('ownlabel', '--')),
    cell(2, 7, w('QLabel', 'label14', prop('text', 'string', '(depends on no. spots)'),
                 '<property name="font"><font><pointsize>8</pointsize></font></property>'), cs=2),
])

dt = lambda n: w('QDateTimeEdit', n, prop('displayFormat', 'string', 'yyyy-MM-dd HH:mm'), prop('calendarPopup', 'bool', True),
                 prop('toolTip', 'string', 'UTC'))   # set to UTC in RxForm (setTimeSpec is deprecated in Qt 6.7+)
small = '<property name="font"><font><pointsize>8</pointsize></font></property>'
narrow = lambda n, tip: w('QLineEdit', n, prop('maximumSize', 'raw', '<size><width>70</width><height>16777215</height></size>'), prop('toolTip', 'string', tip))

bottom = ''.join([
    cell(0, 0, label('label4', 'Filters (interrogates local db):'), cs=2),
    cell(0, 2, label('label5', 'Band:')),
    cell(0, 3, w('QComboBox', 'bandlistBox', items=bands)),
    cell(0, 4, check('datecheckBox', 'Enable date filter'), cs=2),
    cell(0, 6, label('label7', 'From:')),
    cell(0, 7, dt('dateTimePicker1')),
    cell(0, 8, label('label8', 'to:')),
    cell(0, 9, dt('dateTimePicker2')),
    cell(0, 10, button('filterbutton', 'Apply')),
    cell(0, 11, w('QLabel', 'label12', prop('text', 'string', 'CW/SSB \npower (W)'), small)),
    cell(0, 12, w('QComboBox', 'cwssblistBox', items=watts)),
    cell(0, 13, spacer('bottomSpacer'), rs=3),
    cell(1, 0, check('stopRXcheckBox', "Stop receiving (don't interrupt search)"), cs=2),
    cell(1, 2, label('label2', 'Call filter:')),
    cell(1, 3, w('QLineEdit', 'callFiltertextBox', prop('maximumSize', 'raw', '<size><width>100</width><height>16777215</height></size>'),
                 prop('toolTip', 'string', 'Calls starting with this text'))),
    cell(1, 4, label('label6', 'Distance:')),
    cell(1, 5, '<layout class="QHBoxLayout" name="distanceLayout">'
             + '<item>' + narrow('DFromtextBox', 'From (blank = no lower limit)') + '</item>'
             + '<item>' + w('QLabel', 'Dlabel', prop('text', 'string', '0 - 12,453'), small) + '</item>'
             + '<item>' + narrow('DTotextBox', 'To (blank = no upper limit)') + '</item>'
             + '<item>' + label('Ulabel', 'mls') + '</item>'
             + '<item>' + check('kmcheckBox', 'Use km') + '</item>'
             + '<item>' + spacer('distanceSpacer') + '</item></layout>', cs=5),
    cell(1, 10, button('Clearbutton', 'Clear')),
    cell(1, 11, w('QLabel', 'label13', prop('text', 'string', 'for comparison'), small), cs=2),
    cell(2, 0, check('allowDeletecheckBox', 'Allow row deletion', 'Then right-click a row number to delete it'), cs=2),
    cell(2, 11, button('Spectrumbutton', 'Show spectrum'), cs=2),
])

table = w('QTableView', 'dataGridView1', prop('selectionBehavior', 'enum', 'QAbstractItemView::SelectionBehavior::SelectRows'),
          prop('selectionMode', 'enum', 'QAbstractItemView::SelectionMode::SingleSelection'),
          prop('editTriggers', 'raw', '<set>QAbstractItemView::EditTrigger::NoEditTriggers</set>'),
          prop('sortingEnabled', 'bool', True), prop('alternatingRowColors', 'bool', True))

ui = f'''<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated from RXForm.Designer.cs (C# control names kept); laid out with layouts so the grid grows with the window -->
<ui version="4.0">
 <class>RXForm</class>
 <widget class="QWidget" name="RXForm">
  <property name="geometry"><rect><x>0</x><y>0</y><width>1117</width><height>620</height></rect></property>
  <property name="windowTitle"><string>Calls received by this station</string></property>
  <layout class="QVBoxLayout" name="mainLayout">
   <item><layout class="QGridLayout" name="topLayout"><property name="horizontalSpacing"><number>8</number></property>{top}</layout></item>
   <item>{table}</item>
   <item><layout class="QGridLayout" name="bottomLayout"><property name="horizontalSpacing"><number>6</number></property>{bottom}</layout></item>
  </layout>
 </widget>
 <resources/>
 <connections/>
</ui>
'''
open(out, 'w').write(ui)
