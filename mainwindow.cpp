// Generated from Form1.Designer.cs - control and handler names kept from C#.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "rigctl.h"
#include "rigctld.h"
#include "rxform.h"
#include "wsprmessage.h"
#include "msg.h"
#include <QSignalBlocker>
#include <QSettings>
#include <QRegularExpression>
#include "credentials.h"

#include <QApplication>
#include <QCloseEvent>
#include <QBoxLayout>
#include <QStatusBar>
#include <QStyle>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QFrame>
#include <QPushButton>
#include <QTabBar>
#include <QTextCharFormat>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCalendarWidget>
#include <climits>
#include <QHeaderView>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QStandardItemModel>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // always open on the Slots tab: Qt Designer saves whichever tab was showing when the .ui was saved
    ui->tabControl1->setCurrentIndex(0);
    setupClockBar();          // clock on the tab row, as the C# form
    setupCalendars();         // two months side by side + "Today", as the C# MonthCalendar
    makeRoomForCalendar();    // taller window if the calendar's font needs more height
    setupSlotGrid();

    // WinForms AutoSize = true: grow to fit text (Qt fonts measure differently)
    for (QWidget *w : {static_cast<QWidget *>(ui->rangelabel), static_cast<QWidget *>(ui->slotlabel), static_cast<QWidget *>(ui->AllcheckBox), static_cast<QWidget *>(ui->NightcheckBox), static_cast<QWidget *>(ui->DaycheckBox), static_cast<QWidget *>(ui->label110), static_cast<QWidget *>(ui->label109), static_cast<QWidget *>(ui->editslotcheckBox), static_cast<QWidget *>(ui->msgTlabel), static_cast<QWidget *>(ui->repeatTimecheckBox), static_cast<QWidget *>(ui->label70), static_cast<QWidget *>(ui->repeatcheckBox), static_cast<QWidget *>(ui->label10), static_cast<QWidget *>(ui->datetimelabel), static_cast<QWidget *>(ui->label9), static_cast<QWidget *>(ui->label8), static_cast<QWidget *>(ui->label7), static_cast<QWidget *>(ui->label6), static_cast<QWidget *>(ui->label5), static_cast<QWidget *>(ui->label4), static_cast<QWidget *>(ui->label3), static_cast<QWidget *>(ui->label2), static_cast<QWidget *>(ui->label1), static_cast<QWidget *>(ui->ActivecheckBox), static_cast<QWidget *>(ui->label11), static_cast<QWidget *>(ui->label12), static_cast<QWidget *>(ui->label13), static_cast<QWidget *>(ui->label146), static_cast<QWidget *>(ui->label144), static_cast<QWidget *>(ui->label121), static_cast<QWidget *>(ui->label119), static_cast<QWidget *>(ui->saveslotlabel), static_cast<QWidget *>(ui->skedstoplabel), static_cast<QWidget *>(ui->overlabel), static_cast<QWidget *>(ui->dblabel), static_cast<QWidget *>(ui->label117), static_cast<QWidget *>(ui->localsetlabel), static_cast<QWidget *>(ui->localriselabel), static_cast<QWidget *>(ui->label115), static_cast<QWidget *>(ui->utcsetlabel), static_cast<QWidget *>(ui->utcriselabel), static_cast<QWidget *>(ui->label114), static_cast<QWidget *>(ui->label113), static_cast<QWidget *>(ui->Savelabel), static_cast<QWidget *>(ui->Fhelplabel), static_cast<QWidget *>(ui->Flabel), static_cast<QWidget *>(ui->stopInternetcheckBox), static_cast<QWidget *>(ui->label96), static_cast<QWidget *>(ui->label95), static_cast<QWidget *>(ui->slotnolabel), static_cast<QWidget *>(ui->mtypelabel), static_cast<QWidget *>(ui->trackSlotscheckBox), static_cast<QWidget *>(ui->label84), static_cast<QWidget *>(ui->label83), static_cast<QWidget *>(ui->selectedEndDatelabel), static_cast<QWidget *>(ui->selectedStartDatelabel), static_cast<QWidget *>(ui->label79), static_cast<QWidget *>(ui->label78), static_cast<QWidget *>(ui->UTClabel2), static_cast<QWidget *>(ui->LTcheckBox), static_cast<QWidget *>(ui->msgTypelabel), static_cast<QWidget *>(ui->label77), static_cast<QWidget *>(ui->label76), static_cast<QWidget *>(ui->label75), static_cast<QWidget *>(ui->label74), static_cast<QWidget *>(ui->label71), static_cast<QWidget *>(ui->Riglabel1), static_cast<QWidget *>(ui->label22), static_cast<QWidget *>(ui->countdownlabel2), static_cast<QWidget *>(ui->sunlabel), static_cast<QWidget *>(ui->label73), static_cast<QWidget *>(ui->overcheckBox), static_cast<QWidget *>(ui->stopRXcheckBox), static_cast<QWidget *>(ui->label116), static_cast<QWidget *>(ui->asOnecheckBox), static_cast<QWidget *>(ui->label106), static_cast<QWidget *>(ui->longcheckBox), static_cast<QWidget *>(ui->label94), static_cast<QWidget *>(ui->testdbmlabel), static_cast<QWidget *>(ui->label93), static_cast<QWidget *>(ui->label92), static_cast<QWidget *>(ui->label91), static_cast<QWidget *>(ui->label89), static_cast<QWidget *>(ui->label82), static_cast<QWidget *>(ui->label87), static_cast<QWidget *>(ui->noSkedcheckBox), static_cast<QWidget *>(ui->label88), static_cast<QWidget *>(ui->label86), static_cast<QWidget *>(ui->label85), static_cast<QWidget *>(ui->wsprmsglabel), static_cast<QWidget *>(ui->Type2checkBox), static_cast<QWidget *>(ui->Passlabel2), static_cast<QWidget *>(ui->showPasscheckBox), static_cast<QWidget *>(ui->Usernamelabel), static_cast<QWidget *>(ui->ardReplylabel), static_cast<QWidget *>(ui->baselabel), static_cast<QWidget *>(ui->msgTypelabel2), static_cast<QWidget *>(ui->label59), static_cast<QWidget *>(ui->label58), static_cast<QWidget *>(ui->HiLolabel), static_cast<QWidget *>(ui->LEDcheckBox), static_cast<QWidget *>(ui->Riglabel), static_cast<QWidget *>(ui->label39), static_cast<QWidget *>(ui->label38), static_cast<QWidget *>(ui->label37), static_cast<QWidget *>(ui->label35), static_cast<QWidget *>(ui->enableTXcheckBox), static_cast<QWidget *>(ui->label27), static_cast<QWidget *>(ui->testTonescheckBox), static_cast<QWidget *>(ui->label26), static_cast<QWidget *>(ui->label25), static_cast<QWidget *>(ui->label24), static_cast<QWidget *>(ui->label23), static_cast<QWidget *>(ui->countdownlabel), static_cast<QWidget *>(ui->label21), static_cast<QWidget *>(ui->label20), static_cast<QWidget *>(ui->label19), static_cast<QWidget *>(ui->label18), static_cast<QWidget *>(ui->label17), static_cast<QWidget *>(ui->label16), static_cast<QWidget *>(ui->label15), static_cast<QWidget *>(ui->label14), static_cast<QWidget *>(ui->label28), static_cast<QWidget *>(ui->label32), static_cast<QWidget *>(ui->VoxcheckBox), static_cast<QWidget *>(ui->label112), static_cast<QWidget *>(ui->label111), static_cast<QWidget *>(ui->rigctldcheckBox), static_cast<QWidget *>(ui->label72), static_cast<QWidget *>(ui->rigrunlabel), static_cast<QWidget *>(ui->label36), static_cast<QWidget *>(ui->label34), static_cast<QWidget *>(ui->label31), static_cast<QWidget *>(ui->label30), static_cast<QWidget *>(ui->label29), static_cast<QWidget *>(ui->label120), static_cast<QWidget *>(ui->label69), static_cast<QWidget *>(ui->label68), static_cast<QWidget *>(ui->label63), static_cast<QWidget *>(ui->AntPortlabel2), static_cast<QWidget *>(ui->label118), static_cast<QWidget *>(ui->label90), static_cast<QWidget *>(ui->antNolabel), static_cast<QWidget *>(ui->AntPortlabel), static_cast<QWidget *>(ui->newAcheckBox), static_cast<QWidget *>(ui->label51), static_cast<QWidget *>(ui->label50), static_cast<QWidget *>(ui->label49), static_cast<QWidget *>(ui->label48), static_cast<QWidget *>(ui->label55), static_cast<QWidget *>(ui->RXonlylabel), static_cast<QWidget *>(ui->label81), static_cast<QWidget *>(ui->label80), static_cast<QWidget *>(ui->newFcheckBox), static_cast<QWidget *>(ui->Levellabel), static_cast<QWidget *>(ui->label45), static_cast<QWidget *>(ui->label44), static_cast<QWidget *>(ui->label43), static_cast<QWidget *>(ui->standardFcheckBox), static_cast<QWidget *>(ui->label47), static_cast<QWidget *>(ui->label46), static_cast<QWidget *>(ui->label41), static_cast<QWidget *>(ui->label40), static_cast<QWidget *>(ui->label141), static_cast<QWidget *>(ui->label140), static_cast<QWidget *>(ui->label139), static_cast<QWidget *>(ui->label131), static_cast<QWidget *>(ui->label130), static_cast<QWidget *>(ui->label54), static_cast<QWidget *>(ui->label52), static_cast<QWidget *>(ui->label138), static_cast<QWidget *>(ui->label57), static_cast<QWidget *>(ui->label56), static_cast<QWidget *>(ui->label53), static_cast<QWidget *>(ui->label66), static_cast<QWidget *>(ui->label67), static_cast<QWidget *>(ui->label65), static_cast<QWidget *>(ui->label64), static_cast<QWidget *>(ui->BPlabel), static_cast<QWidget *>(ui->HwPortlabel), static_cast<QWidget *>(ui->label62), static_cast<QWidget *>(ui->label61), static_cast<QWidget *>(ui->Hwnolabel), static_cast<QWidget *>(ui->label60), static_cast<QWidget *>(ui->label42), static_cast<QWidget *>(ui->label102), static_cast<QWidget *>(ui->label107), static_cast<QWidget *>(ui->gainlabel), static_cast<QWidget *>(ui->label108), static_cast<QWidget *>(ui->label103), static_cast<QWidget *>(ui->label104), static_cast<QWidget *>(ui->label143), static_cast<QWidget *>(ui->label142), static_cast<QWidget *>(ui->samecheckBox), static_cast<QWidget *>(ui->label105), static_cast<QWidget *>(ui->label101), static_cast<QWidget *>(ui->label100), static_cast<QWidget *>(ui->label99), static_cast<QWidget *>(ui->AudioInlabel), static_cast<QWidget *>(ui->AudioOutlabel), static_cast<QWidget *>(ui->label98), static_cast<QWidget *>(ui->label97), static_cast<QWidget *>(ui->label33), static_cast<QWidget *>(ui->label137), static_cast<QWidget *>(ui->httpscheckBox), static_cast<QWidget *>(ui->APIstartlabel), static_cast<QWidget *>(ui->apiinfolabel), static_cast<QWidget *>(ui->label134), static_cast<QWidget *>(ui->rigeditcheckBox), static_cast<QWidget *>(ui->label136), static_cast<QWidget *>(ui->label125), static_cast<QWidget *>(ui->label124), static_cast<QWidget *>(ui->label123), static_cast<QWidget *>(ui->label126), static_cast<QWidget *>(ui->label127), static_cast<QWidget *>(ui->label128), static_cast<QWidget *>(ui->label129), static_cast<QWidget *>(ui->rigBPlabel), static_cast<QWidget *>(ui->rigportlabel), static_cast<QWidget *>(ui->label132), static_cast<QWidget *>(ui->label133), static_cast<QWidget *>(ui->rignolabel), static_cast<QWidget *>(ui->label135), static_cast<QWidget *>(ui->label122), static_cast<QWidget *>(ui->label149), static_cast<QWidget *>(ui->remoteAllowTxCheckBox), static_cast<QWidget *>(ui->label148), static_cast<QWidget *>(ui->label147), static_cast<QWidget *>(ui->showcodecheckBox), static_cast<QWidget *>(ui->remoteTitle), static_cast<QWidget *>(ui->remoteEnabledCheckBox), static_cast<QWidget *>(ui->remotePortLabel), static_cast<QWidget *>(ui->remoteUserLabel), static_cast<QWidget *>(ui->remotePasswordLabel), static_cast<QWidget *>(ui->remotePasscodeLabel), static_cast<QWidget *>(ui->remoteNoteLabel)})
        w->adjustSize();

    // Controls that were Visible = false in the designer
    ui->slotgroupBox->hide();
    ui->timeEnd->hide();
    ui->repeatTimecheckBox->hide();
    ui->editslotcheckBox->hide();
    ui->DaycheckBox->hide();
    ui->NightcheckBox->hide();
    ui->greygroupBox->hide();
    ui->AllcheckBox->hide();
    ui->slotlabel->hide();
    ui->rangelabel->hide();
    ui->msgTypelabel->hide();
    ui->showRXbutton->hide();
    ui->FlistBox2->hide();
    ui->Flabel->hide();
    ui->Fhelplabel->hide();
    ui->Savelabel->hide();
    ui->saveslotlabel->hide();
    ui->LEDcheckBox->hide();
    ui->button3->hide();
    ui->PasstextBox2->hide();
    ui->Passlabel2->hide();
    ui->Passlabel2->hide();
    ui->wsprmsgbutton->hide();
    ui->wsprmsgtextBox->hide();
    ui->wsprmsglabel->hide();
    ui->asOnecheckBox->hide();
    ui->solarcheckBox->hide();
    ui->locationbutton->hide();
    ui->standardFcheckBox->hide();
    ui->PopulateFbutton->hide();
    ui->FreqgroupBox->hide();
    ui->newFcheckBox->hide();
    ui->FstandardtlistBox->hide();
    ui->AntgroupBox->hide();
    ui->newAcheckBox->hide();
    ui->AntPortlistBox->hide();
    ui->AntPortlabel->hide();
    ui->antNolabel->hide();
    ui->AntPortlistBox2->hide();
    ui->AntPortlabel2->hide();
    ui->HwgroupBox->hide();
    ui->HwPortlabel->hide();
    ui->HwPortcomboBox->hide();
    ui->HwIPtextBox->hide();
    ui->BPlabel->hide();
    ui->HwBaudcomboBox->hide();
    ui->HwPorttextBox->hide();
    ui->HwSerialgroupBox->hide();
    ui->panel1->hide();
    ui->panel2->hide();
    ui->RiggroupBox->hide();
    ui->rigportlabel->hide();
    ui->rigportcomboBox->hide();
    ui->rigiptextBox->hide();
    ui->rigBPlabel->hide();
    ui->rigbaudcomboBox->hide();
    ui->rigporttextBox->hide();
    ui->rigserialgroupBox->hide();
    ui->rigeditcheckBox->hide();

    // Timers (System.Windows.Forms.Timer -> QTimer)
    WSPRtimer = new QTimer(this);
    WSPRtimer->setInterval(1000);
    daytimer = new QTimer(this);
    daytimer->setInterval(1000);
    daytimer->start();
    testPTTtimer = new QTimer(this);
    testPTTtimer->setInterval(30000);
    wsprTXtimer = new QTimer(this);
    wsprTXtimer->setInterval(112000);
    daytimer2 = new QTimer(this);
    daytimer2->setInterval(1000);
    RXblocktimer = new QTimer(this);
    RXblocktimer->setInterval(30000);
    idletimer = new QTimer(this);
    idletimer->setInterval(1000);

    // Events that need an event filter (Label clicks, KeyPress, MouseDown)
    // (no per-widget filters needed: the application-wide filter below sees every widget's events)
    qApp->installEventFilter(this);   // emulates Form.KeyPreview for Form1_KeyDown_1

    // Event wiring (C# += handlers)
    connect(ui->monthCalendar1, &QCalendarWidget::selectionChanged, this, &MainWindow::monthCalendar1_DateChanged);
    connect(ui->monthCalendar1, &QCalendarWidget::clicked, this, &MainWindow::monthCalendar1_DateSelected);
    connect(ui->timelistBox, &QListWidget::currentRowChanged, this, &MainWindow::timelistBox_SelectedIndexChanged);
    connect(ui->dataGridView1, &QTableView::clicked, this, [this](const QModelIndex &i) { dataGridView1_CellMouseClick(i, Qt::LeftButton); });
    connect(ui->dataGridView1, &QTableView::customContextMenuRequested, this, [this](const QPoint &p) { dataGridView1_CellMouseClick(ui->dataGridView1->indexAt(p), Qt::RightButton); });
    connect(ui->dataGridView1, &QTableView::doubleClicked, this, &MainWindow::dataGridView1_CellMouseDoubleClick);
    connect(ui->dataGridView1->verticalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::dataGridView1_RowHeaderMouseClick);
    connect(ui->dataGridView1->verticalHeader(), &QHeaderView::sectionDoubleClicked, this, &MainWindow::dataGridView1_RowHeaderMouseDoubleClick);
    connect(ui->dataGridView1->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { dataGridView1_SelectionChanged(); });
    connect(ui->AllcheckBox, &QCheckBox::toggled, this, &MainWindow::AllcheckBox_CheckedChanged);
    connect(ui->NightcheckBox, &QCheckBox::toggled, this, &MainWindow::NightcheckBox_CheckedChanged);
    connect(ui->DaycheckBox, &QCheckBox::toggled, this, &MainWindow::DaycheckBox_CheckedChanged);
    connect(ui->repeatTimecheckBox, &QCheckBox::toggled, this, &MainWindow::repeatTimecheckBox_CheckedChanged);
    connect(ui->AntselcomboBox, &QComboBox::currentIndexChanged, this, &MainWindow::AntselcomboBox_SelectedIndexChanged);
    connect(ui->timeEnd, &QTimeEdit::timeChanged, this, &MainWindow::timeEnd_ValueChanged);
    connect(ui->repeatcheckBox, &QCheckBox::toggled, this, &MainWindow::repeatcheckBox_CheckedChanged);
    connect(ui->dateEnd, &QDateEdit::dateChanged, this, &MainWindow::dateEnd_ValueChanged);
    connect(ui->dBmcomboBox, &QComboBox::currentIndexChanged, this, &MainWindow::dBmcomboBox_SelectedIndexChanged);
    connect(ui->CancelSlotbutton, &QPushButton::clicked, this, &MainWindow::CancelSlotbutton_Click);
    connect(ui->SaveSlotbutton, &QPushButton::clicked, this, &MainWindow::SaveSlotbutton_Click);
    connect(ui->ActivecheckBox, &QCheckBox::toggled, this, &MainWindow::ActivecheckBox_CheckedChanged);
    connect(ui->OffsettextBox, &QLineEdit::textChanged, this, &MainWindow::OffsettextBox_TextChanged);
    connect(ui->tabControl1, &QTabWidget::currentChanged, this, &MainWindow::tabControl1_SelectedIndexChanged);
    connect(ui->syncbutton, &QPushButton::clicked, this, &MainWindow::syncbutton_Click);
    connect(ui->dbbutton, &QPushButton::clicked, this, &MainWindow::dbbutton_Click);
    connect(ui->flsuhSlotbutton, &QPushButton::clicked, this, &MainWindow::flsuhSlotbutton_Click);
    connect(ui->FlistBox2, &QListWidget::currentRowChanged, this, &MainWindow::FlistBox2_SelectedIndexChanged);
    connect(ui->stopInternetcheckBox, &QCheckBox::toggled, this, &MainWindow::stopInternetcheckBox_CheckedChanged);
    connect(ui->deleteAllSlotsbutton, &QPushButton::clicked, this, &MainWindow::deleteAllSlotsbutton_Click);
    connect(ui->currHourbutton, &QPushButton::clicked, this, &MainWindow::currHourbutton_Click);
    connect(ui->LTcheckBox, &QCheckBox::toggled, this, &MainWindow::LTcheckBox_CheckedChanged);
    connect(ui->TXrunbutton, &QPushButton::clicked, this, &MainWindow::TXrunbutton_Click);
    connect(ui->TXrunbutton2, &QPushButton::clicked, this, &MainWindow::TXrunbutton_Click);
    connect(ui->TXenablebutton, &QPushButton::clicked, this, &MainWindow::TXenablebutton_Click);
    connect(ui->logclearbutton, &QPushButton::clicked, this, &MainWindow::logclearbutton_Click);
    connect(ui->stopRXcheckBox, &QCheckBox::toggled, this, &MainWindow::stopRXcheckBox_CheckedChanged);
    connect(ui->sunrisebutton, &QPushButton::clicked, this, &MainWindow::sunrisebutton_Click);
    connect(ui->locationbutton, &QPushButton::clicked, this, &MainWindow::locationbutton_Click);
    connect(ui->noSkedcheckBox, &QCheckBox::toggled, this, &MainWindow::noSkedcheckBox_CheckedChanged);
    connect(ui->wsprmsgbutton, &QPushButton::clicked, this, &MainWindow::wsprmsgbutton_Click);
    connect(ui->Type2checkBox, &QCheckBox::toggled, this, &MainWindow::Type2checkBox_CheckedChanged);
    connect(ui->savePasswordbutton, &QPushButton::clicked, this, &MainWindow::savePasswordbutton_Click);
    connect(ui->showPasscheckBox, &QCheckBox::toggled, this, &MainWindow::showPasscheckBox_CheckedChanged);
    connect(ui->PasstextBox, &QLineEdit::textChanged, this, &MainWindow::PasstextBox_TextChanged);
    connect(ui->button3, &QPushButton::clicked, this, &MainWindow::button3_Click);
    connect(ui->testEncodebutton, &QPushButton::clicked, this, &MainWindow::testEncodebutton_Click);
    connect(ui->ArduinoPorttextBox, &QLineEdit::textChanged, this, &MainWindow::ArduinoPorttextBox_TextChanged);
    connect(ui->DefaultAntcomboBox, &QComboBox::currentIndexChanged, this, &MainWindow::DefaultAntcomboBox_SelectedIndexChanged);
    connect(ui->FlistBox, &QListWidget::currentRowChanged, this, &MainWindow::FlistBox_SelectedIndexChanged);
    connect(ui->FlistBox, &QListWidget::itemDoubleClicked, this, &MainWindow::FlistBox_MouseDoubleClick);
    connect(ui->button1, &QPushButton::clicked, this, &MainWindow::button1_Click);
    connect(ui->AlphacomboBox, &QComboBox::currentIndexChanged, this, &MainWindow::AlphacomboBox_SelectedValueChanged);
    connect(ui->AlphacomboBox, &QComboBox::currentTextChanged, this, &MainWindow::comboBox1_TextChanged);
    connect(ui->stopWAVbutton, &QPushButton::clicked, this, &MainWindow::stopWAVbutton_Click);
    connect(ui->enableTXcheckBox, &QCheckBox::toggled, this, &MainWindow::enableTXcheckBox_CheckedChanged);
    connect(ui->stopTestbutton, &QPushButton::clicked, this, &MainWindow::stopTestbutton_Click);
    connect(ui->TestTXbutton, &QPushButton::clicked, this, &MainWindow::TestTXbutton_Click);
    connect(ui->testFtextBox, &QLineEdit::textChanged, this, &MainWindow::testFtextBox_TextChanged);
    connect(ui->testButton, &QPushButton::clicked, this, &MainWindow::testButton_Click);
    connect(ui->defaultpwrcomboBox, &QComboBox::currentIndexChanged, this, &MainWindow::defaultpwrcomboBox_SelectedIndexChanged);
    connect(ui->saveConfigbutton, &QPushButton::clicked, this, &MainWindow::saveConfigbutton_Click);
    connect(ui->TXTestbutton, &QPushButton::clicked, this, &MainWindow::TXTestbutton_Click);
    connect(ui->comRbutton, &QPushButton::clicked, this, &MainWindow::comRbutton_Click);
    connect(ui->VoxcheckBox, &QCheckBox::toggled, this, &MainWindow::VoxcheckBox_CheckedChanged);
    connect(ui->rigsearchbutton, &QPushButton::clicked, this, &MainWindow::rigsearchbutton_Click);
    connect(ui->rigctldcheckBox, &QCheckBox::toggled, this, &MainWindow::rigctldcheckBox_CheckedChanged);
    connect(ui->SaveRPathbutton, &QPushButton::clicked, this, &MainWindow::SaveRPathbutton_Click);
    connect(ui->RigCtlPbutton, &QPushButton::clicked, this, &MainWindow::RigCtlPbutton_Click);
    connect(ui->stopRigCtlDbutton, &QPushButton::clicked, this, &MainWindow::stopRigCtlDbutton_Click);
    connect(ui->startRigCtlbutton, &QPushButton::clicked, this, &MainWindow::startRigCtlbutton_Click);
    connect(ui->SaveRigctlbutton, &QPushButton::clicked, this, &MainWindow::SaveRigctlbutton_Click);
    connect(ui->ShowSwlistBox2, &QListWidget::currentRowChanged, this, &MainWindow::ShowSwlistBox2_SelectedIndexChanged);
    connect(ui->ShowSwlistBox, &QListWidget::currentRowChanged, this, &MainWindow::ShowSwlistBox_SelectedIndexChanged);
    connect(ui->CancelAbutton, &QPushButton::clicked, this, &MainWindow::CancelAbutton_Click);
    connect(ui->SaveAbutton, &QPushButton::clicked, this, &MainWindow::SaveAbutton_Click);
    connect(ui->Fcancelbutton, &QPushButton::clicked, this, &MainWindow::Fcancelbutton_Click);
    connect(ui->Fsavebutton, &QPushButton::clicked, this, &MainWindow::Fsavebutton_Click);
    connect(ui->trackBar1, &QSlider::valueChanged, this, &MainWindow::trackBar1_ValueChanged);
    connect(ui->FtextBox, &QLineEdit::textChanged, this, &MainWindow::FtextBox_TextChanged);
    connect(ui->PopulateFbutton, &QPushButton::clicked, this, &MainWindow::PopulateFbutton_Click);
    connect(ui->editFbutton, &QPushButton::clicked, this, &MainWindow::editFbutton_Click);
    connect(ui->FreqlistBox, &QListWidget::itemDoubleClicked, this, &MainWindow::FreqlistBox_MouseDoubleClick);
    connect(ui->AddAbutton, &QPushButton::clicked, this, &MainWindow::AddAbutton_Click);
    connect(ui->DelAbutton, &QPushButton::clicked, this, &MainWindow::DelAbutton_Click);
    connect(ui->EditAbutton, &QPushButton::clicked, this, &MainWindow::EditAbutton_Click);
    connect(ui->AntlistBox, &QListWidget::itemDoubleClicked, this, &MainWindow::AntlistBox_MouseDoubleClick);
    connect(ui->editFibutton, &QPushButton::clicked, this, &MainWindow::editFibutton_Click);
    connect(ui->FilistBox, &QListWidget::itemClicked, this, &MainWindow::FilistBox_MouseClick);
    connect(ui->FilistBox, &QListWidget::itemDoubleClicked, this, &MainWindow::FilistBox_MouseDoubleClick);
    connect(ui->editTubutton, &QPushButton::clicked, this, &MainWindow::editTubutton_Click);
    connect(ui->editSwbutton, &QPushButton::clicked, this, &MainWindow::editSwbutton_Click);
    connect(ui->HwTuTypelistBox, &QListWidget::currentRowChanged, this, &MainWindow::listBox2_SelectedIndexChanged);
    connect(ui->HwSwTypelistBox, &QListWidget::currentRowChanged, this, &MainWindow::HwSwTypelistBox_SelectedIndexChanged);
    connect(ui->HwProtocolcomboBox, &QComboBox::currentIndexChanged, this, &MainWindow::HwProtocolcomboBox_SelectedIndexChanged);
    connect(ui->HwCancelbutton, &QPushButton::clicked, this, &MainWindow::HwCancelbutton_Click);
    connect(ui->HwSavebutton, &QPushButton::clicked, this, &MainWindow::HwSavebutton_Click);
    connect(ui->TulistBox, &QListWidget::itemDoubleClicked, this, &MainWindow::TulistBox_MouseDoubleClick);
    connect(ui->SwlistBox, &QListWidget::itemDoubleClicked, this, &MainWindow::SwlistBox_MouseDoubleClick);
    connect(ui->trackBarGain, &QSlider::valueChanged, this, &MainWindow::trackBarGain_ValueChanged);
    connect(ui->EditRXbutton, &QPushButton::clicked, this, &MainWindow::EditRXbutton_Click);
    connect(ui->samecheckBox, &QCheckBox::toggled, this, &MainWindow::samecheckBox_CheckedChanged);
    connect(ui->wsprdbutton, &QPushButton::clicked, this, &MainWindow::wsprdbutton_Click);
    connect(ui->SaveAudiobutton, &QPushButton::clicked, this, &MainWindow::SaveAudiobutton_Click);
    connect(ui->button5, &QPushButton::clicked, this, &MainWindow::button5_Click);
    connect(ui->Monitorbutton, &QPushButton::clicked, this, &MainWindow::Monitorbutton_Click);
    connect(ui->audioUpdatebutton, &QPushButton::clicked, this, &MainWindow::audioUpdatebutton_Click);
    connect(ui->audioOutlistBox, &QListWidget::currentRowChanged, this, &MainWindow::audioOutlistBox_SelectedIndexChanged);
    connect(ui->audioInlistBox, &QListWidget::currentRowChanged, this, &MainWindow::audioInlistBox_SelectedIndexChanged);
    connect(ui->editrigbutton, &QPushButton::clicked, this, &MainWindow::editrigbutton_Click);
    connect(ui->addrigbutton, &QPushButton::clicked, this, &MainWindow::addrigbutton_Click);
    connect(ui->riglistBox, &QListWidget::itemClicked, this, &MainWindow::riglistBox_MouseClick);
    connect(ui->riglistBox, &QListWidget::itemDoubleClicked, this, &MainWindow::riglistBox_MouseDoubleClick);
    connect(ui->httpscheckBox, &QCheckBox::toggled, this, &MainWindow::httpscheckBox_CheckedChanged);
    connect(ui->rigprotocolcomboBox, &QComboBox::currentIndexChanged, this, &MainWindow::rigprotocolcomboBox_SelectedIndexChanged);
    connect(ui->rigcancelbutton, &QPushButton::clicked, this, &MainWindow::rigcancelbutton_Click);
    connect(ui->saverigbutton, &QPushButton::clicked, this, &MainWindow::saverigbutton_Click);
    connect(ui->showcodecheckBox, &QCheckBox::toggled, this, &MainWindow::showcodecheckBox_CheckedChanged);
    connect(ui->remoteSaveButton, &QPushButton::clicked, this, &MainWindow::remoteSaveButton_Click);
    connect(WSPRtimer, &QTimer::timeout, this, &MainWindow::WSPRtimer_Tick);
    connect(daytimer, &QTimer::timeout, this, &MainWindow::daytimer_Tick);
    connect(testPTTtimer, &QTimer::timeout, this, &MainWindow::testPTTtimer_Tick);
    connect(wsprTXtimer, &QTimer::timeout, this, &MainWindow::wsprTXtimer_Tick);
    connect(daytimer2, &QTimer::timeout, this, &MainWindow::daytimer2_Tick);
    connect(RXblocktimer, &QTimer::timeout, this, &MainWindow::RXblocktimer_Tick);
    connect(idletimer, &QTimer::timeout, this, &MainWindow::idletimer_Tick);

    Form1_Load();   // Form.Load
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);   // before ui is deleted
    if (m_pttOn) {                   // never leave the rig keyed
        QString err;
        keyPtt(false, &err);
    }
    delete m_rx;                     // top-level window of its own (C#: no owner), so not a child
    delete m_help;                   // likewise
    delete ui;
}

// Clock on the tab row, right of the tabs, as in the C# form (Timelabel at x 608, Datelabel 734, local time
// 877, "F1 Help" at the right). Built in code by label name so Designer can't break it (it dropped the old
// "cornerWidget" holder); moved right if the tabs are wider than WinForms' were.
namespace {
// WinForms tabs are only as wide as their caption plus a few pixels; Qt's native tabs add a lot more.
// This keeps the native look and only narrows the tabs.
class CompactTabStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;
    int pixelMetric(PixelMetric m, const QStyleOption *opt = nullptr, const QWidget *w = nullptr) const override
    {
        if (m == PM_TabBarTabHSpace)
            return 12;   // space beside the caption (Qt's styles use about twice this)
        return QProxyStyle::pixelMetric(m, opt, w);
    }
};
} // namespace

// C#: the tab captions and the clock line are Microsoft Sans Serif 8.25 pt
void MainWindow::setupTabFont()
{
    const QFont ms("Microsoft Sans Serif", 8);
    QFont tabFont = ms;
    tabFont.setPointSizeF(8.25);
    QTabBar *bar = ui->tabControl1->tabBar();
    bar->setFont(tabFont);
    auto *style = new CompactTabStyle(QStyleFactory::create(QApplication::style()->name()));
    style->setParent(bar);
    bar->setStyle(style);
    for (const char *name : {"Timelabel", "UTClabel", "Datelabel", "LTlabel", "LTlabel2", "label145"})
        if (auto *l = findChild<QLabel *>(name)) {
            QFont f = tabFont;
            f.setBold(qstrcmp(name, "Timelabel") == 0);   // C#: Timelabel bold
            l->setFont(f);
        }
}

void MainWindow::setupClockBar()
{
    setupTabFont();
    for (const char *name : {"Timelabel", "UTClabel", "Datelabel", "LTlabel", "LTlabel2", "label145"})
        if (auto *l = findChild<QLabel *>(name)) {
            l->setParent(ui->tabControl1);
            l->show();
            l->raise();
        }
    if (auto *old = findChild<QWidget *>("cornerWidget"))
        old->hide();   // now empty
    ui->tabControl1->installEventFilter(this);   // re-place on resize
    positionClockBar();
}

void MainWindow::positionClockBar()
{
    auto *tabs = ui->tabControl1;
    const int rowH = tabs->tabBar()->height() > 0 ? tabs->tabBar()->height() : tabs->tabBar()->sizeHint().height();
    auto place = [&](QLabel *l, int x) {
        if (!l) return x;
        l->adjustSize();
        l->move(x, qMax(0, (rowH - l->height()) / 2));
        l->raise();
        return x + l->width();
    };
    auto label = [&](const char *n) { return tabs->findChild<QLabel *>(n, Qt::FindDirectChildrenOnly); };
    const int tabsEnd = tabs->tabBar()->sizeHint().width() + 12;
    const int x0 = qMax(608, tabsEnd);                        // C#: Timelabel x 608
    int x = place(label("Timelabel"), x0);
    place(label("UTClabel"), x + 4);
    place(label("Datelabel"), x0 + 126);                     // C#: 734
    x = place(label("LTlabel"), x0 + 269);                   // C#: local time at 877
    place(label("LTlabel2"), x + 4);
    if (auto *help = label("label145")) {
        help->adjustSize();
        place(help, tabs->width() - help->width() - 32);      // C#: 1109 in a 1185 wide form
    }
}

// C# showed two months side by side (CalendarDimensions 2 x 1) with "Today: dd/MM/yyyy" underneath.
// QCalendarWidget shows one, so a second one (the following month) sits beside monthCalendar1 under a
// shared header. monthCalendar1 stays the one the rest of the program reads and listens to.
void MainWindow::setupCalendars()
{
    auto *cal1 = ui->monthCalendar1;
    QWidget *page = cal1->parentWidget();
    const QPoint at = cal1->pos();

    m_calBox = new QFrame(page);
    m_calBox->setObjectName("calendarBox");
    m_calBox->setFrameShape(QFrame::Box);
    m_calBox->setStyleSheet("QFrame#calendarBox { background: white; border: 1px solid #a0a0a0; }"
                            "QLabel, QToolButton, QPushButton { background: white; border: none; }");
    auto *v = new QVBoxLayout(m_calBox);
    v->setContentsMargins(4, 2, 4, 2);
    v->setSpacing(0);

    auto *head = new QHBoxLayout;
    auto *prev = new QToolButton(m_calBox);
    prev->setText(QString::fromUtf8("\u25C2"));
    prev->setObjectName("calPrev");
    auto *next = new QToolButton(m_calBox);
    next->setText(QString::fromUtf8("\u25B8"));
    next->setObjectName("calNext");
    m_calMonth1 = new QLabel(m_calBox);
    m_calMonth2 = new QLabel(m_calBox);
    for (QLabel *l : {m_calMonth1, m_calMonth2}) {
        l->setAlignment(Qt::AlignCenter);
        l->setFont(cal1->font());
    }
    head->addWidget(prev);
    head->addWidget(m_calMonth1, 1);
    head->addWidget(m_calMonth2, 1);
    head->addWidget(next);
    v->addLayout(head);

    m_cal2 = new QCalendarWidget(m_calBox);
    m_cal2->setObjectName("monthCalendar2");
    auto *row = new QHBoxLayout;
    row->setSpacing(2);
    cal1->setParent(m_calBox);
    for (QCalendarWidget *c : {cal1, m_cal2}) {
        c->setFont(cal1->font());
        c->setNavigationBarVisible(false);
        c->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        c->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
        c->setFirstDayOfWeek(Qt::Monday);
        c->setGridVisible(false);
        c->setStyleSheet("QCalendarWidget QAbstractItemView { background: white;"
                         " selection-background-color: #c8c8c8; selection-color: black; }");
        QTextCharFormat plain;   // C#: weekends in the same black as other days
        plain.setForeground(Qt::black);
        c->setWeekdayTextFormat(Qt::Saturday, plain);
        c->setWeekdayTextFormat(Qt::Sunday, plain);
        c->setFixedSize(c->sizeHint());
        row->addWidget(c);
    }
    v->addLayout(row);

    auto *today = new QPushButton(m_calBox);
    today->setObjectName("calToday");
    today->setText("Today: " + QDate::currentDate().toString("dd/MM/yyyy"));
    today->setFont(cal1->font());
    today->setStyleSheet("text-align: left; padding-left: 4px;");
    today->setCursor(Qt::PointingHandCursor);
    v->addWidget(today, 0, Qt::AlignLeft);

    m_calBox->move(at);
    m_calBox->adjustSize();
    m_calBox->show();
    m_calBox->lower();   // labels to the right of it stay visible

    m_calLeft = QDate(cal1->selectedDate().year(), cal1->selectedDate().month(), 1);
    connect(prev, &QToolButton::clicked, this, [this] { m_calLeft = m_calLeft.addMonths(-1); syncCalendars(); });
    connect(next, &QToolButton::clicked, this, [this] { m_calLeft = m_calLeft.addMonths(1); syncCalendars(); });
    connect(today, &QPushButton::clicked, this, [this] { ui->monthCalendar1->setSelectedDate(QDate::currentDate()); });
    connect(cal1, &QCalendarWidget::selectionChanged, this, [this] { syncCalendars(); });
    // a day picked in the right-hand month is passed to monthCalendar1, as if it had been clicked there
    connect(m_cal2, &QCalendarWidget::clicked, this, [this](QDate d) {
        ui->monthCalendar1->setSelectedDate(d);
        monthCalendar1_DateSelected(d);
    });
    syncCalendars();
}

// Keep the two pages side by side and the selection showing in whichever month it is in
void MainWindow::syncCalendars()
{
    if (!m_cal2)
        return;
    const QDate sel = ui->monthCalendar1->selectedDate();
    const QDate selMonth(sel.year(), sel.month(), 1);
    if (sender() == ui->monthCalendar1 && selMonth != m_calLeft && selMonth != m_calLeft.addMonths(1))
        m_calLeft = selMonth;   // a date chosen elsewhere (e.g. "time now"): show its month on the left
    const QDate right = m_calLeft.addMonths(1);
    {
        const QSignalBlocker b1(ui->monthCalendar1), b2(m_cal2);
        ui->monthCalendar1->setCurrentPage(m_calLeft.year(), m_calLeft.month());
        m_cal2->setSelectedDate(sel);
        m_cal2->setCurrentPage(right.year(), right.month());
    }
    // days of the next / previous month in grey, as WinForms (keeps any range-selection background)
    for (QCalendarWidget *cal : {ui->monthCalendar1, m_cal2}) {
        const QDate first(cal->yearShown(), cal->monthShown(), 1);
        QDate d = first.addDays(-((first.dayOfWeek() - int(cal->firstDayOfWeek()) + 7) % 7));
        if (d == first)
            d = d.addDays(-7);   // a full row of the previous month is shown when the 1st is on a Monday
        for (int i = 0; i < 42; ++i, d = d.addDays(1)) {
            QTextCharFormat f = cal->dateTextFormat(d);
            f.setForeground(d.month() == first.month() ? QColor(Qt::black) : QColor(150, 150, 150));
            cal->setDateTextFormat(d, f);
        }
    }
    const QLocale c(QLocale::English);
    m_calMonth1->setText(c.toString(m_calLeft, "MMMM yyyy"));
    m_calMonth2->setText(c.toString(right, "MMMM yyyy"));
}

// The calendar's height follows its font (set in Designer). If it needs more room than the form gives it,
// move everything below it down and make the window that much taller.
void MainWindow::makeRoomForCalendar()
{
    QWidget *cal = m_calBox ? static_cast<QWidget *>(m_calBox) : ui->monthCalendar1;
    const int need = cal->y() + cal->height();
    // the band under the calendar: grid headings, time list, grid, buttons
    const int bandTop = cal->y() + 160;
    int firstBelow = INT_MAX;
    for (QWidget *c : ui->tabPage1->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        if (c != cal && c->y() >= bandTop)
            firstBelow = qMin(firstBelow, c->y());
    const int delta = firstBelow == INT_MAX ? 0 : need + 4 - firstBelow;
    if (delta > 0)
        for (QWidget *c : ui->tabPage1->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
            if (c != cal && c->y() >= bandTop)
                c->move(c->x(), c->y() + delta);
    fitWindowToSlotsTab();
}

// The window opens tall enough for the lowest control on the Slots tab (the "Slot database in use"
// button was cut off unless the window was dragged bigger): tab row + lowest control + a margin.
void MainWindow::fitWindowToSlotsTab()
{
    int bottom = 0;
    for (QWidget *c : ui->tabPage1->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        if (!c->isHidden() || c == ui->dbbutton)
            bottom = qMax(bottom, c->geometry().bottom());
    const int tabRow = ui->tabControl1->tabBar()->sizeHint().height();
    const int frame = 2 * ui->tabControl1->style()->pixelMetric(QStyle::PM_DefaultFrameWidth) + 4;
    const int needed = tabRow + bottom + 1 + 8 + frame + statusBar()->sizeHint().height();   // the status bar (TX and clock messages)
    if (height() < needed)
        resize(width(), needed);
    setMinimumHeight(qMin(needed, 600));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == ui->tabControl1 && e->type() == QEvent::Resize)
        positionClockBar();
    if (e->type() == QEvent::MouseButtonPress)
        markActivity();   // any click counts as using the app (slot tracking pauses)
    if (obj == ui->label145 && e->type() == QEvent::MouseButtonRelease) {
        showHelp();       // "F1 Help" by the clock
        return true;
    }
    // KeyPreview: only the first delivery (to the focus widget), and only for this window
    if (e->type() == QEvent::KeyPress && obj == QApplication::focusWidget() && isAncestorOf(static_cast<QWidget *>(obj))
        && Form1_KeyDown_1(static_cast<QKeyEvent *>(e)))
        return true;
    if (e->type() == QEvent::KeyPress && obj == QApplication::focusWidget() && isAncestorOf(static_cast<QWidget *>(obj))
        && Form1_KeyPress(static_cast<QKeyEvent *>(e)))
        return true;
    if (obj == ui->selSwPorttextBox && e->type() == QEvent::KeyPress)
        return selSwPorttextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->selSwitchtextBox && e->type() == QEvent::KeyPress)
        return selSwitchtextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->selTunertextBox && e->type() == QEvent::KeyPress)
        return selTunertextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->label7 && e->type() == QEvent::MouseButtonRelease)
        label7_Click();
    if (obj == ui->OffsettextBox && e->type() == QEvent::KeyPress)
        return OffsettextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->skedstoplabel && e->type() == QEvent::MouseButtonDblClick)
        skedstoplabel_DoubleClick();
    if (obj == ui->pintextBox && e->type() == QEvent::KeyPress)
        return pintextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->PasstextBox2 && e->type() == QEvent::KeyPress)
        return PasstextBox2_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->PasstextBox && e->type() == QEvent::KeyPress)
        return PasstextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->ardReplylabel && e->type() == QEvent::MouseButtonRelease)
        label82_Click();
    if (obj == ui->baseCalltextBox && e->type() == QEvent::KeyPress)
        return baseCalltextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->ArduinoPorttextBox && e->type() == QEvent::KeyPress)
        return ArduinoPorttextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->ArduinoIPtextBox && e->type() == QEvent::KeyPress)
        return ArduinoIPtextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->DefaultAntcomboBox && e->type() == QEvent::MouseButtonDblClick)
        DefaultAntcomboBox_MouseDoubleClick();
    if ((obj == ui->AlphacomboBox || obj == ui->AlphacomboBox->lineEdit()) && e->type() == QEvent::KeyPress)
        return comboBox1_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->LocatortextBox && e->type() == QEvent::KeyPress)
        return LocatortextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if ((obj == ui->defaultFcomboBox || obj == ui->defaultFcomboBox->lineEdit()) && e->type() == QEvent::KeyPress)
        return defaultFcomboBox_KeyPress(static_cast<QKeyEvent *>(e));
    if ((obj == ui->defaultpwrcomboBox || obj == ui->defaultpwrcomboBox->lineEdit()) && e->type() == QEvent::KeyPress)
        return defaultpwrcomboBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->defaultOfftextBox && e->type() == QEvent::KeyPress)
        return defaultOfftextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->CalltextBox && e->type() == QEvent::KeyPress)
        return CalltextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if ((obj == ui->COMcomboBox || obj == ui->COMcomboBox->lineEdit()) && e->type() == QEvent::KeyPress)
        return COMcomboBox_KeyPress(static_cast<QKeyEvent *>(e));
    if ((obj == ui->baudcomboBox || obj == ui->baudcomboBox->lineEdit()) && e->type() == QEvent::KeyPress)
        return baudcomboBox_KeyPress(static_cast<QKeyEvent *>(e));
    if ((obj == ui->RigcomboBox || obj == ui->RigcomboBox->lineEdit()) && e->type() == QEvent::KeyPress)
        return RigcomboBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->AntdesctextBox && e->type() == QEvent::KeyPress)
        return AntdesctextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->AntnametextBox && e->type() == QEvent::KeyPress)
        return AntnametextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->Levellabel && e->type() == QEvent::MouseButtonRelease)
        Levellabel_Click();
    if (obj == ui->FtextBox && e->type() == QEvent::KeyPress)
        return FtextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->HwSwTypelistBox && e->type() == QEvent::KeyPress)
        return HwSwTypelistBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->HwAntportstextBox && e->type() == QEvent::KeyPress)
        return HwAntportstextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->HwPorttextBox && e->type() == QEvent::KeyPress)
        return HwPorttextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->HwIPtextBox && e->type() == QEvent::KeyPress)
        return HwIPtextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->HwtextBox && e->type() == QEvent::KeyPress)
        return HwtextBox_KeyPress(static_cast<QKeyEvent *>(e));
    if (obj == ui->riglistBox->viewport() && e->type() == QEvent::MouseButtonPress)
        riglistBox_MouseDown(static_cast<QMouseEvent *>(e));
    if (obj == ui->UTClabel && e->type() == QEvent::MouseButtonRelease)
        label78_Click();
    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    if (!m_shownOnce) {
        m_shownOnce = true;
        Form1_Shown();
    }
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    Form1_FormClosing(e);
    if (!e->isAccepted())
        return;
    if (m_transmitting)
        stopTransmit("program closing", false);
    if (m_rx) {   // otherwise the receive window keeps the application running
        m_rx->setAllowClose(true);
        m_rx->close();
    }
    QMainWindow::closeEvent(e);
}

// ---------------------------------------------------------------------------
// Handler stubs - one per C# event handler
// ---------------------------------------------------------------------------

// C#: dataGridView1.RowHeaderMouseClick += dataGridView1_RowHeaderMouseClick
void MainWindow::dataGridView1_RowHeaderMouseClick(int row)
{
    // TODO: port from Form1.cs
}

// C#: dataGridView1.RowHeaderMouseDoubleClick += dataGridView1_RowHeaderMouseDoubleClick
void MainWindow::dataGridView1_RowHeaderMouseDoubleClick(int row)
{
    // TODO: port from Form1.cs
}

// C#: dataGridView1.SelectionChanged += dataGridView1_SelectionChanged
void MainWindow::dataGridView1_SelectionChanged()
{
    // TODO: port from Form1.cs
}

// C#: selSwPorttextBox.KeyPress += selSwPorttextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::selSwPorttextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: selSwitchtextBox.KeyPress += selSwitchtextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::selSwitchtextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: selTunertextBox.KeyPress += selTunertextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::selTunertextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: label7.Click += label7_Click
void MainWindow::label7_Click()
{
    // TODO: port from Form1.cs
}

// C#: tabControl1.SelectedIndexChanged += tabControl1_SelectedIndexChanged
void MainWindow::tabControl1_SelectedIndexChanged(int index)
{
    // TODO: port from Form1.cs
}

// C#: FlistBox2.SelectedIndexChanged += FlistBox2_SelectedIndexChanged
void MainWindow::FlistBox2_SelectedIndexChanged(int row)
{
    // rigctld off: the frequency the rig has been set to by hand (C#: FlistBox2, "slot freq will be ignored")
    if (row < 0)
        return;
    selectManualFrequency(ui->FlistBox2->item(row)->text().trimmed().toDouble());
}

// C#: stopInternetcheckBox.CheckedChanged += stopInternetcheckBox_CheckedChanged
void MainWindow::stopInternetcheckBox_CheckedChanged(bool checked)
{
    // C#: stopUrl - spots are still decoded and saved here, but not uploaded to wsprnet.org.
    // Also (new) no automatic internet use: the daily clock check and sunrise/sunset refresh wait;
    // the "sync clock" and sunrise/sunset buttons still work when pressed.
    m_stopInternet = checked;
    if (m_rx)
        m_rx->setStopUrl(checked);
    statusBar()->showMessage(checked ? "Internet stopped: spots are saved but not uploaded to wsprnet"
                                     : "Internet on: spots are uploaded to wsprnet (if Upload is ticked)", 10000);
}

// C#: logclearbutton.Click += logclearbutton_Click
void MainWindow::logclearbutton_Click()
{
    // TODO: port from Form1.cs
}

// C#: stopRXcheckBox.CheckedChanged += stopRXcheckBox_CheckedChanged
void MainWindow::stopRXcheckBox_CheckedChanged(bool checked)
{
    m_stopRX = checked;
    if (!m_rx)
        return;
    if (checked)
        m_rx->hide();
    else
        m_rx->show();
}

// C#: locationbutton.Click += locationbutton_Click
void MainWindow::locationbutton_Click()
{
    // TODO: port from Form1.cs
}

// C#: wsprmsgbutton.Click += wsprmsgbutton_Click
void MainWindow::wsprmsgbutton_Click()
{
    // TODO: port from Form1.cs
}

// C#: Type2checkBox.CheckedChanged += Type2checkBox_CheckedChanged
void MainWindow::Type2checkBox_CheckedChanged(bool checked)
{
    // C#: the "Send type 2/3 as one tx'n" box only shows with type 2/3 allowed; type 1 only clears
    // "allow 6 char locator". (The wsprmsg folder controls stay hidden: types 2/3 are encoded here.)
    ui->asOnecheckBox->setVisible(checked);
    if (!checked)
        ui->longcheckBox->setChecked(false);
    ui->msgTypelabel->setText(checked ? "Message type 1 or 2 or 3" : "Message type 1");
    ui->msgTypelabel2->setText(ui->msgTypelabel->text());
    ui->msgTypelabel->adjustSize();
    ui->msgTypelabel2->adjustSize();
    if (m_rx)   // C#: rxForm.change_header - the call shown and uploaded follows the box
        m_rx->changeHeader(rxCallsign(), m_store.settings(), ui->LocatortextBox->text().trimmed(), m_audioInName,
                           m_wsprdPath);
}

// C#: savePasswordbutton.Click += savePasswordbutton_Click
void MainWindow::savePasswordbutton_Click()
{
    // C#: updatePassword / saveDBPassword / saveUserandPassword
    const QString pw = ui->PasstextBox->text().trimmed();
    const DbSettings old = m_store.settings();
    auto finish = [this](const DbSettings &now) {
        m_store.setSettings(now);
        if (m_rx)
            m_rx->store().setSettings(now);
        QSettings st;
        if (st.contains("db/password"))   // an override here would otherwise win over the new password
            st.setValue("db/password", now.password);
        const QSignalBlocker b(ui->PasstextBox);
        ui->PasstextBox->setText(now.password);
        ui->PasstextBox2->clear();
        ui->PasstextBox2->hide();
        ui->Passlabel2->hide();
        ui->showPasscheckBox->setChecked(false);
    };
    auto writeFile = [this](const DbSettings &now) {
        if (!Security::writeDbCredential(now.user, now.password, QString(), ui->baseCalltextBox->text().trimmed()))
            Msg::ok(this, "The password works but the DBcredential file in your home folder could not be written.\n"
                          "Keep a note of the password: " + now.password, "Database password");
    };

    if (pw.size() < 4) {
        Msg::ok(this, "Password is either blank or too short", "Password > 3?");
        return;
    }
    if (pw != ui->PasstextBox2->text().trimmed()) {
        Msg::ok(this, "Passwords do not match", "Database password");
        return;
    }
    static const QRegularExpression allowed("^[A-Za-z0-9.,_ -]{4,64}$");
    if (!allowed.match(pw).hasMatch()) {
        Msg::ok(this, "Use only letters, digits and . , - _ or space (4 to 64 characters)", "Database password");
        return;
    }
    QString err;
    if (!SlotStore::canLogIn(old, &err)) {
        // C#: "use this to recover" - the saved password is wrong; perhaps the typed one is the real one
        if (!Msg::yesNo(this, "Can't log in to the database with the saved password.\n"
                              "Is the password you typed the database's current password (recovery)?", "Database password"))
            return;
        DbSettings typed = old;
        typed.password = pw;
        if (!SlotStore::canLogIn(typed, &err)) {
            Msg::ok(this, "That password doesn't work either:\n" + err.left(200), "Database password");
            return;
        }
        writeFile(typed);
        finish(typed);
        Msg::ok(this, "OK - the program now logs in with this password", "Database password");
        return;
    }
    if (pw == old.password) {
        Msg::ok(this, "That is already the database password", "Database password");
        return;
    }
    if (!Msg::yesNo(this, QString("Change the database password for user \"%1\"?\n\n"
                                  "The C# WSPR Sked and other programs that read the DBcredential file in your home "
                                  "folder will use the new password too; anything else that logs in as \"%1\" "
                                  "will need it.").arg(old.user),
                    "Database password"))
        return;
    if (!m_store.changeOwnPassword(pw)) {
        Msg::ok(this, "Unable to update password:\n" + m_store.lastError().left(300), "DB password");
        return;
    }
    DbSettings now = old;
    now.password = pw;
    if (!SlotStore::canLogIn(now, &err)) {
        if (SlotStore::canLogIn(old))
            Msg::ok(this, "The server kept the old password - nothing changed.", "DB password");
        else
            Msg::ok(this, "The password was changed but logging in with it failed:\n" + err.left(200)
                              + "\n\nKeep a note of the new password: " + pw, "DB password");
        return;
    }
    writeFile(now);
    finish(now);
    Msg::ok(this, "Database password changed", "Database password");
}

// C#: PasstextBox2.KeyPress += PasstextBox2_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::PasstextBox2_KeyPress(QKeyEvent *e)
{
    // C#: only letters, digits and . , - _ space
    const QString t = e->text();
    if (t.isEmpty() || !t.at(0).isPrint())
        return false;   // control keys (backspace, arrows...)
    const QChar c = t.at(0);
    return !(c.isLetterOrNumber() || QStringLiteral(".,-_ ").contains(c));
}

// C#: showPasscheckBox.CheckedChanged += showPasscheckBox_CheckedChanged
void MainWindow::showPasscheckBox_CheckedChanged(bool checked)
{
    // C#: PasswordChar '\0' / '*'
    const auto mode = checked ? QLineEdit::Normal : QLineEdit::Password;
    ui->PasstextBox->setEchoMode(mode);
    ui->PasstextBox2->setEchoMode(mode);
}

// C#: PasstextBox.TextChanged += PasstextBox_TextChanged
void MainWindow::PasstextBox_TextChanged(const QString &text)
{
    Q_UNUSED(text)
    // C#: typing a new password shows the "re-enter" box
    ui->PasstextBox2->setVisible(true);
    ui->Passlabel2->setVisible(true);
}

// C#: PasstextBox.KeyPress += PasstextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::PasstextBox_KeyPress(QKeyEvent *e)
{
    // C#: only letters, digits and . , - _ space
    const QString t = e->text();
    if (t.isEmpty() || !t.at(0).isPrint())
        return false;   // control keys (backspace, arrows...)
    const QChar c = t.at(0);
    return !(c.isLetterOrNumber() || QStringLiteral(".,-_ ").contains(c));
}

// C#: ardReplylabel.Click += label82_Click
void MainWindow::label82_Click()
{
    // TODO: port from Form1.cs
}

// C#: button3.Click += button3_Click
void MainWindow::button3_Click()
{
    // TODO: port from Form1.cs
}

// C#: testEncodebutton.Click += testEncodebutton_Click
void MainWindow::testEncodebutton_Click()
{
    // C#: testEncodebutton_Click - encode with the chosen message type and slot, show the 162 symbols.
    // (C# ran wspr_enc.exe for types 2/3; they are encoded here.)
    takeDefaultsFromUi();
    ui->testdbmlabel->setText(QString::number(m_cfg.defaultdBm));
    ui->testdbmlabel->adjustSize();
    // The message type follows from the call, locator and type 2/3 settings, as for a slot (checkCall),
    // so it is no longer chosen here. Slot 2 only exists when type 2/3 go as two transmissions.
    int msgT = checkCall(ui->CalltextBox->text().trimmed().toUpper(), ui->LocatortextBox->text().trimmed());
    if (!ui->Type2checkBox->isChecked() || msgT < 1)
        msgT = 1;
    const bool twoTx = ui->Type2checkBox->isChecked() && !ui->asOnecheckBox->isChecked();
    const int slot = twoTx && ui->slotlistBox->currentRow() == 1 ? 2 : 1;
    TxMessageRequest req;
    req.callsign = ui->CalltextBox->text().trimmed().toUpper();
    req.baseCall = ui->baseCalltextBox->text().trimmed().toUpper();
    if (req.baseCall.isEmpty())
        req.baseCall = findBaseCall(req.callsign).isEmpty() ? req.callsign : findBaseCall(req.callsign);
    req.locator = ui->LocatortextBox->text().trimmed();
    req.powerdBm = m_cfg.defaultdBm;
    req.msgType = msgT;
    req.slotNo = slot;
    req.allowType2 = ui->Type2checkBox->isChecked();
    req.longLocator = ui->longcheckBox->isChecked() || msgT == 3;
    req.oneMsg = ui->asOnecheckBox->isChecked();   // what the scheduler would send with this setting
    const TxMessage m = planMessage(req);
    if (!m.ok()) {
        ui->leveltextBox->setPlainText(describeMessage(m, req));
        return;
    }
    QString sym;
    for (size_t i = 0; i < m.symbols->size(); ++i) {
        sym += QChar('0' + (*m.symbols)[i]);
        if (i % 54 == 53)
            sym += '\n';
    }
    ui->leveltextBox->setPlainText(QString("Type %1, slot %2: %3\n%4\n\n%5")
                                       .arg(m.wsprType).arg(slot).arg(m.text, describeMessage(m, req), sym.trimmed()));
}

// C#: baseCalltextBox.KeyPress += baseCalltextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::baseCalltextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: DefaultAntcomboBox.MouseDoubleClick += DefaultAntcomboBox_MouseDoubleClick
void MainWindow::DefaultAntcomboBox_MouseDoubleClick()
{
    // TODO: port from Form1.cs
}

// C#: FlistBox.MouseDoubleClick += FlistBox_MouseDoubleClick
void MainWindow::FlistBox_MouseDoubleClick(QListWidgetItem *item)
{
    // TODO: port from Form1.cs
}

// C#: AlphacomboBox.KeyPress += comboBox1_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::comboBox1_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: LocatortextBox.KeyPress += LocatortextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::LocatortextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: defaultFcomboBox.KeyPress += defaultFcomboBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::defaultFcomboBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: defaultpwrcomboBox.KeyPress += defaultpwrcomboBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::defaultpwrcomboBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: defaultOfftextBox.KeyPress += defaultOfftextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::defaultOfftextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: CalltextBox.KeyPress += CalltextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::CalltextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: COMcomboBox.KeyPress += COMcomboBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::COMcomboBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: baudcomboBox.KeyPress += baudcomboBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::baudcomboBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: RigcomboBox.KeyPress += RigcomboBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::RigcomboBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: AntdesctextBox.KeyPress += AntdesctextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::AntdesctextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: AntnametextBox.KeyPress += AntnametextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::AntnametextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: Levellabel.Click += Levellabel_Click
void MainWindow::Levellabel_Click()
{
    // TODO: port from Form1.cs
}

// C#: HwTuTypelistBox.SelectedIndexChanged += listBox2_SelectedIndexChanged
void MainWindow::listBox2_SelectedIndexChanged(int row)
{
    // TODO: port from Form1.cs
}

// C#: HwSwTypelistBox.KeyPress += HwSwTypelistBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::HwSwTypelistBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: HwAntportstextBox.KeyPress += HwAntportstextBox_KeyPress  (return true to swallow the key, like e.Handled = true)
bool MainWindow::HwAntportstextBox_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}

// C#: EditRXbutton.Click += EditRXbutton_Click
void MainWindow::EditRXbutton_Click()
{
    // TODO: port from Form1.cs
}

// C#: UTClabel.Click += label78_Click
void MainWindow::label78_Click()
{
    // TODO: port from Form1.cs
}

// C#: idletimer.Tick += idletimer_Tick
void MainWindow::idletimer_Tick()
{
    // TODO: port from Form1.cs
}

// C#: this.FormClosing += Form1_FormClosing  (e->ignore() == e.Cancel = true)
void MainWindow::Form1_FormClosing(QCloseEvent *e)
{
    if (m_closeConfirmed)   // already answered (closeEvent can come twice)
        return;
    if (!Msg::yesNo(this, "Exit app (y/n)?", "Exit")) {
        e->ignore();
        return;
    }
    m_closeConfirmed = true;
    if (m_rx)
        m_rx->saveConfig();   // C#: rxForm.Save_Config
    if (Msg::yesNo(this, "Save all settings before exit (y/n)?", "Save Settings")) {
        const QString problems = saveAllSettings();   // C#: SaveAll, SaveRigctl, Save_Audio
        if (!problems.isEmpty())
            Msg::ok(this, "Some settings were not saved:\n\n" + problems, "Save Settings");
    }
}

// C#: this.KeyPress += Form1_KeyPress  (KeyPreview; return true to swallow, like e.Handled = true)
bool MainWindow::Form1_KeyPress(QKeyEvent *e)
{
    // TODO: port from Form1.cs
    return false;
}
