// LoadErrorLabel - see loaderror.h.
#include "loaderror.h"

#include <QEvent>

LoadErrorLabel::Kind LoadErrorLabel::kindFor(const QString &mysqlError)
{
    const QString e = mysqlError.toLower();
    if (e.contains("can't connect") || e.contains("cannot connect") || e.contains("connection refused")
        || e.contains("lost connection") || e.contains("server has gone away") || e.contains("(2002)")
        || e.contains("(2003)"))
        return Kind::ServerNotRunning;
    return Kind::DatabasesMissing;
}

QString LoadErrorLabel::text(const QString &appName, Kind kind)
{
    if (kind == Kind::ServerNotRunning) {   // C#: checkMySQLDatabases - "MySQL Not Running"
#ifndef Q_OS_WIN
        return "MySQL server is not running. Start it in a terminal with:\n"
               "    sudo systemctl start mariadb    (or mysql)\n"
               "and to have it start whenever the PC starts:\n"
               "    sudo systemctl enable mariadb\n\n"
               "If it isn't installed:  sudo apt install mariadb-server  - then add the admin user and import\n"
               "the databases (wspr_db.sql): see the installation instructions.\n\n"
               "Once you have done this, please restart " + appName + ".";
#endif
        return "MySQL server is not running. You can either install XAMPP and import the databases\n"
               "...see the installation instructions to configure XAMPP.\n\n"
               "OR you can use MySQL Community as an alternative\n"
               "...you can run the 'wspr_mysql_setup.exe' installer to do this.\n\n"
               "Once you have done this, please restart " + appName + ".";
    }
    // C#: LoadError.resx label1.Text
    return "Error whilst loading!\n"
           "The app cannot connect to the mySQL databases!\n"
           "Ensure that you have mySQL installed and you have:\n"
           "1) imported the wspr scheduler databases - from the file\n"
           "\"wspr_db.sql\"\n"
           "2) ... and added the admin user.\n"
           "Please read the installation and configuration\n"
           "instructions to install and configure the databases.\n"
           "Then re-run " + appName + ".\n\n"
           "- and make sure that latest database updates have been applied.";
}

LoadErrorLabel::LoadErrorLabel(const QString &appName, QWidget *parent) : QLabel(parent), m_app(appName)
{
    setObjectName("loaderrorlabel");
    QFont f("Segoe UI");
    f.setPointSizeF(9.75);   // C#: label1
    setFont(f);
    setStyleSheet("QLabel#loaderrorlabel { background-color: lightgoldenrodyellow; color: black;"
                  " border: 1px solid #c8b860; padding: 12px; }");
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setWordWrap(true);
    hide();
    parent->installEventFilter(this);
}

void LoadErrorLabel::showError(const QString &mysqlError, bool keepTrying)
{
    QString t = text(m_app, kindFor(mysqlError));
    if (!mysqlError.trimmed().isEmpty())
        t += "\n\nMySQL said: " + mysqlError.trimmed().left(200);
    if (keepTrying)
        t += "\n\n" + m_app + " keeps trying every minute: this message goes once the database is there.";
    showText(t);
}

void LoadErrorLabel::showText(const QString &t)
{
    setText(t);
    setFixedWidth(qMin(560, parentWidget()->width() - 40));
    adjustSize();
    centre();
    show();
    raise();
}

void LoadErrorLabel::centre()
{
    const QSize p = parentWidget()->size();
    move(qMax(0, (p.width() - width()) / 2), qMax(0, (p.height() - height()) / 2));
}

bool LoadErrorLabel::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == parentWidget() && e->type() == QEvent::Resize && isVisible())
        centre();
    return false;
}
