// Drives the real receive window against the scratch MariaDB (tests/rx_fixture.sql), answering its
// message boxes with Yes/OK. Saves a screenshot to rxform_qt.png in the working directory.
// Run with QT_QPA_PLATFORM=offscreen on a machine without a display.
#include "msg.h"
#include "rxcycle.h"
#include "rxform.h"
#include "rxtablemodel.h"
#include "spectrumform.h"
#include "wavrecorder.h"
#include "wsprencode.h"
#include "wsprmodulator.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTableView>
#include <QTimeZone>
#include <QTimer>
#include <cstdio>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { ++fails; std::printf("FAIL line %d: %s\n", __LINE__, #c); } else std::printf("ok   %s\n", #c); } while (0)

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Rx::setDebugLogPath(QDir::temp().filePath("rxform_uitest_debug.txt"));
    QStringList answered;
    QTimer auto_;
    QObject::connect(&auto_, &QTimer::timeout, [&] {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            auto *d = qobject_cast<QDialog *>(w);
            if (!d || !d->isVisible() || d->objectName() != "MessageDialog") continue;
            if (auto *l = d->findChild<QLabel *>()) answered << l->text().split('\n').first();
            for (QPushButton *b : d->findChildren<QPushButton *>())
                if (b->text() == "Yes" || b->text() == "OK") { b->click(); break; }
        }
    });
    auto_.start(30);
    auto pump = [&](int ms = 100) { QEventLoop l; QTimer::singleShot(ms, &l, &QEventLoop::quit); l.exec(); };

    RxForm f;
    f.store().setDatabase("wspr_rpt_selftest");
    if (!f.store().count()) {
        std::printf("no scratch database - load tests/rx_fixture.sql; skipped\n");
        return 77;
    }
    f.setHeader("G4GCI", DbSettings{}, "IO90GU", "", "", "0.2.1");
    f.setFrequency("14.095600");
    f.setTime("22:51:07", "22:51");
    f.resize(1117, 620);
    f.show();
    pump(200);

    auto *grid = f.findChild<QTableView *>("dataGridView1");
    auto cb = [&](const char *n) { return f.findChild<QCheckBox *>(n); };
    auto combo = [&](const char *n) { return f.findChild<QComboBox *>(n); };
    auto lab = [&](const char *n) { return f.findChild<QLabel *>(n)->text(); };
    auto edit = [&](const char *n) { return f.findChild<QLineEdit *>(n); };
    auto btn = [&](const char *n) { return f.findChild<QPushButton *>(n); };

    CHECK(f.windowTitle() == "Transmissions received by this station");
    CHECK(lab("ownlabel") == "G4GCI" && lab("myloclabel") == "IO90GU" && lab("RXFlabel") == "14.095600");
    CHECK(lab("Timelabel") == "22:51:07 UTC");
    f.setTime("22:51:07", "23:51");
    CHECK(lab("Timelabel") == "22:51:07 UTC    (23:51 LT)");

    // rxconfig from the database (rxstore_test left deep=0 quick=1 osd=0 upload=0)
    CHECK(cb("QuickcheckBox")->isChecked() && !cb("DeepcheckBox")->isChecked() && combo("OSDlistBox")->currentIndex() == 0);
    cb("DeepcheckBox")->click();                       // ticking Deep clears Quick (C#: when focused)
    CHECK(cb("DeepcheckBox")->isChecked() && !cb("QuickcheckBox")->isChecked());
    combo("OSDlistBox")->setCurrentIndex(4);
    CHECK(lab("OSDlabel") == "OSD 4 (very slow)");
    combo("OSDlistBox")->setCurrentIndex(2);
    CHECK(lab("OSDlabel") == "OSD 2 (slow)");
    auto cfg = f.store().readConfig();                 // saved as it changes
    CHECK(cfg && cfg->deep && !cfg->quick && cfg->osd == 2);
    CHECK(f.useDeep() && f.osd() == 2);

    // a decoded cycle arrives (what RxCycle emits after wsprd)
    const QDateTime cyc(QDate(2026, 11, 1), QTime(22, 58), QTimeZone::utc());
    const QString out = " 2258 -12 0.4 14.097152 1 K1ABC FN42 37\n 2258 -5 0.2 14.097122 0 <G4GCI/P> IO90GU 23\n<DecodeFinished>\n";
    const int before = *f.store().count();
    emit f.cycle()->decoded(Rx::parseWsprdOutput(out, cyc, 14.0956, "G4GCI", "IO90GU"), "14.095600", cyc);
    CHECK(grid->model()->rowCount() == 2 && *f.store().count() == before + 2);
    CHECK(grid->model()->index(0, 0).data().toString() == "2026-11-01 22:58");
    const auto *m = f.model();
    CHECK(m->cellText(0, RxTableModel::TxCall) == "<G4GCI/P>" && m->cellText(1, RxTableModel::Snr) == "-12"
          && m->cellText(1, RxTableModel::Watts) == "5" && m->cellText(1, RxTableModel::Miles) == QString::number(Rx::kmToMiles(m->spot(1).distance)));
    CHECK(m->cellText(1, RxTableModel::Cw) == Rx::cwLabel(-12, 37, 50));

    // comparison power changes headings and every row
    combo("cwssblistBox")->setCurrentText("5");
    CHECK(grid->model()->headerData(RxTableModel::Cw, Qt::Horizontal).toString() == "CW @5W");
    CHECK(m->cellText(1, RxTableModel::Cw) == Rx::cwLabel(-12, 37, 36));
    combo("cwssblistBox")->setCurrentText("100");

    // stop receiving: label, nothing saved
    cb("stopRXcheckBox")->setChecked(true);
    CHECK(lab("RXofflabel") == "RX STOPPED" && f.receivingStopped() && !f.startReceive());
    emit f.cycle()->decoded(Rx::parseWsprdOutput("<DecodeFinished>\n", cyc.addSecs(120), 14.0956, "G4GCI", "IO90GU"),
                            "14.095600", cyc.addSecs(120));
    CHECK(grid->model()->rowCount() == 2 && *f.store().count() == before + 2);
    cb("stopRXcheckBox")->setChecked(false);
    CHECK(lab("RXofflabel") == "RX Running");

    // previous decodes / filters
    btn("button1")->click();
    CHECK(grid->model()->rowCount() == *f.store().count());
    combo("bandlistBox")->setCurrentIndex(9);          // 20m
    btn("filterbutton")->click();
    const int n20 = grid->model()->rowCount();
    CHECK(n20 >= 3);
    edit("callFiltertextBox")->setText("K1*");
    btn("filterbutton")->click();
    CHECK(edit("callFiltertextBox")->text() == "K1" && grid->model()->rowCount() == 2);   // '*' removed, as C#
    edit("callFiltertextBox")->clear();
    combo("bandlistBox")->setCurrentIndex(0);
    CHECK(lab("Ulabel") == "mls" && lab("Dlabel") == "0 - 12,453");
    edit("DFromtextBox")->setText("3000");             // miles -> 4827 km
    btn("filterbutton")->click();
    CHECK(grid->model()->rowCount() == 2);             // the two K1ABC spots (5182 km)
    cb("kmcheckBox")->setChecked(true);
    CHECK(lab("Ulabel") == "km" && lab("Dlabel") == "0 - 20,035");
    edit("DFromtextBox")->setText("5300");
    btn("filterbutton")->click();
    CHECK(grid->model()->rowCount() == 0);
    edit("DFromtextBox")->clear();
    btn("Clearbutton")->click();
    CHECK(grid->model()->rowCount() == *f.store().count());

    // delete with a right click on the row number: only when allowed; answered Yes
    const int rows = grid->model()->rowCount();
    auto *vh = grid->verticalHeader();
    const QPoint p0(vh->width() / 2, vh->sectionViewportPosition(0) + 5);
    emit vh->customContextMenuRequested(p0);
    pump(200);
    CHECK(grid->model()->rowCount() == rows);          // not allowed yet
    cb("allowDeletecheckBox")->setChecked(true);
    const QString first = grid->model()->index(0, 1).data().toString();
    emit vh->customContextMenuRequested(p0);
    pump(200);
    CHECK(grid->model()->rowCount() == rows - 1 && *f.store().count() == rows - 1);
    CHECK(answered.contains("Delete this row (Y/N)?"));
    std::printf("     deleted %s\n", qPrintable(first));

    // spectrum window follows a recording (a WSPR signal at 1450-1454 Hz plus noise)
    {
        const QString wav = QDir::temp().filePath("rxform_uitest_spectrum.wav");
        WsprModulator::Params mp;
        mp.offsetHz = 50;
        WsprModulator mod(*WsprEncode::encodeMessage("K1ABC FN42 37", nullptr), mp);
        const QVector<float> sig = mod.renderAll();
        QVector<float> rec(12000 * 110);
        quint32 r = 1;
        for (int i = 0; i < rec.size(); ++i) {
            r = r * 1664525u + 1013904223u;
            rec[i] = (i >= 12000 && i - 12000 < sig.size() ? 0.05f * sig[i - 12000] : 0.0f) + (int(r >> 9) / 8388608.0f - 1.0f) * 0.05f;
        }
        WavWriter w;
        w.open(wav);
        w.write(rec.constData(), 12000 * 40);           // first 40 s written...
        btn("Spectrumbutton")->click();
        SpectrumForm *sp = f.spectrum();
        CHECK(sp->isVisible() && sp->windowTitle() == "WSPR Spectrum");
        sp->resize(1200, 590);
        sp->setFrequency("14.095600");
        sp->setWavPath(wav, QDateTime(QDate(2026, 11, 2), QTime(0, 6), QTimeZone::utc()));
        sp->readNow();
        const int l1 = sp->view()->lineCount();
        CHECK(l1 == 12000 * 40 / 16384);
        w.write(rec.constData() + 12000 * 40, rec.size() - 12000 * 40);   // ...then the rest, read live
        w.close();
        sp->readNow();
        CHECK(sp->view()->lineCount() == 12000 * 110 / 16384);
        sp->readNow();
        CHECK(sp->findChild<QLabel *>("statusLabel")->text().startsWith("Idle or TX in progress"));
        // brightest column of a line in the middle of the transmission is at 1450-1455 Hz
        const QImage img = sp->view()->image();
        int bestX = 0, bestV = -1;
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = img.pixel(x, 20);
            const int v = qRed(c) * 4 + qGreen(c) * 2 + qBlue(c) / 2;
            if (v > bestV) { bestV = v; bestX = x; }
        }
        const double bestHz = 1300 + 400.0 * bestX / img.width();
        std::printf("     waterfall peak at %.1f Hz\n", bestHz);
        CHECK(bestHz > 1449 && bestHz < 1456);
        emit sp->view()->hoverHz(1452.0);
        CHECK(sp->findChild<QLabel *>("MHzfreqLabel")->text() == "14.097052 MHz");
        sp->findChild<QSlider *>("gainControl")->setValue(12);
        CHECK(sp->findChild<QLabel *>("gainLabel")->text() == "Gain: +12 dB");
        sp->findChild<QSlider *>("gainControl")->setValue(0);
        pump(200);
        sp->grab().save("spectrum_qt.png");
        sp->close();                                     // only hides
        CHECK(!sp->isVisible());
    }

    // the window ignores close until the main window allows it (C#: CS_NOCLOSE)
    f.close();
    CHECK(f.isVisible());

    btn("Clearbutton")->click();
    combo("bandlistBox")->setCurrentIndex(0);
    cb("allowDeletecheckBox")->setChecked(false);
    cb("kmcheckBox")->setChecked(false);
    pump(300);
    f.grab().save("rxform_qt.png");

    // database down: short status, one pop-up, window does not widen
    {
        const int w0 = f.width();
        DbSettings bad;
        bad.port = 1;   // nothing listening
        f.store().setSettings(bad);
        answered.clear();
        btn("button1")->click();
        pump(300);
        CHECK(lab("statuslabel") == "database error" && f.width() == w0);
        CHECK(answered.size() == 1 && answered.first().startsWith("Database error"));
        btn("button1")->click();                      // the same error again: no second pop-up
        pump(300);
        CHECK(answered.size() == 1 && f.width() == w0);
        f.setLabel("RX error: " + QString(300, 'x'));  // a very long error from the sound card / wsprd
        pump(300);
        CHECK(lab("statuslabel") == "RX error" && f.width() == w0);
        f.store().setSettings(DbSettings{});
    }

    f.setAllowClose(true);
    f.close();
    CHECK(!f.isVisible());
    std::printf(fails ? "rxform_uitest: %d FAILED\n" : "rxform_uitest: all passed\n", fails);
    return fails ? 1 : 0;
}
