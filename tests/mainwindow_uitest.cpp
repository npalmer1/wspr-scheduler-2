// Drives the real main window against the scratch MariaDB (tests/fixture.sql), answering message boxes
// with Yes/OK. Covers the Freq&Ant tab: antennas (add / edit / rename in slots / delete) and the
// per-frequency TX audio level. Saves screenshots of the tabs (tab_N.png) in the working directory.
// Run with QT_QPA_PLATFORM=offscreen on a machine without a display.
#include "appfont.h"
#include "mainwindow.h"
#include "slotstore.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTimer>
#include <QCheckBox>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QTableView>
#include <QCalendarWidget>
#include <QDoubleSpinBox>
#include <QMutex>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimeZone>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QShortcut>
#include <QDir>
#include "rxspot.h"
#include <QSslSocket>
#include <QFile>
#include "rigctl.h"
#include "txplan.h"
#include <cmath>
#include <cstdio>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { ++fails; std::printf("FAIL line %d: %s\n", __LINE__, #c); } else std::printf("ok   %s\n", #c); } while (0)

// Answers like the Arduino switch sketch; runs in its own thread because the switch calls block
class FakeBoard : public QObject
{
public:
    QTcpServer *server = nullptr;
    QMutex lock;
    QStringList commands;
    void start()
    {
        server = new QTcpServer(this);
        server->listen(QHostAddress::LocalHost);
        connect(server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *c = server->nextPendingConnection();
            connect(c, &QTcpSocket::readyRead, c, [this, c] {
                if (!c->canReadLine()) return;
                const QString cmd = QString::fromLatin1(c->readLine()).trimmed();
                { QMutexLocker l(&lock); commands << cmd; }
                const QStringList p = cmd.split(' ');
                const QString reply = p.size() == 3 ? QString("Pin: %1 set: %2").arg(p[1], p[2]) : QString("ok");
                c->write((reply + "\r\n").toLatin1());
                c->disconnectFromHost();
            });
        });
    }
    QStringList take() { QMutexLocker l(&lock); QStringList c = commands; commands.clear(); return c; }
};

// A web-controlled transmitter (Non Rigctl TX): records the request paths, answers 200 OK
class FakeWeb : public QObject
{
public:
    QTcpServer *server = nullptr;
    QMutex lock;
    QStringList paths;
    QString band = "20";   // what "SET FREQ" is answered with: "Band set to: <band>"
    bool confirm = true;   // false: "SET FREQ" is answered "error"
    void start()
    {
        server = new QTcpServer(this);
        server->listen(QHostAddress::LocalHost);
        connect(server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *c = server->nextPendingConnection();
            connect(c, &QTcpSocket::readyRead, c, [this, c] {
                const QByteArray req = c->readAll();
                const QList<QByteArray> first = req.left(req.indexOf('\r')).split(' ');
                QByteArray body = "ok";
                { QMutexLocker l(&lock); paths << QString::fromLatin1(first.value(1));
                  if (first.value(1).contains("SET+FREQ")) body = confirm ? "Band set to: " + band.toLatin1() : QByteArray("error"); }
                c->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size())
                         + "\r\nConnection: close\r\n\r\n" + body);
                c->disconnectFromHost();
            });
        });
    }
    QStringList take() { QMutexLocker l(&lock); QStringList p = paths; paths.clear(); return p; }
};

static QVariant dbValue(const QString &sql)
{
    QSqlDatabase d = QSqlDatabase::contains("uitest") ? QSqlDatabase::database("uitest")
                                                       : QSqlDatabase::addDatabase("QMYSQL", "uitest");
    d.setHostName("127.0.0.1");
    d.setUserName("admin");
    d.setPassword("wspr");
    if (!d.isOpen())
        d.open();
    QSqlQuery q(d);
    if (!q.exec(sql) || !q.next())
        return {};
    return q.value(0);
}

int main(int argc, char **argv)
{
    qputenv("WSPRSKED_SUN_URL", "http://127.0.0.1:9/v2");   // never the real sunrise-sunset.org from tests
    QTemporaryDir remoteDir;                                 // remote.ini and the certificate, not ~/WSPRSkedRemote
    qputenv("WSPRSKED_REMOTE_DIR", remoteDir.path().toUtf8());
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("G4GCI-test");
    QCoreApplication::setApplicationName("WSPR Sked test");
    applyAppFont();   // as main.cpp

    SlotStore probe;
    if (!probe.loadConfig()) {
        std::printf("mainwindow_uitest: no scratch database; skipped\n");
        return 77;
    }

    QStringList answered;
    bool sayNo = false;   // answer the next Yes/No question with No
    QTimer auto_;
    QObject::connect(&auto_, &QTimer::timeout, [&] {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            auto *d = qobject_cast<QDialog *>(w);
            if (!d || !d->isVisible() || d->objectName() != "MessageDialog") continue;
            if (auto *l = d->findChild<QLabel *>()) answered << l->text().split('\n').first();
            bool done = false;
            if (sayNo)
                for (QPushButton *b : d->findChildren<QPushButton *>())
                    if (b->text() == "No") { sayNo = false; b->click(); done = true; break; }
            if (!done)
                for (QPushButton *b : d->findChildren<QPushButton *>())
                    if (b->text() == "Yes" || b->text() == "OK") { b->click(); break; }
        }
    });
    auto_.start(30);
    auto pump = [&](int ms = 100) { QEventLoop l; QTimer::singleShot(ms, &l, &QEventLoop::quit); l.exec(); };

    {   // database not reachable at start-up: the LoadError message in the window (C#: LoadError form)
        QSettings().setValue("db/password", "wrong-password");
        MainWindow bad;
        bad.show();
        pump(500);
        auto *le = bad.findChild<QLabel *>("loaderrorlabel");
        CHECK(le && le->isVisible() && le->text().startsWith("Error whilst loading!")
              && le->text().contains("Then re-run WSPR Scheduler.") && le->text().contains("Access denied")
              && !le->text().contains("keeps trying"));
        bad.grab().save("sked_load_error.png");
        QSettings().remove("db/password");
        for (QWidget *tl : QApplication::topLevelWidgets())
            if (tl != &bad && tl->windowTitle() == "Transmissions received by this station") tl->hide();
        sayNo = false;
        bad.setAttribute(Qt::WA_DontShowOnScreen);
    }
    MainWindow w;
    w.show();
    pump(500);
    auto list = [&](const char *n) { return w.findChild<QListWidget *>(n); };
    auto btn = [&](const char *n) { return w.findChild<QPushButton *>(n); };
    auto edit = [&](const char *n) { return w.findChild<QLineEdit *>(n); };
    auto combo = [&](const char *n) { return w.findChild<QComboBox *>(n); };
    auto lab = [&](const char *n) { return w.findChild<QLabel *>(n)->text(); };
    auto *tabs = w.findChild<QTabWidget *>("tabControl1");
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i).startsWith("Freq"))
            tabs->setCurrentIndex(i);
    pump(100);

    // ---- help (F1 / "F1 Help"): wspr_sked2_help.rtf in a window of its own ----
    {
        // a copy next to the program is the fallback; C:\WSPR_Sked (here ~/WSPR_Sked) comes first
        const QString helpPath = QCoreApplication::applicationDirPath() + "/wspr_sked2_help.rtf";
        const QString dataHelp = QDir(Rx::defaultDataDir()).filePath("wspr_sked2_help.rtf");
        QFile::remove(helpPath);
        QFile::remove(dataHelp);
        answered.clear();
        w.showHelp();
        pump(100);
        CHECK(answered.filter("wspr_sked2_help.rtf not found in").size() == 1);
        QFile hf(helpPath);
        hf.open(QIODevice::WriteOnly);
        hf.write("{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}{\\b WSPR Scheduler help}\\par Press F1 for this.\\par}");
        hf.close();
        bool f1 = false;
        for (QShortcut *sc : w.findChildren<QShortcut *>())
            if (sc->key() == QKeySequence(Qt::Key_F1)) f1 = true;
        CHECK(f1);
        auto *helpLabel = w.findChild<QLabel *>("label145");
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(2, 2), helpLabel->mapToGlobal(QPointF(2, 2)), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(2, 2), helpLabel->mapToGlobal(QPointF(2, 2)), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(helpLabel, &press);
        QApplication::sendEvent(helpLabel, &release);
        pump(100);
        QWidget *helpWin = nullptr;
        for (QWidget *tl : QApplication::topLevelWidgets())
            if (tl->objectName() == "helpWindow") helpWin = tl;
        CHECK(helpWin && helpWin->isVisible() && helpWin->windowTitle() == "WSPR Scheduler 2 - Help");
        if (helpWin) {
            auto *tb = helpWin->findChild<QTextBrowser *>("helpText");
            CHECK(tb && tb->toPlainText().contains("WSPR Scheduler help") && tb->toPlainText().contains("Press F1 for this."));
            CHECK(!helpWin->isAncestorOf(&w) && helpWin->parentWidget() == nullptr);   // separate window
            helpWin->close();
        }
        w.showHelp();   // opens again, same window
        pump(50);
        int helpWins = 0;
        for (QWidget *tl : QApplication::topLevelWidgets())
            if (tl->objectName() == "helpWindow") ++helpWins;
        CHECK(helpWins == 1 && helpWin && helpWin->isVisible());
        // the copy in the data folder wins over the one next to the program
        QDir().mkpath(Rx::defaultDataDir());
        QFile df(dataHelp);
        df.open(QIODevice::WriteOnly);
        df.write("{\\rtf1 Data folder copy\\par}");
        df.close();
        w.showHelp();
        pump(50);
        if (helpWin) {
            CHECK(helpWin->findChild<QTextBrowser *>("helpText")->toPlainText().contains("Data folder copy"));
            helpWin->close();
        }
        QFile::remove(helpPath);
        QFile::remove(dataHelp);
    }

    // ---- antennas from the database ----
    auto *ants = list("AntlistBox");
    CHECK(ants->count() == 3);
    CHECK(ants->count() == 3 && ants->item(1)->text().simplified() == "Vertical 1 0 1");
    CHECK(combo("AntselcomboBox")->count() == 3 && combo("DefaultAntcomboBox")->count() == 3);

    // add: switch 2, port "4", second switch 1, port "2", tuner 3
    btn("AddAbutton")->click();
    CHECK(w.findChild<QGroupBox *>("AntgroupBox")->isVisible() && !ants->isEnabled());
    CHECK(lab("antNolabel") == "4");   // next free number (C# used the list length: 3, already taken)
    edit("AntnametextBox")->setText("Beverage");
    w.findChild<QPlainTextEdit *>("AntdesctextBox")->setPlainText("RX only, 200 m");
    list("ShowSwlistBox")->setCurrentRow(2);
    CHECK(list("AntPortlistBox")->isVisible());
    list("AntPortlistBox")->setCurrentRow(3);
    list("ShowSwlistBox2")->setCurrentRow(1);
    list("AntPortlistBox2")->setCurrentRow(1);
    list("ShowTulistBox")->setCurrentRow(3);
    btn("SaveAbutton")->click();
    pump(100);
    CHECK(ants->count() == 4 && ants->isEnabled() && !w.findChild<QGroupBox *>("AntgroupBox")->isVisible());
    CHECK(dbValue("SELECT CONCAT(Antenna,'|',Switch,'|',SwitchPort,'|',Switch2,'|',SwitchPort2,'|',Tuner) "
                  "FROM wspr.antennas WHERE AntNo = 4").toString() == "Beverage|2|3|1|1|3");
    CHECK(combo("AntselcomboBox")->findText("Beverage") >= 0 && combo("DefaultAntcomboBox")->findText("Beverage") >= 0);

    // a duplicate name is refused
    btn("AddAbutton")->click();
    edit("AntnametextBox")->setText("beverage");
    btn("SaveAbutton")->click();
    pump(100);
    CHECK(answered.contains("Antenna already exists") && ants->count() == 4);
    btn("CancelAbutton")->click();

    // edit: rename with an apostrophe, slots from today follow
    const QString today = QDateTime::currentDateTimeUtc().date().toString("yyyy-MM-dd");
    dbValue("REPLACE INTO wspr_slots.slots (`date`,`time`,`frequency`,`offset`,`power`,`powerw`,`antenna`,`tuner`,"
            "`switch`,`switchport`,`rotator`,`azimuth`,`elevation`,`end`,`active`,`repeating`,`timeend`,`rpttime`,"
            "`parent`,`audio`,`slotno`,`msgtype`,`rpttype`,`greyoffset`,`switch2`,`switchport2`) VALUES ('" + today
            + "','23:58:00',14.0956,50,23,0.2,'Beverage',0,2,3,'',0,0,NULL,1,0,'00:00:00',0,'',100,1,1,0,0,0,0)");
    ants->setCurrentRow(3);
    btn("EditAbutton")->click();
    CHECK(edit("AntnametextBox")->text() == "Beverage" && list("ShowSwlistBox")->currentRow() == 2
          && list("AntPortlistBox")->currentRow() == 3 && list("ShowTulistBox")->currentRow() == 3);
    edit("AntnametextBox")->setText("Neville's Beverage");
    btn("SaveAbutton")->click();
    pump(200);
    CHECK(answered.contains("Update all slots from now with new name (Y/N)?"));
    CHECK(dbValue("SELECT Antenna FROM wspr.antennas WHERE AntNo = 4").toString() == "Neville's Beverage");
    CHECK(dbValue("SELECT antenna FROM wspr_slots.slots WHERE date = '" + today + "' AND time = '23:58:00'").toString()
          == "Neville's Beverage");
    CHECK(combo("AntselcomboBox")->findText("Neville's Beverage") >= 0 && combo("AntselcomboBox")->findText("Beverage") < 0);
    dbValue("DELETE FROM wspr_slots.slots WHERE date = '" + today + "' AND time = '23:58:00'");

    // delete: gone from the list, the database and both drop-downs (C# left it in the drop-downs)
    ants->setCurrentRow(3);
    btn("DelAbutton")->click();
    pump(100);
    CHECK(ants->count() == 3 && dbValue("SELECT COUNT(*) FROM wspr.antennas WHERE AntNo = 4").toInt() == 0);
    CHECK(combo("AntselcomboBox")->findText("Neville's Beverage") < 0
          && combo("DefaultAntcomboBox")->findText("Neville's Beverage") < 0);

    // ---- frequencies and TX audio level ----
    auto *fl = list("FreqlistBox");
    auto row = [&](const QString &prefix) {
        for (int i = 0; i < fl->count(); ++i)
            if (fl->item(i)->text().simplified().section(' ', 0, 0) == prefix)
                return i;
        return -1;
    };
    CHECK(row("14.0956") >= 0 && fl->item(row("14.0956"))->text().simplified() == "14.0956 0.8");
    CHECK(row("10.1402") >= 0);                 // added frequency shown again (C# dropped it)
    CHECK(row("10.1387") >= 0 && fl->item(row("10.1387"))->text().simplified() == "10.1387 1");
    CHECK(std::abs(w.txLevelFor(14.0956) - 0.8) < 1e-9 && w.txLevelFor(99.0) == 1.0);
    CHECK(combo("FreqcomboBox")->findText("10.1402") >= 0 && list("FlistBox")->findItems("10.1402", Qt::MatchExactly).size() == 1);

    fl->setCurrentRow(row("14.0956"));
    btn("editFbutton")->click();
    auto *slider = w.findChild<QSlider *>("trackBar1");
    CHECK(w.findChild<QGroupBox *>("FreqgroupBox")->isVisible() && slider->value() == 80 && lab("Levellabel") == "0.8");
    slider->setValue(55);
    CHECK(lab("Levellabel") == "0.55" && std::abs(w.txVolume() - 0.55f) < 1e-6);
    btn("Fsavebutton")->click();
    pump(100);
    CHECK(fl->item(row("14.0956"))->text().simplified() == "14.0956 0.55");
    CHECK(std::abs(dbValue("SELECT AudioLevel FROM wspr.frequencies WHERE ABS(Frequency - 14.0956) < 1e-7").toDouble() - 0.55) < 1e-9);
    CHECK(std::abs(w.txLevelFor(14.0956) - 0.55) < 1e-9);

    fl->setCurrentRow(row("13.5539"));          // 22m: receive only
    btn("editFbutton")->click();
    CHECK(!slider->isEnabled() && lab("RXonlylabel") == "RX only on this band");
    btn("Fcancelbutton")->click();
    CHECK(fl->isEnabled() && !w.findChild<QGroupBox *>("FreqgroupBox")->isVisible());

    // restore the fixture value
    dbValue("UPDATE wspr.frequencies SET AudioLevel = 0.8 WHERE ABS(Frequency - 14.0956) < 1e-7");

    // ---- switches, tuners, filters ----
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i).startsWith("Switches"))
            tabs->setCurrentIndex(i);
    pump(50);
    auto *swl = list("SwlistBox");
    CHECK(swl->item(1)->text() == "1\tArduino shack" && swl->item(2)->text() == "2\tW410A loft" && swl->item(3)->text() == "3\tswitch3");
    CHECK(list("ShowSwlistBox")->item(2)->text() == "2\tW410A loft" && list("ShowSwlistBox2")->item(1)->text() == "1\tArduino shack");
    CHECK(list("FilistBox")->item(1)->text() == "1\tLPF board");
    CHECK(w.switches().size() == 11 && w.switches()[1].ip == "192.168.1.60" && w.switches()[1].channels == 6);

    swl->setCurrentRow(0);                       // "No switch" can't be edited
    btn("editSwbutton")->click();
    pump(50);
    CHECK(answered.contains("Cannot edit \"No switch\"") && !w.findChild<QGroupBox *>("HwgroupBox")->isVisible());

    swl->setCurrentRow(2);                       // the serial W-410A
    btn("editSwbutton")->click();
    pump(50);
    auto *hw = w.findChild<QGroupBox *>("HwgroupBox");
    CHECK(hw->isVisible() && hw->title() == "Switch" && !swl->isEnabled());
    CHECK(combo("HwProtocolcomboBox")->currentText() == "Serial" && combo("HwPortcomboBox")->currentText() == "COM5"
          && combo("HwBaudcomboBox")->currentText() == "9600" && combo("HwParitycomboBox")->currentText() == "none");
    CHECK(list("HwSwTypelistBox")->currentRow() == 1 && edit("HwAntportstextBox")->text() == "4");
    edit("HwAntportstextBox")->setText("6");
    combo("HwBaudcomboBox")->setCurrentText("19200");
    btn("HwSavebutton")->click();
    pump(100);
    CHECK(!hw->isVisible() && swl->isEnabled());
    CHECK(dbValue("SELECT CONCAT(Name,'|',Protocol,'|',Port,'|',Baud,'|',Serial,'|',Channels) FROM wspr.switches WHERE Id = 2")
              .toString() == "W410A loft|Serial|COM5|19200|8,none,1,none|6");

    // a new TCP tuner in row 1
    list("TulistBox")->setCurrentRow(1);
    btn("editTubutton")->click();
    pump(50);
    CHECK(hw->isVisible() && hw->title() == "Tuner" && combo("HwProtocolcomboBox")->currentIndex() == -1);
    edit("HwtextBox")->setText("ATU");
    combo("HwProtocolcomboBox")->setCurrentText("TCP");
    edit("HwIPtextBox")->setText("192.168.1.70");
    edit("HwPorttextBox")->setText("5000");
    btn("HwSavebutton")->click();             // no tuner type chosen yet
    pump(50);
    CHECK(answered.contains("You must select and highlight a tuner type") && hw->isVisible());
    list("HwTuTypelistBox")->setCurrentRow(0);
    edit("HwIPtextBox")->setText("300.1.1.1");
    btn("HwSavebutton")->click();
    pump(50);
    CHECK(answered.contains("Invalid IP or port") && hw->isVisible());
    edit("HwIPtextBox")->setText("192.168.1.70");
    btn("HwSavebutton")->click();
    pump(100);
    CHECK(!hw->isVisible() && list("TulistBox")->item(1)->text() == "1\tATU" && list("ShowTulistBox")->item(1)->text() == "1\tATU");
    CHECK(dbValue("SELECT CONCAT(Name,'|',Protocol,'|',IP,'|',Port,'|',Type) FROM wspr.tuners WHERE Id = 1").toString()
          == "ATU|TCP|192.168.1.70|5000|Arduino - single action on data pin 1");

    // filter in use: chosen with a click, saved straight away
    list("FilistBox")->setCurrentRow(1);
    emit list("FilistBox")->itemClicked(list("FilistBox")->item(1));
    pump(100);
    CHECK(w.selectedFilter() == 1 && edit("selFiltertextBox")->text() == "1\tLPF board");
    CHECK(dbValue("SELECT selectedFilter FROM wspr_configs.settings LIMIT 1").toInt() == 1);
    dbValue("UPDATE wspr_configs.settings SET selectedFilter = 0");

    // screenshots of every tab at the new font size
    for (int i = 0; i < tabs->count(); ++i) {
        tabs->setCurrentIndex(i);
        pump(50);
        w.grab().save(QString("tab_%1.png").arg(i + 1));
    }


    // ---- transmit: a second window against a fake switch board and Hamlib's dummy rig ----
    w.close();
    pump(200);
    if (QStandardPaths::findExecutable("rigctld").isEmpty()) {
        std::printf("rigctld not installed - transmit part skipped\n");
    } else {
        QThread boardThread;
        FakeBoard board;
        board.moveToThread(&boardThread);
        boardThread.start();
        QMetaObject::invokeMethod(&board, [&] { board.start(); }, Qt::BlockingQueuedConnection);
        const int rigPort = 45330;
        auto startDummy = [&] {
            auto *p = new QProcess;
            p->start(QStandardPaths::findExecutable("rigctld"), {"-m", "1", "-P", "RIG", "-t", QString::number(rigPort)});
            p->waitForStarted();
            for (int i = 0; i < 50 && !RigCtl("127.0.0.1", rigPort).isReachable(); ++i) QThread::msleep(100);
            return p;
        };
        QProcess *dummy = startDummy();
        RigCtl probeRig("127.0.0.1", rigPort);

        dbValue(QString("UPDATE wspr.switches SET IP = '127.0.0.1', Port = '%1' WHERE Id = 1").arg(board.server->serverPort()));
        dbValue(QString("UPDATE wspr.rigctl SET IPv4 = '127.0.0.1', Port = '%1', norigctld = 0").arg(rigPort));
        dbValue("UPDATE wspr_configs.settings SET selectedFilter = 0");
        dbValue("REPLACE INTO wspr.antennas (AntNo, Antenna, Description, Switch, Tuner, SwitchPort, Switch2, SwitchPort2) "
                "VALUES (9, 'TXtest', 'transmit test', 1, 0, 3, 0, 0)");
        dbValue("DELETE FROM wspr_slots.slots WHERE `date` = '2030-01-01'");
        const QString cols = "(`date`,`time`,`frequency`,`offset`,`power`,`powerw`,`antenna`,`tuner`,`switch`,`switchport`,"
                             "`end`,`active`,`repeating`,`timeend`,`rpttime`,`parent`,`slotno`,`msgtype`,`rpttype`,`greyoffset`,"
                             "`rotator`,`azimuth`,`elevation`,`audio`)";
        dbValue("INSERT INTO wspr_slots.slots " + cols + " VALUES ('2030-01-01','10:02:00',14.0956,50,23,0.2,'TXtest',0,0,0,"
                "'2030-01-01',1,0,'00:00:00',0,'',1,1,0,0,'',0,0,0)");
        dbValue("INSERT INTO wspr_slots.slots " + cols + " VALUES ('2030-01-01','10:06:00',7.0386,50,23,0.2,'TXtest',0,0,0,"
                "'2030-01-01',0,0,'00:00:00',0,'',1,1,0,0,'',0,0,0)");
        QSettings().setValue("tx/dryRun", true);

        MainWindow t;
        t.show();
        pump(500);
        for (QTimer *tm : t.findChildren<QTimer *>()) tm->stop();   // the test supplies the clock
        for (QTimer *tm : w.findChildren<QTimer *>()) tm->stop();
        auto *dry = t.findChild<QCheckBox *>("dryRuncheckBox");
        auto *enable = t.findChild<QCheckBox *>("enableTXcheckBox");
        auto *vox = t.findChild<QCheckBox *>("VoxcheckBox");
        auto *runBtn = t.findChild<QPushButton *>("TXrunbutton");
        auto at = [](int m, int s) { return QDateTime(QDate(2030, 1, 1), QTime(10, m, s), QTimeZone::utc()); };
        auto cycle = [&](int slotMin) {   // look-up, prepare, fire for the slot at 10:slotMin
            t.txTick(at(slotMin - 2, 5));   // even :05 clears the once-per-cycle flags
            t.txTick(at(slotMin - 1, 54));
            t.txTick(at(slotMin - 1, 55));
            t.txTick(at(slotMin, 0));
        };
        {   // RX gain slider starts at the saved level (it always came up at 1 = 0.01)
            const QVariant saved = dbValue("SELECT inlevel FROM wspr_configs.rxsettings LIMIT 1");
            if (saved.isValid())
                CHECK(t.findChild<QSlider *>("trackBarGain")->value() == qBound(1, saved.toInt(), 500));
        }
        {   // Gaussian alpha: from the settings, saved when changed
            auto *alpha = t.findChild<QComboBox *>("AlphacomboBox");
            CHECK(alpha->currentText() == "0.1" && !alpha->isEditable());
            alpha->setCurrentIndex(alpha->findText("0.05"));
            CHECK(std::abs(dbValue("SELECT Alpha FROM wspr_configs.settings LIMIT 1").toDouble() - 0.05) < 1e-9);
            alpha->setCurrentIndex(alpha->findText("0.1"));
            CHECK(std::abs(dbValue("SELECT Alpha FROM wspr_configs.settings LIMIT 1").toDouble() - 0.1) < 1e-9);
        }
        CHECK(MainWindow::radioDisplayName("  1046  Yaesu   FT-450D   20230328.2  Stable  RIG_MODEL_FT450")
              == "Yaesu FT-450D (Hamlib model 1046)");
        CHECK(MainWindow::radioDisplayName("1  Hamlib  Dummy  20230328.0  Stable  RIG_MODEL_DUMMY") == "Hamlib Dummy (Hamlib model 1)");
        {
            auto *r = t.findChild<QLabel *>("label31");
            auto *stop = t.findChild<QPushButton *>("stopRigCtlDbutton");
            CHECK(r->text().startsWith("Radio to use: ") && r->x() == stop->x() && r->y() > stop->geometry().bottom());
            std::printf("     %s\n", qPrintable(r->text()));
        }
        {   // TX Configuration defaults: watts shown, used by a new slot at once, saved by Save Settings
            auto *pwr = t.findChild<QComboBox *>("defaultpwrcomboBox");
            pwr->setCurrentIndex(pwr->findText("30"));
            CHECK(t.findChild<QLineEdit *>("defaultWtextBox")->text() == "1");
            pwr->setCurrentIndex(pwr->findText("37"));
            CHECK(t.findChild<QLineEdit *>("defaultWtextBox")->text() == "5");
            t.findChild<QLineEdit *>("defaultOfftextBox")->setText("77");
            auto *defF = t.findChild<QComboBox *>("defaultFcomboBox");
            defF->setCurrentIndex(defF->findText("7.0386"));
            t.findChild<QCalendarWidget *>("monthCalendar1")->setSelectedDate(QDate::currentDate().addDays(1));
            t.findChild<QListWidget *>("timelistBox")->setCurrentRow(5);
            pump(100);
            auto *grid = t.findChild<QTableView *>("dataGridView1");
            emit grid->doubleClicked(grid->model()->index(2, 1));
            pump(100);
            CHECK(t.findChild<QGroupBox *>("slotgroupBox")->isVisible());
            CHECK(t.findChild<QLineEdit *>("OffsettextBox")->text() == "77");
            CHECK(t.findChild<QComboBox *>("dBmcomboBox")->currentText() == "37"
                  && t.findChild<QLineEdit *>("PowertextBox")->text() == "5");
            CHECK(t.findChild<QComboBox *>("FreqcomboBox")->currentText().startsWith("7.0386"));
            t.findChild<QPushButton *>("CancelSlotbutton")->click();
            pump(50);
            auto *callBox = t.findChild<QLineEdit *>("CalltextBox");
            callBox->setText("g4gci/p");                           // as if typed: capitals, base call follows
            emit callBox->textEdited(callBox->text());
            CHECK(callBox->text() == "G4GCI/P" && t.findChild<QLineEdit *>("baseCalltextBox")->text() == "G4GCI");
            callBox->setText("G4GCI");
            t.findChild<QPushButton *>("saveConfigbutton")->click();
            pump(100);
            CHECK(answered.contains("Settings saved"));
            CHECK(dbValue("SELECT CONCAT(`Offset`,'|',Power,'|',DefaultF,'|',PowerW) FROM wspr_configs.settings LIMIT 1").toString()
                  == "77|37|7.0386|5");
        }
        {   // database password: *** until Show is ticked; editing shows the re-enter box
            auto *pass = t.findChild<QLineEdit *>("PasstextBox");
            auto *show = t.findChild<QCheckBox *>("showPasscheckBox");
            CHECK(pass->echoMode() == QLineEdit::Password && pass->text() == "wspr"
                  && !t.findChild<QLineEdit *>("PasstextBox2")->isVisible());
            show->setChecked(true);
            CHECK(pass->echoMode() == QLineEdit::Normal);
            show->setChecked(false);
            CHECK(pass->echoMode() == QLineEdit::Password);
            for (int i = 0; i < t.findChild<QTabWidget *>("tabControl1")->count(); ++i)
                if (t.findChild<QTabWidget *>("tabControl1")->tabText(i).startsWith("TX Conf"))
                    t.findChild<QTabWidget *>("tabControl1")->setCurrentIndex(i);
            pass->setText("wspr2");
            CHECK(t.findChild<QLineEdit *>("PasstextBox2")->isVisible() && t.findChild<QLabel *>("Passlabel2")->isVisible());
            auto *pass2 = t.findChild<QLineEdit *>("PasstextBox2");
            pass->setText("ab"); pass2->setText("ab");
            t.findChild<QPushButton *>("savePasswordbutton")->click();
            pump(100);
            CHECK(answered.contains("Password is either blank or too short"));
            pass->setText("abcd1"); pass2->setText("abcd2");
            t.findChild<QPushButton *>("savePasswordbutton")->click();
            pump(100);
            CHECK(answered.contains("Passwords do not match"));
            pass->setText("wspr"); pass2->setText("wspr");
            t.findChild<QPushButton *>("savePasswordbutton")->click();
            pump(100);
            CHECK(answered.contains("That is already the database password"));
            const QSignalBlocker b(pass);
            pass->setText("wspr");
        }
        {   // "Send type 2/3 as one tx'n" shows only with type 2/3 allowed
            auto *t2 = t.findChild<QCheckBox *>("Type2checkBox");
            auto *one = t.findChild<QCheckBox *>("asOnecheckBox");
            t2->setChecked(true);
            CHECK(!one->isHidden() && t.findChild<QLabel *>("msgTypelabel")->text() == "Message type 1 or 2 or 3");
            t2->setChecked(false);
            CHECK(one->isHidden() && !t.findChild<QCheckBox *>("longcheckBox")->isChecked());
        }
        {   // Test encoding: type decided by call/locator/settings; slot list only for two transmissions
            auto *sl = t.findChild<QListWidget *>("slotlistBox");
            auto *out = t.findChild<QPlainTextEdit *>("leveltextBox");
            auto *t2 = t.findChild<QCheckBox *>("Type2checkBox");
            auto *one = t.findChild<QCheckBox *>("asOnecheckBox");
            CHECK(t.findChild<QListWidget *>("slotTestlistBox")->isHidden());
            t2->setChecked(false);
            CHECK(sl->isHidden());
            t.findChild<QPushButton *>("testEncodebutton")->click();
            CHECK(out->toPlainText().startsWith("Type 1, slot 1: G4GCI IO90 "));
            CHECK(out->toPlainText().section("\n\n", 1).remove('\n').size() == 162);
            CHECK(out->toPlainText().section('\n', 1, 1).startsWith("Type 1 (standard): callsign G4GCI, locator IO90"));
            t2->setChecked(true);
            t.findChild<QCheckBox *>("longcheckBox")->setChecked(true);
            one->setChecked(false);
            CHECK(!sl->isHidden() && sl->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff);
            sl->setCurrentRow(1);
            t.findChild<QPushButton *>("testEncodebutton")->click();
            CHECK(out->toPlainText().startsWith("Type 3, slot 2: <G4GCI> IO90GU "));
            std::printf("     %s\n", qPrintable(out->toPlainText().section('\n', 1, 1)));
            one->setChecked(true);
            CHECK(sl->isHidden());
            t.findChild<QPushButton *>("testEncodebutton")->click();
            CHECK(out->toPlainText().startsWith("Type 1, slot 1: G4GCI IO90"));   // plain call: one type 1
            t2->setChecked(false);
        }
        CHECK(t.windowTitle().contains("V.0.2.3") && t.windowTitle().contains("GNU GPLv3"));
        CHECK(dry && dry->isChecked() && t.isDryRun());
        enable->setChecked(true);
        CHECK(t.findChild<QPushButton *>("TXenablebutton")->text() == "TX on (dry run)");
        probeRig.setFrequency(3568600);
        probeRig.setPtt(false);
        board.take();

        // dry run with VOX: antenna and frequency change, no PTT and no audio
        vox->setChecked(true);
        t.txTick(at(0, 5));
        t.txTick(at(1, 54));
        CHECK(runBtn->text() == "Dry run next 14.0956 MHz");
        auto countdown = [&] { return t.findChild<QLabel *>("countdownlabel")->text(); };
        CHECK(countdown() == "TX in 6 seconds");
        t.txTick(at(1, 55));
        CHECK(board.take() == QStringList{"set-pin 3 high"});
        CHECK(probeRig.frequency().value_or(0) == 14095600);
        CHECK(t.findChild<QLabel *>("TXRXAntlabel")->text() == "TXtest");
        t.txTick(at(2, 0));
        CHECK(t.isTransmitting() && runBtn->text() == "DRY RUN 14.0956 MHz" && countdown() == "TX time: 0");
        t.txTick(at(2, 37));
        CHECK(countdown() == "TX time: 37");
        CHECK(probeRig.ptt() == false);
        t.findChild<QPushButton *>("stopTestbutton")->click();
        CHECK(!t.isTransmitting() && probeRig.ptt() == false);

        // real transmission: PTT on at the slot, off when stopped
        dry->setChecked(false);
        vox->setChecked(false);
        CHECK(t.findChild<QPushButton *>("TXenablebutton")->text() == "TX Enabled");
        cycle(2);
        CHECK(t.isTransmitting() && runBtn->text() == "TX 14.0956 MHz" && probeRig.ptt() == true);
        CHECK(t.findChild<QPushButton *>("TXrunbutton2")->text() == runBtn->text());
        runBtn->click();                              // the TX/RX button on the main page stops TX
        CHECK(!t.isTransmitting() && probeRig.ptt() == false && runBtn->text() == "RX 14.0956 MHz"
              && runBtn->styleSheet().contains("#808000"));   // olive: receiving, no slot
        cycle(2);
        CHECK(t.isTransmitting() && probeRig.ptt() == true);
        t.findChild<QPushButton *>("stopTestbutton")->click();
        CHECK(!t.isTransmitting() && probeRig.ptt() == false);

        // "wider tones" test: audio only, never PTT (C#), even with dry run off
        t.findChild<QCheckBox *>("testTonescheckBox")->setChecked(true);
        cycle(2);
        CHECK(t.isTransmitting() && probeRig.ptt() == false);
        t.findChild<QPushButton *>("stopTestbutton")->click();
        CHECK(!t.findChild<QCheckBox *>("testTonescheckBox")->isChecked());   // one transmission only

        // unticking Enable TX during a transmission stops it
        cycle(2);
        CHECK(probeRig.ptt() == true);
        enable->setChecked(false);
        CHECK(!t.isTransmitting() && probeRig.ptt() == false);
        enable->setChecked(true);

        // schedule off: label on the main page, no countdown, no slot read
        auto *noSked = t.findChild<QCheckBox *>("noSkedcheckBox");
        CHECK(t.findChild<QLabel *>("skedstoplabel")->text() == "Sked enabled");
        noSked->setChecked(true);
        CHECK(t.findChild<QLabel *>("skedstoplabel")->text() == "Sked disabled");
        t.txTick(at(4, 10));
        CHECK(countdown().isEmpty());
        noSked->setChecked(false);
        CHECK(t.findChild<QLabel *>("skedstoplabel")->text() == "Sked enabled");

        // inactive slot: frequency and antenna change for receive, no TX
        t.txTick(at(4, 10));                          // the schedule is read early for the countdown
        CHECK(countdown() == "RX in: 110 seconds");
        t.txTick(at(4, 11));
        CHECK(countdown() == "RX in: 109 seconds");
        board.take();
        cycle(6);
        CHECK(probeRig.frequency().value_or(0) == 7038600 && board.take() == QStringList{"set-pin 3 high"});
        CHECK(!t.isTransmitting() && probeRig.ptt() == false && runBtn->text() == "RX 7.0386 MHz"
              && runBtn->styleSheet().contains("#4169E1"));   // blue: receive slot
        CHECK(countdown() == "RX time: 0");
        t.txTick(at(7, 50));
        CHECK(countdown() == "RX time: 110");
        t.txTick(at(7, 52));
        CHECK(countdown().isEmpty());                 // nothing scheduled at 10:08

        // frequency correction: the rig is tuned 0.85 ppm high
        auto *ppm = t.findChild<QDoubleSpinBox *>("ppmspinBox");
        CHECK(ppm && ppm->value() == 0.0);
        ppm->setValue(0.85);
        cycle(2);
        CHECK(probeRig.frequency().value_or(0) == 14095612 && t.isTransmitting());
        CHECK(t.statusBar()->currentMessage().contains("TX") );
        t.findChild<QPushButton *>("stopTestbutton")->click();
        ppm->setValue(0.0);

        // ---- Test TX buttons (default antenna is Windom: switch 1, port 1 -> pin 0) ----
        auto *flist = t.findChild<QListWidget *>("FlistBox");
        const auto f20 = flist->findItems("14.0956", Qt::MatchStartsWith);
        CHECK(!f20.isEmpty());
        probeRig.setFrequency(3568600);
        flist->setCurrentItem(f20.first());           // clicking a frequency tunes the rig at once
        CHECK(t.findChild<QLineEdit *>("testFtextBox")->text() == "14.0956");
        CHECK(probeRig.frequency().value_or(0) == 14095600);
        board.take();
        auto *defAnt = t.findChild<QComboBox *>("DefaultAntcomboBox");
        const int windom = defAnt->findText("Windom");
        defAnt->setCurrentIndex(windom);
        emit defAnt->activated(windom);               // choosing a default antenna switches to it
        CHECK(board.take() == QStringList{"set-pin 0 high"} && t.findChild<QLabel *>("TXRXAntlabel")->text() == "Windom");

        // "Test TX/PTT": switch, tune and key straight away
        probeRig.setFrequency(3568600);
        t.findChild<QPushButton *>("TestTXbutton")->click();
        CHECK(t.isTransmitting() && probeRig.ptt() == true && probeRig.frequency().value_or(0) == 14095600);
        CHECK(board.take() == QStringList{"set-pin 0 high"});
        t.findChild<QPushButton *>("stopTestbutton")->click();
        CHECK(!t.isTransmitting() && probeRig.ptt() == false);

        // "Test wspr": audio only, never PTT; refused with VOX ticked
        t.findChild<QPushButton *>("testButton")->click();
        CHECK(t.isTransmitting() && probeRig.ptt() == false && board.take().isEmpty());
        t.findChild<QPushButton *>("stopTestbutton")->click();
        vox->setChecked(true);
        t.findChild<QPushButton *>("testButton")->click();
        CHECK(!t.isTransmitting());
        vox->setChecked(false);

        // "Wspr TX Test": queued for the next even minute, then a normal slot
        const QDateTime now = QDateTime::currentDateTimeUtc();
        t.findChild<QPushButton *>("TXTestbutton")->click();
        QDateTime c = TxPlan::nextCycleStart(now);
        if (now.secsTo(c) < 4) c = c.addSecs(120);
        CHECK(!t.isTransmitting() && t.statusBar()->currentMessage().contains("Wspr TX Test queued for " + c.toString("HH:mm")));
        probeRig.setFrequency(3568600);
        t.txTick(c.addSecs(-6));     // odd :54 - the schedule look-up must not replace the test
        t.txTick(c.addSecs(-5));
        CHECK(probeRig.frequency().value_or(0) == 14095600 && board.take() == QStringList{"set-pin 0 high"});
        t.txTick(c);
        CHECK(t.isTransmitting() && probeRig.ptt() == true && runBtn->text() == "TX test 14.0956 MHz");
        t.findChild<QPushButton *>("stopTestbutton")->click();
        CHECK(!t.isTransmitting() && probeRig.ptt() == false);

        // 22 m is receive only
        t.findChild<QLineEdit *>("testFtextBox")->setText("13.5539");
        t.findChild<QPushButton *>("TestTXbutton")->click();
        pump(100);
        CHECK(!t.isTransmitting() && answered.contains("RX only on 22 or 8m"));

        // Test Arduino: toggles the pin, reports only what the board confirms
        {
            board.take();
            t.findChild<QLineEdit *>("ArduinoIPtextBox")->setText("127.0.0.1");
            t.findChild<QLineEdit *>("ArduinoPorttextBox")->setText(QString::number(board.server->serverPort()));
            t.findChild<QLineEdit *>("pintextBox")->setText("3");
            auto *testBtn = t.findChild<QPushButton *>("button1");
            auto waitFor = [&](auto cond) { for (int i = 0; i < 60 && !cond(); ++i) pump(50); };
            testBtn->click();
            waitFor([&] { return testBtn->isEnabled(); });
            CHECK(board.take() == QStringList{"set-pin 3 high"} && t.findChild<QLabel *>("ardReplylabel")->text() == "Test ok!"
                  && t.findChild<QLabel *>("HiLolabel")->text() == "Pin 3 high");
            CHECK(t.findChild<QLineEdit *>("ArduinoPorttextBox")->text() == QString::number(board.server->serverPort()));
            testBtn->click();
            waitFor([&] { return testBtn->isEnabled(); });
            CHECK(board.take() == QStringList{"set-pin 3 low"} && t.findChild<QLabel *>("HiLolabel")->text() == "Pin 3 low");
            t.findChild<QLineEdit *>("pintextBox")->clear();
            answered.clear();
            testBtn->click();
            pump(100);
            CHECK(board.take().isEmpty() && answered.filter("Enter the board's IP address").size() == 1);
        }

        // rigctld connection test
        {
            auto *rt = t.findChild<QPushButton *>("rigTestbutton");
            CHECK(rt != nullptr);
            auto waitFor = [&](auto cond) { for (int i = 0; i < 100 && !cond(); ++i) pump(50); };
            answered.clear();
            rt->click();
            waitFor([&] { return rt->isEnabled() && !answered.isEmpty(); });
            CHECK(answered.filter(QRegularExpression("^rigctld at 127.0.0.1:\\d+ answers and reads the radio")).size() == 1);
            auto *portBox = t.findChild<QLineEdit *>("PorttextBox");
            const QString saved = portBox->text();
            portBox->setText("45399");                    // nothing there
            answered.clear();
            rt->click();
            waitFor([&] { return rt->isEnabled() && !answered.isEmpty(); });
            CHECK(answered.filter("Nothing answers at 127.0.0.1:45399").size() == 1);
            portBox->setText(saved);
        }

        // a switch that doesn't answer cancels the transmission
        const quint16 boardPort = board.server->serverPort();
        QMetaObject::invokeMethod(&board, [&] { board.server->close(); }, Qt::BlockingQueuedConnection);
        cycle(2);
        CHECK(!t.isTransmitting() && probeRig.ptt() == false);
        pump(200);
        CHECK(answered.filter("Antenna switch error").size() >= 1);

        // rigctld gone: slot skipped
        dummy->kill(); dummy->waitForFinished(); delete dummy;
        t.txTick(at(0, 5));
        t.txTick(at(1, 54));
        pump(100);
        CHECK(answered.filter("rigctld not running").size() >= 1);
        t.txTick(at(1, 55));
        t.txTick(at(2, 0));
        CHECK(!t.isTransmitting());
        std::printf("     status: %s\n", qPrintable(t.statusBar()->currentMessage()));

        // rigctld on another PC: address and port editable and saved; nothing started here
        {
            auto *ip = t.findChild<QLineEdit *>("IPtextBox");
            auto *port = t.findChild<QLineEdit *>("PorttextBox");
            CHECK(!ip->isReadOnly() && !port->isReadOnly());
            ip->setText("192.0.2.10");
            port->setText("4533");
            answered.clear();
            t.findChild<QPushButton *>("SaveRigctlbutton")->click();
            pump(300);
            CHECK(dbValue("SELECT CONCAT(IPv4,':',Port) FROM wspr.rigctl LIMIT 1").toString() == "192.0.2.10:4533");
            CHECK(answered.filter("rigctld on 192.0.2.10:4533 is not answering").size() == 1);
            port->setText("70000");                   // not a port number
            CHECK(!port->hasAcceptableInput());
        }

        // Non Rigctl TX: a web-controlled transmitter instead of rigctld
        {
            QThread webThread;
            FakeWeb web;
            web.moveToThread(&webThread);
            webThread.start();
            QMetaObject::invokeMethod(&web, [&] { web.start(); }, Qt::BlockingQueuedConnection);
            QMetaObject::invokeMethod(&board, [&] { board.server->listen(QHostAddress::LocalHost, boardPort); },
                                      Qt::BlockingQueuedConnection);   // the antenna switch is back
            auto *list = t.findChild<QListWidget *>("riglistBox");
            CHECK(list->count() >= 1 && list->item(0)->text() == "0\tNone specified");
            t.findChild<QPushButton *>("addrigbutton")->click();
            CHECK(!t.findChild<QGroupBox *>("RiggroupBox")->isHidden());
            t.findChild<QLineEdit *>("rignametextBox")->setText("Beacon");
            t.findChild<QComboBox *>("rigprotocolcomboBox")->setCurrentText("TCP");
            t.findChild<QLineEdit *>("rigiptextBox")->setText("127.0.0.1");
            t.findChild<QLineEdit *>("rigporttextBox")->setText(QString::number(web.server->serverPort()));
            // the user manual's Arduino Nano + AD9850 WSPR TX settings
            t.findChild<QLineEdit *>("APIURLtextBox")->setText("api/cmd?c=");
            t.findChild<QLineEdit *>("rigtxcmdtextBox")->setText("SET FREQ freq;SET CALL call;SET OFFSET offset;SET LOC grid;SET PWR pwr");
            t.findChild<QLineEdit *>("rigPTTcmdtextBox")->setText("TXNOW");
            t.findChild<QLineEdit *>("rigrxcmdtextBox")->setText("STOP");
            t.findChild<QLineEdit *>("rigCheckReplytextBox")->setText("BAND SET TO");   // any case
            for (int i = 0; i < t.findChild<QTabWidget *>("tabControl1")->count(); ++i)
                if (t.findChild<QTabWidget *>("tabControl1")->tabText(i).startsWith("Non"))
                    t.findChild<QTabWidget *>("tabControl1")->setCurrentIndex(i);
            pump(100);
            t.grab().save("nonrig_tcp.png");
            t.findChild<QPushButton *>("saverigbutton")->click();
            pump(100);
            CHECK(list->count() == 2 && list->item(1)->text().endsWith("\tBeacon"));
            CHECK(dbValue("SELECT CONCAT(rigname,'|',Protocol,'|',IP,'|',URL,'|',TXcommand) FROM wspr.rigs WHERE id = 1").toString()
                  == "Beacon|TCP|127.0.0.1|api/cmd?c=|SET FREQ freq;SET CALL call;SET OFFSET offset;SET LOC grid;SET PWR pwr");
            emit list->itemDoubleClicked(list->item(1));      // "Set this as current rig" - Yes
            pump(100);
            CHECK(t.findChild<QLineEdit *>("selrigtextBox")->text() == "1\tBeacon");
            CHECK(dbValue("SELECT selected FROM wspr.rigs WHERE id = 1").toInt() == 1);
            t.findChild<QCheckBox *>("rigctldcheckBox")->setChecked(true);   // "Do not use rigctld"
            pump(100);
            dry->setChecked(false);
            vox->setChecked(false);
            t.findChild<QCheckBox *>("testTonescheckBox")->setChecked(false);
            t.findChild<QLineEdit *>("CalltextBox")->setText("G4GCI");
            t.findChild<QCheckBox *>("Type2checkBox")->setChecked(false);
            web.take();
            t.txTick(at(0, 5));
            t.txTick(at(1, 54));
            t.txTick(at(1, 55));                           // TX command, freq = 14095600 + 1400 + 50
            const QStringList prep = web.take();
            CHECK(prep == (QStringList{"/api/cmd?c=SET+FREQ+14097050", "/api/cmd?c=SET+CALL+G4GCI", "/api/cmd?c=SET+OFFSET+50",
                                       "/api/cmd?c=SET+LOC+IO90", "/api/cmd?c=SET+PWR+23"}));
            if (prep.size() != 5) for (const QString &p : prep) std::printf("     got %s\n", qPrintable(p));
            t.txTick(at(2, 0));                            // start TX
            const QStringList on = web.take();
            CHECK(t.isTransmitting() && on == QStringList{"/api/cmd?c=TXNOW"});
            if (on.size() != 1) std::printf("     enable=%d run=%s tx=%d got %s / %s\n", enable->isChecked(), qPrintable(runBtn->text()), t.isTransmitting(), qPrintable(on.join(",")), qPrintable(t.statusBar()->currentMessage()));
            t.findChild<QPushButton *>("stopTestbutton")->click();
            CHECK(!t.isTransmitting() && web.take() == QStringList{"/api/cmd?c=STOP"});
            CHECK(dbValue("SELECT CONCAT(command1,'|',reply1) FROM wspr.rigs WHERE id = 1").toString() == "TX|BAND SET TO");
            // the beacon doesn't confirm: that slot is cancelled
            QMetaObject::invokeMethod(&web, [&] { QMutexLocker l(&web.lock); web.band = "?"; web.confirm = false; },
                                      Qt::BlockingQueuedConnection);
            answered.clear();
            t.txTick(at(2, 5)); t.txTick(at(1, 54)); t.txTick(at(1, 55)); t.txTick(at(2, 0));
            pump(200);
            CHECK(!t.isTransmitting() && answered.filter("no reply contained \"BAND SET TO\"").size() >= 1);
            web.take();
            // a transmitter that doesn't answer: the slot is cancelled
            QMetaObject::invokeMethod(&web, [&] { web.server->close(); }, Qt::BlockingQueuedConnection);
            t.txTick(at(2, 5)); t.txTick(at(1, 54)); t.txTick(at(1, 55)); t.txTick(at(2, 0));
            CHECK(!t.isTransmitting());
            pump(200);
            CHECK(answered.filter("Error sending TX command to Beacon").size() >= 1);
            // right click deletes it
            QMouseEvent right(QEvent::MouseButtonPress, list->visualItemRect(list->item(1)).center(),
                              list->viewport()->mapToGlobal(list->visualItemRect(list->item(1)).center()),
                              Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QApplication::sendEvent(list->viewport(), &right);
            pump(100);
            CHECK(list->count() == 1 && dbValue("SELECT COUNT(*) FROM wspr.rigs WHERE id = 1").toInt() == 0);

            // no rig at all: the frequency is picked by hand and slot frequencies are ignored (user manual)
            auto *f2 = t.findChild<QListWidget *>("FlistBox2");
            const auto f40 = f2->findItems("7.0386", Qt::MatchStartsWith);
            CHECK(!f2->isHidden() && !f40.isEmpty());
            f2->setCurrentItem(f40.first());
            CHECK(t.statusBar()->currentMessage().contains("Frequency selected: 7.0386 MHz"));
            CHECK(t.findChild<QListWidget *>("FlistBox")->currentItem()->text().startsWith("7.0386"));
            vox->setChecked(true);                          // VOX keys the rig
            t.txTick(at(2, 5)); t.txTick(at(1, 54)); t.txTick(at(1, 55)); t.txTick(at(2, 0));
            CHECK(t.isTransmitting() && runBtn->text() == "TX 7.0386 MHz");   // not the slot's 14.0956
            t.findChild<QPushButton *>("stopTestbutton")->click();
            vox->setChecked(false);
            webThread.quit();
            webThread.wait();
        }

        // Remote tab: HTTPS page from a "browser" in another thread (the server runs in the window's thread)
        {
            auto *rt = t.findChild<QTabWidget *>("tabControl1");
            for (int i = 0; i < rt->count(); ++i)
                if (rt->tabText(i) == "Remote") rt->setCurrentIndex(i);
            QTcpServer probePort;
            probePort.listen(QHostAddress::LocalHost);
            const int rport = probePort.serverPort() < 1024 ? 45029 : probePort.serverPort();
            probePort.close();
            CHECK(t.findChild<QLabel *>("remoteStatusLabel")->text().startsWith("Remote access is off"));
            t.findChild<QCheckBox *>("remoteEnabledCheckBox")->setChecked(true);
            t.findChild<QLineEdit *>("remotePortTextBox")->setText("80");
            answered.clear();
            t.findChild<QPushButton *>("remoteSaveButton")->click();
            pump(100);
            CHECK(answered.contains("Port must be a number from 1024 to 65535 (for example 5029)"));
            t.findChild<QLineEdit *>("remotePortTextBox")->setText(QString::number(rport));
            t.findChild<QLineEdit *>("remoteUserTextBox")->setText("neville");
            auto *rpw = t.findChild<QLineEdit *>("remotePasswordTextBox");
            auto *rpc = t.findChild<QLineEdit *>("remotePasscodeTextBox");
            CHECK(rpw->echoMode() == QLineEdit::Password && rpc->echoMode() == QLineEdit::Password);
            t.findChild<QCheckBox *>("showcodecheckBox")->setChecked(true);
            CHECK(rpw->echoMode() == QLineEdit::Normal && rpc->echoMode() == QLineEdit::Normal);
            t.findChild<QCheckBox *>("showcodecheckBox")->setChecked(false);
            rpw->setText("short");
            rpc->setText("4711");
            t.findChild<QPushButton *>("remoteSaveButton")->click();
            pump(100);
            CHECK(answered.contains("The password must be at least 8 characters"));
            rpw->setText("radio rocks 73");
            t.findChild<QPushButton *>("remoteSaveButton")->click();
            pump(300);
            CHECK(answered.filter("Remote settings saved").size() == 1);
            CHECK(rpw->text().isEmpty() && rpw->placeholderText() == "(unchanged)");
            CHECK(t.remoteServer() && t.remoteServer()->isListening() && t.remoteServer()->port() == rport);
            CHECK(t.findChild<QLabel *>("remoteStatusLabel")->text().contains("running on port"));
            CHECK(t.findChild<QLabel *>("label147")->text().contains(QString(":%1").arg(rport)));
            {
                QFile ini(remoteDir.filePath("remote.ini"));
                ini.open(QIODevice::ReadOnly);
                const QByteArray all = ini.readAll();
                CHECK(all.contains("user=neville") && !all.contains("radio rocks") && !all.contains("4711"));
            }

            QString cookie;
            auto http = [&](const QByteArray &method, const QByteArray &path, const QByteArray &form) {
                QByteArray out;
                QThread *th = QThread::create([&] {
                    QSslSocket s;
                    s.setPeerVerifyMode(QSslSocket::VerifyNone);
                    s.connectToHostEncrypted("127.0.0.1", quint16(rport));
                    if (!s.waitForEncrypted(10000)) return;
                    QByteArray req = method + ' ' + path + " HTTP/1.1\r\nHost: x\r\n";
                    if (!cookie.isEmpty()) req += "Cookie: wsprsked=" + cookie.toLatin1() + "\r\n";
                    if (!form.isEmpty()) req += "Content-Length: " + QByteArray::number(form.size()) + "\r\n";
                    s.write(req + "\r\n" + form);
                    while (s.waitForReadyRead(10000)) out += s.readAll();
                });
                th->start();
                while (!th->isFinished()) pump(20);
                delete th;
                const auto m = QRegularExpression("Set-Cookie: wsprsked=([0-9a-f]*)").match(QString::fromLatin1(out));
                if (m.hasMatch()) cookie = m.captured(1);
                return QString::fromUtf8(out);
            };
            CHECK(http("POST", "/login", "user=neville&password=radio+rocks+73&passcode=4711").startsWith("HTTP/1.1 303"));
            QString page = http("GET", "/", "");
            const QString csrf = QRegularExpression("name=\"csrf\" value=\"([0-9a-f]+)\"").match(page).captured(1);
            CHECK(page.contains("WSPR Scheduler - G4GCI") && page.contains("Schedule: enabled") && !page.contains("Go next slot"));
            CHECK(t.findChild<QLabel *>("label148")->text().contains("logged in"));
            auto *noSked = t.findChild<QCheckBox *>("noSkedcheckBox");
            dry->setChecked(true);
            vox->setChecked(false);
            enable->setChecked(true);
            http("POST", "/do", "a=txoff&csrf=" + csrf.toLatin1());
            CHECK(!enable->isChecked());
            http("POST", "/do", "a=txon&csrf=" + csrf.toLatin1());
            CHECK(enable->isChecked());
            page = http("GET", "/", "");
            std::printf("     remote page status: %s\n", qPrintable(QRegularExpression("<pre>([^<]*)</pre>").match(page).captured(1).replace('\n', " | ")));
            CHECK(page.contains("ENABLED - dry run"));
            http("POST", "/do", "a=skedoff&csrf=" + csrf.toLatin1());
            CHECK(noSked->isChecked() && t.findChild<QLabel *>("skedstoplabel")->text() == "Sked disabled");
            http("POST", "/do", "a=skedon&csrf=" + csrf.toLatin1());
            CHECK(!noSked->isChecked());

            // allow TX/RX control: Go next slot on 20 m with the test antenna
            t.findChild<QCheckBox *>("remoteAllowTxCheckBox")->setChecked(true);
            t.findChild<QPushButton *>("remoteSaveButton")->click();   // restarts: log in again
            pump(300);
            cookie.clear();
            http("POST", "/login", "user=neville&password=radio+rocks+73&passcode=4711");
            page = http("GET", "/", "");
            const QString csrf2 = QRegularExpression("name=\"csrf\" value=\"([0-9a-f]+)\"").match(page).captured(1);
            CHECK(page.contains("Go next slot") && page.contains("<option>14.0956</option>") && page.contains("9: TXtest"));
            const QDateTime now = QDateTime::currentDateTimeUtc();
            QDateTime cyc = TxPlan::nextCycleStart(now);
            if (now.secsTo(cyc) < 4) cyc = cyc.addSecs(120);
            http("POST", "/do", "a=go&freq=14.0956&ant=9%3A+TXtest&csrf=" + csrf2.toLatin1());
            CHECK(noSked->isChecked());
            page = http("GET", "/", "");
            std::printf("     after go: %s\n", qPrintable(QRegularExpression("<p class=\"msg\">([^<]*)</p>").match(page).captured(1)));
            CHECK(page.contains("TX armed for") && page.contains("14.0956 MHz, antenna TXtest (dry run)"));
            CHECK(t.statusBar()->currentMessage().contains("TX armed for"));
            t.txTick(cyc.addSecs(-6));   // odd :54 - look-up keeps the remote slot although the sked is off
            t.txTick(cyc.addSecs(-5));   // :55 - switch and tune
            t.txTick(cyc);               // :00 - transmit (dry run)
            CHECK(t.isTransmitting() && runBtn->text() == "DRY RUN 14.0956 MHz");
            CHECK(http("GET", "/", "").contains("Transmitting now on 14.0956 MHz"));
            page = http("POST", "/do", "a=go&freq=14.0956&ant=9%3A+TXtest&csrf=" + csrf2.toLatin1());
            CHECK(http("GET", "/", "").contains("Transmitting now - try again after this slot"));
            t.findChild<QPushButton *>("stopTestbutton")->click();
            // TX disabled: the next slot only receives on the chosen frequency
            enable->setChecked(false);
            http("POST", "/do", "a=go&freq=14.0956&ant=9%3A+TXtest&csrf=" + csrf2.toLatin1());
            CHECK(http("GET", "/", "").contains("(TX disabled)"));
            const QDateTime n2 = QDateTime::currentDateTimeUtc();
            QDateTime cyc2 = TxPlan::nextCycleStart(n2);
            if (n2.secsTo(cyc2) < 4) cyc2 = cyc2.addSecs(120);
            t.txTick(cyc2.addSecs(-5));
            t.txTick(cyc2.addSecs(1));
            CHECK(!t.isTransmitting() && runBtn->text() == "RX 14.0956 MHz");
            t.grab().save("remote_tab.png");
            http("POST", "/do", "a=logout&csrf=" + csrf2.toLatin1());
            CHECK(t.findChild<QLabel *>("label148")->text().contains("logged out"));

            // switched off: the server stops
            t.findChild<QCheckBox *>("remoteEnabledCheckBox")->setChecked(false);
            t.findChild<QPushButton *>("remoteSaveButton")->click();
            pump(100);
            CHECK(!t.remoteServer()->isListening());
            CHECK(t.findChild<QLabel *>("remoteStatusLabel")->text().startsWith("Remote access is off"));
            noSked->setChecked(false);
        }

        // Stop Internet and stop RX
        {
            auto *si = t.findChild<QCheckBox *>("stopInternetcheckBox");
            si->setChecked(true);
            CHECK(t.statusBar()->currentMessage().startsWith("Internet stopped"));
            si->setChecked(false);
            auto *srx = t.findChild<QCheckBox *>("stopRXcheckBox");
            QWidget *rxWin = nullptr;
            for (QWidget *w2 : QApplication::topLevelWidgets())
                if (w2->windowTitle() == "Transmissions received by this station" && w2->isVisible()) rxWin = w2;
            srx->setChecked(true);
            CHECK(!rxWin || !rxWin->isVisible());
            t.findChild<QPushButton *>("saveConfigbutton")->click();
            pump(100);
            CHECK(dbValue("SELECT stopRX FROM wspr_configs.settings LIMIT 1").toInt() == 1);
            srx->setChecked(false);
            CHECK(!rxWin || rxWin->isVisible());
        }

        // exit: asked first; No keeps the program running
        answered.clear();
        sayNo = true;
        t.close();
        pump(200);
        CHECK(answered.contains("Exit app (y/n)?") && t.isVisible());
        t.findChild<QLineEdit *>("defaultOfftextBox")->setText("61");
        t.close();                                    // Yes to exit, Yes to saving
        pump(200);
        CHECK(!t.isVisible() && answered.contains("Save all settings before exit (y/n)?"));
        CHECK(dbValue("SELECT `Offset` FROM wspr_configs.settings LIMIT 1").toInt() == 61);
        boardThread.quit();
        boardThread.wait();
        dbValue("DELETE FROM wspr_slots.slots WHERE `date` = '2030-01-01'");
        dbValue("DELETE FROM wspr.antennas WHERE AntNo = 9");
    }

 if (fails) for (const QString &a : answered) std::printf("  dialog: %s\n", qPrintable(a));
    std::printf(fails ? "mainwindow_uitest: %d FAILED\n" : "mainwindow_uitest: all passed\n", fails);
    w.close();
    pump(200);
    return fails ? 1 : 0;
}
