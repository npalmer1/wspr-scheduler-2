// Hardware checks on demand: "Test Arduino" (TX Configuration) and "Test connection" (Rigctl rigs).
// C#: testArduino (button1). The rigctld connection test is new.
//
// Differences from the C# Test Arduino:
//  - the port typed is used (C# put 5000 back in the box on every click);
//  - IP, port and pin are all checked before anything is sent (C# accepted any one of them);
//  - "Test ok!" only when the board confirms the pin and level asked for (C#: any reply);
//  - runs on a worker thread, so the window doesn't freeze while waiting for the board.
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "arduinoswitch.h"
#include "msg.h"
#include "rigctl.h"
#include "rigctld.h"

#include <QCheckBox>
#include <QHostAddress>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSerialPortInfo>
#include <QStatusBar>
#include <QTcpSocket>
#include <QThread>
#include <memory>

// ---- Test Arduino ----------------------------------------------------------------------------

bool MainWindow::pintextBox_KeyPress(QKeyEvent *) { return false; }           // validators do the checking
bool MainWindow::ArduinoPorttextBox_KeyPress(QKeyEvent *) { return false; }
bool MainWindow::ArduinoIPtextBox_KeyPress(QKeyEvent *) { return false; }
void MainWindow::ArduinoPorttextBox_TextChanged(const QString &) {}

void MainWindow::setupHardwareTests()
{
    ui->ArduinoIPtextBox->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9.]{0,15}"), ui->ArduinoIPtextBox));
    ui->ArduinoPorttextBox->setValidator(new QIntValidator(1, 65535, ui->ArduinoPorttextBox));
    ui->pintextBox->setValidator(new QIntValidator(0, 13, ui->pintextBox));   // Arduino data pins D0-D13
    if (ui->ArduinoPorttextBox->text().isEmpty())
        ui->ArduinoPorttextBox->setText("5000");                                 // C# default
    ui->ArduinoIPtextBox->setToolTip("IP address of the WiFi Arduino switch or filter board");
    ui->pintextBox->setToolTip("Data pin 0-13 (antenna switch: pin = switch port - 1)");
    ui->LEDcheckBox->setToolTip("The level sent last: each click sends the other one");

    if (!m_rigTestButton) {
        m_rigTestButton = new QPushButton("Test connection", ui->tabPage3);
        m_rigTestButton->setObjectName("rigTestbutton");
        m_rigTestButton->setToolTip("Check that rigctld answers at the IP address and port shown, and that it can read the radio");
        const QRect stop = ui->stopRigCtlDbutton->geometry();
        m_rigTestButton->setGeometry(stop.x(), stop.bottom() + 8, qMax(stop.width(), 100), stop.height());
        m_rigTestButton->show();
        connect(m_rigTestButton, &QPushButton::clicked, this, &MainWindow::testRigConnection);
        showRadioToUse();   // the radio label goes under this button
    }
}

void MainWindow::button1_Click()
{
    const QString ip = ui->ArduinoIPtextBox->text().trimmed();
    bool okPort = false, okPin = false;
    const int port = ui->ArduinoPorttextBox->text().trimmed().toInt(&okPort);
    const int pin = ui->pintextBox->text().trimmed().toInt(&okPin);
    if (!checkIP(ip) || !okPort || port < 1 || port > 65535 || !okPin || pin < 0 || pin > 13) {
        Msg::ok(this, "Enter the board's IP address, its TCP port (normally 5000) and a pin number 0-13",
                "Test Arduino");
        return;
    }
    const bool high = !ui->LEDcheckBox->isChecked();   // C#: toggles each click
    ui->button1->setEnabled(false);
    ui->ardReplylabel->setText("Test in progress - please wait");
    ui->HiLolabel->setText("pending");
    for (QLabel *l : {ui->ardReplylabel, ui->HiLolabel})
        l->adjustSize();
    auto result = std::make_shared<ArduinoSwitch::Result>();
    QThread *t = QThread::create([=] { *result = ArduinoSwitch::setPin(ip, port, pin, high); });
    connect(t, &QThread::finished, this, [=] {
        t->deleteLater();
        ui->button1->setEnabled(true);
        if (result->ok) {
            ui->LEDcheckBox->setChecked(high);
            ui->ardReplylabel->setText("Test ok!");
            ui->HiLolabel->setText(QString("Pin %1 %2").arg(pin).arg(high ? "high" : "low"));
        } else {
            ui->ardReplylabel->setText("Test fail?");
            ui->HiLolabel->setText("error");
        }
        for (QLabel *l : {ui->ardReplylabel, ui->HiLolabel})
            l->adjustSize();
        Msg::timed(this, result->ok ? "Received: " + result->reply : result->error, "Switch reply", result->ok ? 2000 : 5000);
    });
    t->start();
}

// ---- rigctld connection test ------------------------------------------------------------------

namespace {
struct RigCheck
{
    bool tcp = false;        // something accepted the connection
    QString tcpError;
    bool answered = false;   // rigctld answered "f"
    qint64 hz = 0;
    QString rigError;        // what rigctld said instead of a frequency
    std::optional<bool> ptt;
};

RigCheck checkRig(const QString &ip, int port)
{
    RigCheck c;
    {
        QTcpSocket s;
        s.connectToHost(ip, quint16(port));
        if (!s.waitForConnected(2500)) {
            c.tcpError = s.errorString();
            return c;
        }
        c.tcp = true;
    }
    RigCtl rig(ip, port);
    rig.setTimeout(3000);
    if (const auto f = rig.frequency()) {
        c.answered = true;
        c.hz = *f;
        c.ptt = rig.ptt();
    } else {
        c.rigError = rig.lastError();
    }
    return c;
}
} // namespace

void MainWindow::testRigConnection()
{
    const QString ip = ui->IPtextBox->text().trimmed().isEmpty() ? QString("127.0.0.1") : ui->IPtextBox->text().trimmed();
    const int port = ui->PorttextBox->text().trimmed().toInt();
    if (port < 1 || port > 65535) {
        Msg::ok(this, "Enter a TCP port 1-65535 (rigctld normally uses 4532)", "Test connection");
        return;
    }
    m_rigTestButton->setEnabled(false);
    m_rigTestButton->setText("Testing...");
    auto result = std::make_shared<RigCheck>();
    QThread *t = QThread::create([=] { *result = checkRig(ip, port); });
    connect(t, &QThread::finished, this, [=] {
        t->deleteLater();
        m_rigTestButton->setEnabled(true);
        m_rigTestButton->setText("Test connection");
        const RigCheck &c = *result;
        const bool local = Hamlib::isLocalAddress(ip);
        QStringList lines;
        QString title;
        if (c.answered) {
            title = "rigctld OK";
            lines << QString("rigctld at %1:%2 answers and reads the radio.").arg(ip).arg(port)
                  << QString("Dial frequency: %1 MHz").arg(c.hz / 1e6, 0, 'f', 6);
            if (c.ptt)
                lines << QString("PTT: %1").arg(*c.ptt ? "ON (transmitting!)" : "off");
            setRigLabels(true);
        } else if (c.tcp) {
            title = "rigctld answers, the radio doesn't";
            lines << QString("rigctld is running at %1:%2 but can't read the radio:").arg(ip).arg(port)
                  << "  " + (c.rigError.isEmpty() ? QString("no answer") : c.rigError)
                  << "Check the radio is on, and the COM port, baud rate and radio model on this tab"
                     " (they must match the radio's CAT settings), then Stop and Start RigCtlD.";
            setRigLabels(false);
        } else {
            title = "Can't reach rigctld";
            lines << QString("Nothing answers at %1:%2 (%3).").arg(ip).arg(port).arg(c.tcpError);
            if (local)
                lines << "rigctld isn't running on this PC at that port. Press Start RigCtlD"
                         " (or Save RigCtl Config, which starts it with these settings).";
            else
                lines << QString("rigctld runs on %1: start it there with -T 0.0.0.0 -t %2, and check that PC's firewall"
                                 " allows TCP port %2.").arg(ip).arg(port);
            setRigLabels(false);
        }
        // settings typed but not saved, or rigctld started here and stopped by itself
        if (ip != m_rigCfg.ipv4.trimmed() || QString::number(port) != m_rigCfg.port.trimmed())
            lines << QString("\nNote: the saved settings are %1:%2 - this test used the boxes as they are now;"
                             " press Save RigCtl Config to use them.").arg(m_rigCfg.ipv4, m_rigCfg.port);
        if (local && !c.answered) {
            const QString com = m_rigCfg.comPort.trimmed();
            bool comFound = com.isEmpty();
            for (const QString &p : Hamlib::serialPortNames())
                if (p.compare(com, Qt::CaseInsensitive) == 0)
                    comFound = true;
            if (!comFound)
                lines << QString("\nThe saved COM port %1 isn't on this PC now - pick the radio's port from the list.").arg(com);
            if (m_rigctld && !m_rigctld->lastOutput().trimmed().isEmpty())
                lines << "\nrigctld last said:\n" + m_rigctld->lastOutput().trimmed().right(300);
        }
        statusBar()->showMessage(title, 15000);
        Msg::ok(this, lines.join('\n'), title);
    });
    t->start();
}
