#pragma once
// The message shown at start-up when the WSPR Scheduler databases can't be opened (C#: the LoadError
// form, shared by WSPR Scheduler and WSPR Scheduler Live). Here it is a label in the middle of the main
// window instead of a form of its own: hide() it once the database is there.
// Two messages, as C# WSPR Sked: the MySQL server isn't running at all, or it is but the databases /
// the admin user aren't there.
#include <QLabel>

class LoadErrorLabel : public QLabel
{
    Q_OBJECT
public:
    enum class Kind { ServerNotRunning, DatabasesMissing };

    // floats over `parent`, centred, above everything else; hidden until showError()
    LoadErrorLabel(const QString &appName, QWidget *parent);

    // mysqlError: what MySQL said (shown underneath, and used to choose the message);
    // keepTrying: add "keeps trying every minute" (WSPR Scheduler Live)
    void showError(const QString &mysqlError, bool keepTrying);
    void showText(const QString &text);   // any other message, in the same style

    static Kind kindFor(const QString &mysqlError);
    static QString text(const QString &appName, Kind kind);   // C#: label1.Text / the MessageBox

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    void centre();
    QString m_app;
};
