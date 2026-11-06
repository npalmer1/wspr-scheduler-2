// Help (F1, or a click on "F1 Help" by the clock). C#: ShowHelp + HelpForm - main_help.rtf in a window
// of its own. Qt has no RTF reader, so the file is converted to HTML (rtfhtml.*) each time it is opened,
// which also picks up any edits to the file.
//
// The help file is wspr_sked2_help.rtf in C:\WSPR_Sked (the installer puts it there, and never over
// an edited copy), so it can be edited in WordPad without administrator rights. Its own name keeps it
// apart from the C# version's main_help.rtf in the same folder. A copy next to the program is used if
// there is none in C:\WSPR_Sked; wspr_sked2_help.html is used too, if there is no .rtf.
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "msg.h"
#include "rtfhtml.h"
#include "rxspot.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
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
    const QStringList dirs{Rx::defaultDataDir(), QCoreApplication::applicationDirPath()};
    for (const QString &name : {QStringLiteral("wspr_sked2_help.rtf"), QStringLiteral("wspr_sked2_help.html")})
        for (const QString &d : dirs) {
            const QString p = QDir(d).filePath(name);
            if (QFileInfo::exists(p))
                return p;
        }
    return QString();
}

void MainWindow::showHelp()
{
    const QString file = helpFile();
    if (file.isEmpty()) {
        Msg::timed(this, "wspr_sked2_help.rtf not found in " + QDir::toNativeSeparators(Rx::defaultDataDir()),
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
        m_help->setWindowTitle("WSPR Scheduler 2 - Help");
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
    QTextDocument *doc = m_helpText->document();
    doc->clear();
    if (file.endsWith(".rtf", Qt::CaseInsensitive)) {
        const Rtf::Html h = Rtf::toHtml(data);
        if (!h.ok) {
            Msg::timed(this, QDir::toNativeSeparators(file) + " is not an RTF file", "Help file", 3000);
            return;
        }
        for (const Rtf::Image &img : h.images)
            doc->addResource(QTextDocument::ImageResource, QUrl(img.name), img.image);
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
