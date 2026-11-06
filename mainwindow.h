#pragma once
// Generated from Form1.Designer.cs - control and handler names kept from C#.
#include <QMainWindow>
#include <QModelIndex>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QHash>
#include <memory>
#include <optional>

#include "repeatplanner.h"
#include "slotstore.h"
#include "remoteserver.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class QTimer;
class SlotTableModel;
class RxForm;
class InputLevelMonitor;
class RigctldProcess;
class RigCtl;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QKeyEvent;
class QMouseEvent;
class WsprAudioOut;
class QDoubleSpinBox;
class QFrame;
class QCalendarWidget;
class QLabel;
class QPushButton;
class QLineEdit;
class SunFetcher;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private slots:
    void monthCalendar1_DateChanged();
    void monthCalendar1_DateSelected(QDate date);
    void timelistBox_SelectedIndexChanged(int row);
    void dataGridView1_CellMouseClick(const QModelIndex &index, Qt::MouseButton button);
    void dataGridView1_CellMouseDoubleClick(const QModelIndex &index);
    void dataGridView1_RowHeaderMouseClick(int row);
    void dataGridView1_RowHeaderMouseDoubleClick(int row);
    void dataGridView1_SelectionChanged();
    void AllcheckBox_CheckedChanged(bool checked);
    void NightcheckBox_CheckedChanged(bool checked);
    void DaycheckBox_CheckedChanged(bool checked);
    void repeatTimecheckBox_CheckedChanged(bool checked);
    bool selSwPorttextBox_KeyPress(QKeyEvent *e);
    bool selSwitchtextBox_KeyPress(QKeyEvent *e);
    bool selTunertextBox_KeyPress(QKeyEvent *e);
    void AntselcomboBox_SelectedIndexChanged(int index);
    void timeEnd_ValueChanged(QTime time);
    void repeatcheckBox_CheckedChanged(bool checked);
    void dateEnd_ValueChanged(QDate date);
    void dBmcomboBox_SelectedIndexChanged(int index);
    void CancelSlotbutton_Click();
    void SaveSlotbutton_Click();
    void label7_Click();
    void ActivecheckBox_CheckedChanged(bool checked);
    void OffsettextBox_TextChanged(const QString &text);
    bool OffsettextBox_KeyPress(QKeyEvent *e);
    void tabControl1_SelectedIndexChanged(int index);
    void syncbutton_Click();
    void dbbutton_Click();
    void flsuhSlotbutton_Click();
    void skedstoplabel_DoubleClick();
    void FlistBox2_SelectedIndexChanged(int row);
    void stopInternetcheckBox_CheckedChanged(bool checked);
    void deleteAllSlotsbutton_Click();
    void currHourbutton_Click();
    void LTcheckBox_CheckedChanged(bool checked);
    void TXrunbutton_Click();
    void TXenablebutton_Click();
    void logclearbutton_Click();
    void stopRXcheckBox_CheckedChanged(bool checked);
    void sunrisebutton_Click();
    void locationbutton_Click();
    bool pintextBox_KeyPress(QKeyEvent *e);
    void noSkedcheckBox_CheckedChanged(bool checked);
    void wsprmsgbutton_Click();
    void Type2checkBox_CheckedChanged(bool checked);
    void savePasswordbutton_Click();
    bool PasstextBox2_KeyPress(QKeyEvent *e);
    void showPasscheckBox_CheckedChanged(bool checked);
    void PasstextBox_TextChanged(const QString &text);
    bool PasstextBox_KeyPress(QKeyEvent *e);
    void label82_Click();
    void button3_Click();
    void testEncodebutton_Click();
    bool baseCalltextBox_KeyPress(QKeyEvent *e);
    void ArduinoPorttextBox_TextChanged(const QString &text);
    bool ArduinoPorttextBox_KeyPress(QKeyEvent *e);
    bool ArduinoIPtextBox_KeyPress(QKeyEvent *e);
    void DefaultAntcomboBox_SelectedIndexChanged(int index);
    void DefaultAntcomboBox_MouseDoubleClick();
    void FlistBox_SelectedIndexChanged(int row);
    void FlistBox_MouseDoubleClick(QListWidgetItem *item);
    void button1_Click();
    void AlphacomboBox_SelectedValueChanged(int index);
    void comboBox1_TextChanged(const QString &text);
    bool comboBox1_KeyPress(QKeyEvent *e);
    void stopWAVbutton_Click();
    bool LocatortextBox_KeyPress(QKeyEvent *e);
    void enableTXcheckBox_CheckedChanged(bool checked);
    void stopTestbutton_Click();
    void TestTXbutton_Click();
    void testFtextBox_TextChanged(const QString &text);
    void testButton_Click();
    bool defaultFcomboBox_KeyPress(QKeyEvent *e);
    void defaultpwrcomboBox_SelectedIndexChanged(int index);
    bool defaultpwrcomboBox_KeyPress(QKeyEvent *e);
    void saveConfigbutton_Click();
    bool defaultOfftextBox_KeyPress(QKeyEvent *e);
    void TXTestbutton_Click();
    bool CalltextBox_KeyPress(QKeyEvent *e);
    bool COMcomboBox_KeyPress(QKeyEvent *e);
    bool baudcomboBox_KeyPress(QKeyEvent *e);
    void comRbutton_Click();
    void VoxcheckBox_CheckedChanged(bool checked);
    void rigsearchbutton_Click();
    void rigctldcheckBox_CheckedChanged(bool checked);
    void SaveRPathbutton_Click();
    void RigCtlPbutton_Click();
    void stopRigCtlDbutton_Click();
    void startRigCtlbutton_Click();
    bool RigcomboBox_KeyPress(QKeyEvent *e);
    void SaveRigctlbutton_Click();
    void ShowSwlistBox2_SelectedIndexChanged(int row);
    void ShowSwlistBox_SelectedIndexChanged(int row);
    void CancelAbutton_Click();
    void SaveAbutton_Click();
    bool AntdesctextBox_KeyPress(QKeyEvent *e);
    bool AntnametextBox_KeyPress(QKeyEvent *e);
    void Fcancelbutton_Click();
    void Fsavebutton_Click();
    void Levellabel_Click();
    void trackBar1_ValueChanged(int value);
    void FtextBox_TextChanged(const QString &text);
    bool FtextBox_KeyPress(QKeyEvent *e);
    void PopulateFbutton_Click();
    void editFbutton_Click();
    void FreqlistBox_MouseDoubleClick(QListWidgetItem *item);
    void AddAbutton_Click();
    void DelAbutton_Click();
    void EditAbutton_Click();
    void AntlistBox_MouseDoubleClick(QListWidgetItem *item);
    void editFibutton_Click();
    void FilistBox_MouseClick(QListWidgetItem *item);
    void FilistBox_MouseDoubleClick(QListWidgetItem *item);
    void editTubutton_Click();
    void editSwbutton_Click();
    void listBox2_SelectedIndexChanged(int row);
    void HwSwTypelistBox_SelectedIndexChanged(int row);
    bool HwSwTypelistBox_KeyPress(QKeyEvent *e);
    bool HwAntportstextBox_KeyPress(QKeyEvent *e);
    bool HwPorttextBox_KeyPress(QKeyEvent *e);
    bool HwIPtextBox_KeyPress(QKeyEvent *e);
    void HwProtocolcomboBox_SelectedIndexChanged(int index);
    bool HwtextBox_KeyPress(QKeyEvent *e);
    void HwCancelbutton_Click();
    void HwSavebutton_Click();
    void TulistBox_MouseDoubleClick(QListWidgetItem *item);
    void SwlistBox_MouseDoubleClick(QListWidgetItem *item);
    void trackBarGain_ValueChanged(int value);
    void EditRXbutton_Click();
    void samecheckBox_CheckedChanged(bool checked);
    void wsprdbutton_Click();
    void SaveAudiobutton_Click();
    void button5_Click();
    void Monitorbutton_Click();
    void audioUpdatebutton_Click();
    void audioOutlistBox_SelectedIndexChanged(int row);
    void audioInlistBox_SelectedIndexChanged(int row);
    void editrigbutton_Click();
    void addrigbutton_Click();
    void riglistBox_MouseClick(QListWidgetItem *item);
    void riglistBox_MouseDoubleClick(QListWidgetItem *item);
    void riglistBox_MouseDown(QMouseEvent *e);
    void httpscheckBox_CheckedChanged(bool checked);
    void rigprotocolcomboBox_SelectedIndexChanged(int index);
    void rigcancelbutton_Click();
    void saverigbutton_Click();
    void showcodecheckBox_CheckedChanged(bool checked);
    void remoteSaveButton_Click();
    void WSPRtimer_Tick();
    void daytimer_Tick();
    void testPTTtimer_Tick();
    void wsprTXtimer_Tick();
    void daytimer2_Tick();
    void label78_Click();
    void RXblocktimer_Tick();
    void idletimer_Tick();
    void Form1_FormClosing(QCloseEvent *e);
    void Form1_Load();
    void Form1_Shown();
    bool Form1_KeyDown_1(QKeyEvent *e);
    bool Form1_KeyPress(QKeyEvent *e);

private:
    void setupClockBar();                  // clock on the tab row (mainwindow.cpp)
    void positionClockBar();
    void setupTabFont();                   // C#: MS Sans Serif 8.25 pt, narrow WinForms-style tabs
    void setupCalendars();                 // two-month calendar (C#: MonthCalendar 2 x 1)
    void syncCalendars();
    void makeRoomForCalendar();
    void fitWindowToSlotsTab();
    QFrame *m_calBox = nullptr;
    QCalendarWidget *m_cal2 = nullptr;   // the right-hand month
    QLabel *m_calMonth1 = nullptr, *m_calMonth2 = nullptr;
    QDate m_calLeft;                       // first day of the left-hand month

    // ---- Slots tab (mainwindow_slots.cpp) ----
    void setupSlotGrid();
    DbSettings loadDbSettings() const;
    void applyConfigToUi();
    void loadAntennas();
    void setDbStatus(bool ok);
    QDateTime schedNow() const;
    void updateClockLabels();
    void markActivity();
    void currHour(bool noChangeIfMatch, bool exactMin);
    void showHour(const QDate &date, int hour, std::optional<QTime> select);
    void reloadCurrentHour();
    int selectedSlotRow() const;
    QString locator4() const;
    void updateSunLabels(const QDate &date, int hour);
    std::optional<SunWindow> sunWindow(const QDate &date);
    void openSlotEditor(int row);
    void closeSlotEditor();
    void selectFrequency(double mhz);
    void fillEditorDefaults();
    void fillEditorFromSlot(const SlotRecord &s);
    void applyAntenna(const QString &name);
    void updateRepeatUi();
    void checkPeriod();
    int rptTypeFromUi() const;
    void exclusiveRepeat(QCheckBox *keep);
    bool validateSlot();
    int checkCall(const QString &call, QString locator);
    bool checkNextSlot(int row);
    bool saveSlotSet(const QDate &date, const QTime &time, int slotNo, int msgT, bool thisSlotOnly,
                     const QString &parent);
    QString parentFor(const QDate &d, const QTime &t, const std::optional<SlotRecord> &original, int rptType) const;
    void deleteSlotAt(const QDate &d, const QTime &t, bool all, const QString &parent);
    void deleteThisRow();
    void showRangeSelection();
    void endRangeMode();

    // ---- Freq&Ant tab (mainwindow_freqant.cpp) ----
    void setupFreqAnt();                   // C#: ReadAntennas list part + ReadFrequencies
    void fillAntennaList();
    void updateFrequencyboxes();           // C#: updateFrequencyboxes (FlistBox, defaultFcomboBox, slot editor)
    void editF();                          // C#: editF
    void addAnt();                         // C#: addAnt
    void editAnt();                        // C#: editAnt
    void delAnt();                         // C#: delAnt
    bool saveAnt();                        // C#: saveAnt
    void closeAntEditor();
    void closeFreqEditor();
    void setDefaultSwitchesandTuners();    // C#: setDefaultSwitchesandTuners
    void updateDefaultAnt();               // C#: updateDefaultAnt
public:
    double txLevelFor(double mhz) const;   // audio level saved for this frequency (1.0 if none)
    float txVolume() const { return m_txVolume; }
private:

    // ---- Switches&Tuners tab (mainwindow_hw.cpp) ----
    void setupHardware();                  // C#: ReadHardware("switches"/"tuners"/"filters")
    void loadHardwareList(const QString &table);
    QListWidget *hwList(const QString &table) const;
    QVector<HardwareInfo> &hwItems(const QString &table);
    void editButtonAction(const QString &hwtype);   // C#: editButtonAction ("switch", "tuner", "filter")
    void editHW(const QString &table);     // C#: editHW + loadHWtoGroupBox
    void setHwBoxes(bool serial);          // C#: setHwBoxes
    void hideHwBoxes();                    // C#: hideHwBoxes
    void closeHwEditor();
    static bool checkIP(const QString &ip);   // C#: checkIP
public:
    const QVector<HardwareInfo> &switches() const { return m_sw; }
    const QVector<HardwareInfo> &tuners() const { return m_tu; }
    const QVector<HardwareInfo> &filters() const { return m_fil; }
    int selectedFilter() const { return m_selectedFilter; }
private:

    // ---- Rigctl rigs tab (mainwindow_rig.cpp) ----
    void setupRig();                       // C#: getComports + readRigctl + getRigList + runRigCtlD at load
    void getComports();                    // C#: getComports (COM lists on this tab and Switches)
    void getRigList();                     // C#: getRigList ("rigctl -l")
    void applyRigctlToUi();
    RigctlSettings rigctlFromUi() const;
    void ensurePpmBox();
    void showRadioToUse();                 // "Radio to use: Yaesu FT-450D" under Stop RigCtlD
public:
    static QString radioDisplayName(const QString &rigListLine);
private:                   // "Frequency correction (ppm)" (new)
    void startRigctld(bool announce);      // C#: runRigCtlD / startRigCtlbutton
    void setRigLabels(bool running);       // rigrunlabel, Riglabel, Riglabel1
    void getRigF();                        // C#: getRigF - dial frequency from rigctld every 30 s
    QString hamlibPath() const;

    // ---- receive window (mainwindow_rx.cpp) ----
    void setupReceiver();                  // C#: rxForm.set_header + Read_Audio (Form1_Load)
    void showReceiver();                   // C#: if (!stopRX) rxForm.Show()
    void receiverTick(const QDateTime &utc);   // C#: the rxForm part of daytimer_Action
    QString rxCallsign() const;
    bool txSlotActiveNow() const;          // C#: enableTXcheckBox.Checked && slotActive (not in a dry run)
    void findSound();                      // C#: findSound - fill the device lists
    void matchAudioList();                 // C#: match_audio_list - select the saved devices
    bool saveAudio();                      // C#: Save_Audio
    void selectSameCard();                 // "Same device for input and output"

    // ---- TX Configuration defaults and Save Settings (mainwindow_config.cpp) ----
    void setupConfigTab();
    void takeDefaultsFromUi();             // default F / offset / power / antenna -> m_cfg
    QString findBaseCall(const QString &call) const;   // C#: findBaseCall
    QString saveAllSettings();
    void showTestSlotChoice();
    // ---- Non Rigctl TX tab (mainwindow_otherrig.cpp) ----
    void setupOtherRigs();
    void loadOtherRigList();
    const OtherRig *otherRig(int id) const;
    void showSelectedRig();
    bool otherRigInUse() const;            // C#: noRigctld && selectedRig > 0
    bool manualFrequencyMode() const;      // rigctld off, no Non Rigctl TX rig: rig tuned by hand
    void selectManualFrequency(double mhz);
    double m_manualMHz = 0.0;              // C#: FlistBox2 selection
    void setRigBoxes(bool serial);         // C#: setrigBoxes
    void layoutRigEditor();
    void openRigEditor(const OtherRig &r, bool isNew);
    QString otherRigCommand(QString cmd, const SlotRecord &s) const;   // freq call grid pwr offset filled in
    bool sendOtherRig(const QString &commands, QString *error, bool checkReply = false);   // C#: sendOtherTXRigCommand
    QLineEdit *m_rigCheckReply = nullptr;  // "Reply to TX command" (new): empty = no reply expected
    QVector<OtherRig> m_otherRigs;
    int m_selectedRig = 0;                 // C#: selectedRig
    bool m_rigEditNew = false;
    int m_rigEditId = 0;

    // ---- Test Arduino / rigctld connection test (mainwindow_tests.cpp) ----
    void setupHardwareTests();
    void testRigConnection();
    QPushButton *m_rigTestButton = nullptr;             // on exit: settings, rigctl, audio; returns what failed
    bool m_closeConfirmed = false;

    // ---- help window (mainwindow_help.cpp) ----
    void setupHelp();
    QString helpFile() const;              // C:\WSPR_Sked\wspr_sked2_help.rtf, else next to the program
    QWidget *m_help = nullptr;             // C#: HelpForm
    class QTextBrowser *m_helpText = nullptr;
public:
    void showHelp();                       // C#: ShowHelp (F1)
private:

    // ---- Remote tab (mainwindow_remote.cpp) ----
    void setupRemote();
    bool startRemote(QString *error);      // (re)start the HTTPS server with m_remoteCfg
    void showRemoteStatus();
    QString remoteStatus();                // the status box on the remote page
    QString remoteTxNext(const QString &freqMHz, const QString &antenna);   // C#: RemoteTxNextSlot
    RemoteServer *m_remote = nullptr;
    RemoteConfig m_remoteCfg;
    QString m_remoteError;                 // why the server isn't running
    QStringList m_remoteLog;               // last few remote actions
    QString m_remoteTxStatus;              // C#: remoteTxStatus
public:
    RemoteServer *remoteServer() const { return m_remote; }   // tests
private:

    // ---- sunrise/sunset, clock, alpha (mainwindow_sun.cpp) ----
    void setupSunAndClock();
    void updateSunTimes(bool announce);    // C#: updateSunriseSunset
    void sunTimesFetched(bool ok, const QMap<QString, SunTimes> &days, const QString &error);
    void syncClock(bool announce);         // C#: SyncSystemClockWithNTP
    void clockAndSunTick(const QDateTime &utc);
    SunFetcher *m_sunFetch = nullptr;
    QString m_sunForLocator;
    bool m_sunAnnounce = false;
    bool m_clockSyncRunning = false;
    bool m_stopInternet = false;           // C#: stopUrl ("Stop Internet (if portable)")

    // ---- transmit (mainwindow_tx.cpp) ----
    void setupTransmit();
    void lookUpNextSlot(const QDateTime &utc);   // C#: findSlot at odd :54-57
    void prepareSlot();                    // C#: WSPRtimer prep - antenna, filter, rig frequency
    void startTransmit();                  // C#: StartTX / WSPR_Transmit
    void startTxAudio();
    enum class TxTest { None, Scheduled, Now, AudioOnly, Remote };   // Remote: "Go next slot" from the Remote page
    std::optional<SlotRecord> testSlotFromUi(QString *error) const;   // default antenna/offset/power, test frequency
    bool txTestAllowed(bool needRig);
    void switchToAntenna(const QString &name);   // C#: DefaultAntcomboBox -> activateAntSwitch
    void tuneTo(double mhz);                     // C#: testFreq - FlistBox click tunes the rig
    void stopTransmit(const QString &why, bool completed);   // C#: wsprTXtimer_Action / stopTX
    bool keyPtt(bool on, QString *error);  // C#: PTT
    RigCtl *txRig();                       // rigctld connection with the current settings
    void showTxState(const QDateTime &utc);   // countdown labels, TXrunbutton (C#: flashTX)
    QString countdownText(const QDateTime &utc);   // "TX in 95 seconds" / "TX time: 37" (C#: WSPRtimer + flashTX)
    void txStatus(const QString &text, bool error = false);
    double txAlpha() const;
public:
    void txTick(const QDateTime &utc);     // C#: slot part of daytimer_Action + WSPRtimer_Action (public for tests)
    bool isDryRun() const;
    bool isTransmitting() const { return m_transmitting; }
private:

    Ui::MainWindow *ui;
    bool m_shownOnce = false;

    SlotStore m_store;
    SlotTableModel *m_slotModel = nullptr;
    AppConfig m_cfg;
    QVector<AntennaInfo> m_antennas;
    QHash<QString, SunTimes> m_sunCache;   // "MM-dd" -> local rise/set for m_sunLocator
    QString m_sunLocator;
    QDate m_selectedDate;
    bool m_dbOk = true;
    bool m_showDbError = true;
    int m_idleSeconds = 0;                 // C#: keypresses
    bool m_slotMoved = false;              // C#: currslotmoved

    // Freq&Ant
    QVector<FrequencyLevel> m_freqLevels;  // FreqlistBox contents
    bool m_newAnt = false;                 // C#: newAcheckBox
    bool m_newFreq = false;                // C#: newFcheckBox
    QString m_oldAntName;                  // C#: oldAnt
    float m_txVolume = 1.0f;               // C#: Volume (follows trackBar1, also during TX)

    // switches, tuners, filters (index = list row; entry 0 is "No ...")
    QVector<HardwareInfo> m_sw, m_tu, m_fil;
    QString m_hwTable;                     // table being edited in HwgroupBox
    int m_selectedFilter = 0;              // C#: selectedFilter

    // rig control
    RigctldProcess *m_rigctld = nullptr;
    std::unique_ptr<RigCtl> m_rig;         // reads the dial frequency (C#: sendRigCommand("f"))
    RigctlSettings m_rigCfg;
    bool m_noRigctld = false;              // C#: noRigctld
    bool m_rigRunning = false;
    QDoubleSpinBox *m_ppmBox = nullptr;    // rig reference correction
    QString m_dialMHz;                     // last frequency read from the rig, "14.095600"

    // receive window
    RxForm *m_rx = nullptr;
    bool m_stopRX = false;                 // C#: stopRX
    bool m_recordFlag = false;             // C#: recordFlag
    QString m_audioInName;                 // C#: audioInName (wspr_configs.rxsettings)
    QString m_wsprdPath;                   // C#: wsprdfilepath
    float m_gain = 1.0f;                   // C#: gain = inLevel / 100
    int m_inLevel = 100;                   // C#: inLevel (trackBarGain 1..500)
    int m_outLevel = 100;                  // C#: outLevel (TX audio level, used by transmit)
    QString m_audioOutName;                // C#: audioOutName
    InputLevelMonitor *m_levelMon = nullptr;   // C#: MonitorMic

    // slot editor state
    int m_editRow = -1;
    QDate m_editDate;
    QTime m_editTime;
    std::optional<SlotRecord> m_editOriginal;       // slot being edited (nullopt = new)
    std::optional<SlotRecord> m_editNextOriginal;   // the slot 2 minutes later (type 2/3 pairs)
    int m_origRptType = 0;                          // C#: rptType
    bool m_repeatStatus = false;
    bool m_loadingEditor = false;

    // "Delete all slots from" range selection
    bool m_rangeMode = false;
    QDate m_rangeStart, m_rangeEnd;
    // transmit
    std::optional<SlotRecord> m_txSlot;    // slot for the coming / current cycle (C#: Slot)
    QDateTime m_txCycleUtc;                // start of that cycle
    bool m_slotActive = false;             // C#: slotActive
    bool m_lookedUp = false;               // C#: Flag
    bool m_prepDone = false;               // C#: prepDone
    bool m_fired = false;                  // C#: triggered
    bool m_transmitting = false;
    bool m_pttOn = false;
    bool m_flash = false;
    double m_txMHz = 0.0;
    QString m_txAntenna;
    QString m_txCall;                      // call that went out (for the log)
    WsprAudioOut *m_txAudio = nullptr;
    QCheckBox *m_dryRunBox = nullptr;
    QTimer *m_txAudioTimer = nullptr;      // starts the audio at even :01
    TxTest m_txTest = TxTest::None;
    bool m_txWide = false;                 // "wider tones" test: audio only, never PTT, not logged
    std::optional<SlotRecord> m_preview;   // next cycle's slot, read early for the countdown only
    QDateTime m_previewCycle;        // a Test TX button started this transmission

    QTimer *WSPRtimer = nullptr;
    QTimer *daytimer = nullptr;
    QTimer *testPTTtimer = nullptr;
    QTimer *wsprTXtimer = nullptr;
    QTimer *daytimer2 = nullptr;
    QTimer *RXblocktimer = nullptr;
    QTimer *idletimer = nullptr;
};
