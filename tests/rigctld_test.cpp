// rigctld start/stop and the rig list (Rigctl rigs tab), using Hamlib's dummy rig (model 1) on a
// spare port; plus the rigctl settings in the scratch database (tests/fixture.sql) if it is there.
// Needs Hamlib (rigctl, rigctld) on PATH or in HAMLIB_DIR.
#include "rigctl.h"
#include "rigctld.h"
#include "slotstore.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool waitAnswering(const QString &ip, int port, int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        if (RigctldProcess::isAnswering(ip, port, 300))
            return true;
        QCoreApplication::processEvents();
        QThread::msleep(100);
    }
    return false;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString dir = qEnvironmentVariable("HAMLIB_DIR");

    // parsing (no Hamlib needed)
    CHECK(Hamlib::modelNumber("  1046  Yaesu   FT-450  20221109.0  Stable  RIG_MODEL_FT450") == 1046);
    CHECK(Hamlib::modelNumber("1") == 1 && Hamlib::modelNumber(" Rig #  Mfg") == 0 && Hamlib::modelNumber("") == 0);
    CHECK(Hamlib::parseRigList(" Rig #  Mfg  Model\n     1  Hamlib  Dummy\r\n\n  1046  Yaesu FT-450\n")
          == (QStringList{"     1  Hamlib  Dummy", "  1046  Yaesu FT-450"}));
    RigctlSettings s;
    s.radio = "  1046  Yaesu   FT-450  ...";
    s.comPort = "COM4";
    s.baud = "38400";
    CHECK(Hamlib::rigctldArguments(s) == (QStringList{"-m", "1046", "-r", "COM4", "-s", "38400", "-T", "127.0.0.1", "-t", "4532"}));

    if (Hamlib::executable(dir, "rigctld").isEmpty()) {
        std::printf("rigctld_test: Hamlib not found, process tests skipped\n");
    } else {
        QString err;
        const QStringList rigs = Hamlib::rigList(dir, &err);
        CHECK(rigs.size() > 100);
        CHECK(!rigs.isEmpty() && Hamlib::modelNumber(rigs.first()) == 1 && rigs.first().contains("Dummy"));

        RigctlSettings d;
        d.radio = rigs.value(0);           // dummy rig: no serial port needed
        d.comPort.clear();
        d.baud.clear();
        d.port = "4599";
        CHECK(!RigctldProcess::isAnswering("127.0.0.1", 4599, 300));
        RigctldProcess p;
        CHECK(p.start(dir, d));
        CHECK(p.startedHere());
        CHECK(waitAnswering("127.0.0.1", 4599, 5000));
        RigCtl rc("127.0.0.1", 4599);
        CHECK(rc.setFrequency(14095600));
        CHECK(rc.frequency().value_or(0) == 14095600);
        CHECK(p.start(dir, d));            // restart replaces the first copy
        CHECK(waitAnswering("127.0.0.1", 4599, 5000));
        CHECK(p.stop());
        CHECK(!p.startedHere());
        QThread::msleep(300);
        CHECK(!RigctldProcess::isAnswering("127.0.0.1", 4599, 300));

        RigctlSettings none;               // no radio chosen
        CHECK(!p.start(dir, none) && p.error() == "No radio selected");
        RigctlSettings bad = d;            // a real rig on a port that doesn't exist: rigctld exits
        bad.radio = "1046";
        bad.comPort = "/dev/does-not-exist";
        bad.port = "4598";
        int exitCode = -999;
        QObject::connect(&p, &RigctldProcess::exited, [&](int c, const QString &) { exitCode = c; });
        CHECK(p.start(dir, bad));
        QElapsedTimer t;
        t.start();
        while (exitCode == -999 && t.elapsed() < 8000) {
            QCoreApplication::processEvents();
            QThread::msleep(50);
        }
        std::printf("  rigctld with a missing port: exit %d, said: %s\n", exitCode,
                    qPrintable(p.lastOutput().trimmed().left(120)));
        CHECK(exitCode != -999 || !RigctldProcess::isAnswering("127.0.0.1", 4598, 300));
        p.stop();
    }

    // database (scratch server with tests/fixture.sql loaded)
    SlotStore st;
    if (auto cfg = st.loadRigctl()) {
        CHECK(Hamlib::modelNumber(cfg->radio) == 1046 && cfg->comPort == "COM4" && cfg->baud == "38400" && !cfg->noRigctld);
        RigctlSettings n = *cfg;
        n.comPort = "COM7";
        n.vox = true;
        n.ppm = 0.85;
        n.radio = "O'Neil's rig 1046";       // apostrophe: C# UPDATE pasted the text into SQL
        CHECK(st.saveRigctl(n));
        auto back = st.loadRigctl();
        CHECK(back && back->comPort == "COM7" && back->vox && back->ppm == 0.85 && back->radio == "O'Neil's rig 1046");
        CHECK(st.saveRigctl(*cfg));
    } else {
        std::printf("rigctld_test: no scratch database, settings test skipped (%s)\n", qPrintable(st.lastError()));
    }

    std::printf(failures ? "rigctld_test: %d FAILED\n" : "rigctld_test: all passed\n", failures);
    return failures ? 1 : 0;
}
