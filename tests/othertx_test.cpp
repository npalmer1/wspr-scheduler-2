// OtherTx against a fake HTTP rig (python http.server) and a fake serial rig (socat pair).
// Linux/macOS (needs python3; serial part needs socat); skipped otherwise.
#include "othertx.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static QString readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace OtherTx;
    if (QStandardPaths::findExecutable("python3").isEmpty()) {
        std::printf("python3 not found - skipped\n");
        return 77;
    }
    QTemporaryDir dir;

    // ---- HTTP rig: logs each request path, answers "ok:<path>", 500 for paths containing FAIL, slow for SLOW
    const QString httpLog = dir.filePath("http.log");
    QTcpServer probe; probe.listen(QHostAddress::LocalHost); const int port = probe.serverPort(); probe.close();
    const QString server = QString(R"(
import http.server, time
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        open('%1','a').write(self.path + '\n')
        if 'SLOW' in self.path: time.sleep(3)
        code = 500 if 'FAIL' in self.path else 200
        self.send_response(code); self.end_headers(); self.wfile.write(('ok:' + self.path).encode())
    def log_message(self, *a): pass
http.server.HTTPServer(('127.0.0.1', %2), H).serve_forever()
)").arg(httpLog).arg(port);
    QProcess http;
    http.start("python3", {"-c", server});
    QThread::msleep(700);

    RigLink web{"HTTP", "127.0.0.1", QString::number(port), "", "", "/api?cmd=", false};
    CHECK(baseUrl(web) == QString("http://127.0.0.1:%1/api?cmd=").arg(port));
    auto r = send(web, "FREQ 14095600;PTT ON;");
    CHECK(r.ok);
    CHECK(readAll(httpLog) == "/api?cmd=FREQ+14095600\n/api?cmd=PTT+ON\n");   // in order, spaces as '+', no empty request
    CHECK(r.reply == "ok:/api?cmd=FREQ+14095600\nok:/api?cmd=PTT+ON");

    r = send(web, "FREQ 7038600;FAIL;PTT ON");          // stops at the failing command, PTT not sent
    CHECK(!r.ok && r.error.contains("HTTP 500"));
    CHECK(!readAll(httpLog).contains("7038600\n/api?cmd=FAIL\n/api?cmd=PTT"));
    std::printf("     HTTP 500: %s\n", qPrintable(r.error));

    QElapsedTimer t; t.start();
    r = send(web, "SLOW", 800);
    CHECK(!r.ok && t.elapsed() < 2500);
    std::printf("     slow device: %s (%lld ms)\n", qPrintable(r.error), t.elapsed());

    RigLink dead{"HTTP", "127.0.0.1", "1", "", "", "x?c=", false};
    r = send(dead, "PTT ON", 800);
    CHECK(!r.ok);
    std::printf("     nothing listening: %s\n", qPrintable(r.error));
    http.kill(); http.waitForFinished();

    // ---- serial rig
    RigLink bad{"Serial", "", "COM99", "9600", "8,none,1", "", false};
    CHECK(!send(bad, "FA014095600;").ok);                 // incomplete settings -> error, not silent 8N1
    if (QStandardPaths::findExecutable("socat").isEmpty()) {
        std::printf("socat not found - serial part skipped\n");
        return failures ? 1 : 0;
    }
    const QString a = dir.filePath("ttyA"), b = dir.filePath("ttyB"), serLog = dir.filePath("ser.log");
    QProcess socat;
    socat.start("socat", {"pty,raw,echo=0,link=" + a, "pty,raw,echo=0,link=" + b});
    for (int i = 0; i < 50 && !(QFile::exists(a) && QFile::exists(b)); ++i) QThread::msleep(50);
    // fake CAT rig: logs lines; answers "FA;" queries with a Yaesu-style ';'-terminated reply, set commands silently
    const QString cat = QString(R"(
import os
fd = os.open('%1', os.O_RDWR); buf = b''; n = 0
while n < 2:
    buf += os.read(fd, 100)
    while b'\n' in buf:
        line, buf = buf.split(b'\n', 1)
        open('%2','a').write(line.decode() + '\n'); n += 1
        if line == b'FA;': os.write(fd, b'FA014095600;')
)").arg(b, serLog);
    QProcess dev;
    dev.start("python3", {"-c", cat});
    QThread::msleep(200);
    RigLink yaesu{"Serial", "", a, "38400", "8,none,2,none", "", false};
    r = send(yaesu, "FA014095600;TX1;");
    CHECK(r.ok && r.reply.isEmpty());                    // no answer to set commands is fine (C#: "error")
    r = send(yaesu, "FA;");
    CHECK(r.ok && r.reply == "FA014095600;");            // ';'-terminated reply captured (C# ReadLine timed out)
    dev.waitForFinished(2000);
    CHECK(readAll(serLog) == "FA014095600;TX1;\nFA;\n");  // sent as one line each, ';' kept
    socat.kill(); socat.waitForFinished();

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
