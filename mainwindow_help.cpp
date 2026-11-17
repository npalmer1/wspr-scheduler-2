// Help (F1, or a click on "F1 Help" by the clock). C#: ShowHelp + HelpForm - main_help.rtf in a window
// of its own. Qt has no RTF reader, so the file is converted to HTML (rtfhtml.*) each time it is opened,
// which also picks up any edits to the file.
//
// The help file is wspr_sked2_help_vN.rtf (or wspr_sked2_help.rtf, = version 0) in C:\WSPR_Sked (the
// installer puts it there, and never over an edited copy), so it can be edited in WordPad without
// administrator rights. Its own name keeps it apart from the C# version's main_help.rtf in the same
// folder. The highest version wins, in C:\WSPR_Sked or next to the program - so a new release's
// _v(N+1) is shown even where an older one was installed. wspr_sked2_help.html is used if there's no .rtf.
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "msg.h"
#include "rtfhtml.h"
#include "rxspot.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QScreen>
#include <QShortcut>
#include <QTextBrowser>
#include <QVBoxLayout>

void MainWindow::setupHelp()
{
    auto *f1 = new QShortcut(QKeySequence(Qt::Key_F1), this);
    f1->setContext(Qt::WindowShortcut);
    connect(f1, &QShortcut::activated, this, &MainWindow::showHelp);
    ui->label145->setCursor(Qt::PointingHandCursor);
    ui->label145->setToolTip("Help (F1)");
    ui->label145->installEventFilter(this);   // click = F1 (see eventFilter)
}

QString MainWindow::helpFile() const
{
    // ~/WSPR_Sked (C:\WSPR_Sked), next to the program, and on Linux where "cmake --install" puts it
    // (/usr/local/share/wspr-scheduler-2)
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList dirs{Rx::defaultDataDir(), app, QDir(app).filePath("../share/wspr-scheduler-2")};
    return newestHelpFile(dirs);
}

// The highest wspr_sked2_help_vN.rtf in dirs (no "_vN" = 0); of the same version, the one saved last
// (an edited copy next to the program beats an older one in C:\WSPR_Sked). No .rtf: the .html.
QString MainWindow::newestHelpFile(const QStringList &dirs)
{
    static const QRegularExpression re("^wspr_sked2_help(?:_v(\\d+))?\\.rtf$",
                                       QRegularExpression::CaseInsensitiveOption);
    QString best;
    int bestVersion = -1;
    QDateTime bestTime;
    for (const QString &d : dirs)
        for (const QString &name : QDir(d).entryList({"wspr_sked2_help*.rtf"}, QDir::Files)) {
            const auto m = re.match(name);
            if (!m.hasMatch())
                continue;
            const int v = m.captured(1).toInt();
            const QString path = QDir(d).filePath(name);
            const QDateTime t = QFileInfo(path).lastModified();
            if (v > bestVersion || (v == bestVersion && t > bestTime)) {
                bestVersion = v;
                bestTime = t;
                best = path;
            }
        }
    if (best.isEmpty())
        for (const QString &d : dirs) {
            const QString p = QDir(d).filePath("wspr_sked2_help.html");
            if (QFileInfo::exists(p))
                return p;
        }
    return best;
}

void MainWindow::showHelp()
{
    const QString file = helpFile();
    if (file.isEmpty()) {
        Msg::timed(this, "wspr_sked2_help_vN.rtf not found in " + QDir::toNativeSeparators(Rx::defaultDataDir()),
                   "Help file", 3000);   // C#: "mainhelp.rtf not found"
        return;
    }
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        Msg::timed(this, "Can't read " + QDir::toNativeSeparators(file), "Help file", 3000);
        return;
    }
    const QByteArray data = f.readAll();

    if (!m_help) {
        m_help = new QWidget(nullptr, Qt::Window);   // a window of its own (C#: HelpForm), not kept on top
        m_help->setObjectName("helpWindow");
        m_help->setWindowIcon(windowIcon());
        auto *lay = new QVBoxLayout(m_help);
        lay->setContentsMargins(0, 0, 0, 0);
        m_helpText = new QTextBrowser(m_help);
        m_helpText->setObjectName("helpText");
        m_helpText->setOpenExternalLinks(true);
        lay->addWidget(m_helpText);
        QSize sz(860, 700);
        if (QScreen *s = screen())
            sz = sz.boundedTo(s->availableGeometry().size() * 0.9);
        m_help->resize(sz);
        // beside the main window if there's room, else over it
        if (QScreen *s = screen()) {
            const QRect avail = s->availableGeometry();
            QPoint at = frameGeometry().topRight() + QPoint(8, 0);
            if (at.x() + sz.width() > avail.right())
                at = frameGeometry().topLeft() + QPoint(40, 40);
            m_help->move(at);
        }
    }
    m_help->setWindowTitle("WSPR Scheduler 2 - Help - " + QDir::toNativeSeparators(file));   // which file
    QTextDocument *doc = m_helpText->document();
    doc->clear();
    if (file.endsWith(".rtf", Qt::CaseInsensitive)) {
        const Rtf::Html h = Rtf::toHtml(data);
        if (!h.ok) {
            Msg::timed(this, QDir::toNativeSeparators(file) + " is not an RTF file", "Help file", 3000);
            return;
        }
        for (const Rtf::Image &img : h.images)   // shrunk smoothly (see rtfhtml.h)
            doc->addResource(QTextDocument::ImageResource, QUrl(img.name), img.forDisplay(m_help->devicePixelRatioF()));
        m_helpText->setHtml(h.html);
    } else {
        m_helpText->setSearchPaths({QFileInfo(file).absolutePath()});
        m_helpText->setHtml(QString::fromUtf8(data));
    }
    m_helpText->moveCursor(QTextCursor::Start);
    m_help->show();
    m_help->raise();
    m_help->activateWindow();
}
