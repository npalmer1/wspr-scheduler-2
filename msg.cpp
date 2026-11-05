#include "msg.h"

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const char *kAppTitle = "WSPR Scheduler";

// The MessageForm look: OldLace, fixed size, text on top, small buttons bottom-right
class MessageDialog : public QDialog
{
public:
    MessageDialog(QWidget *parent, const QString &text, const QString &title)
        : QDialog(parent ? parent->window() : QApplication::activeWindow())
    {
        setWindowTitle(title.isEmpty() ? QString(kAppTitle) : title);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
        setObjectName("MessageDialog");
        setStyleSheet("QDialog#MessageDialog { background-color: oldlace; }");
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(12, 18, 12, 10);
        v->setSpacing(14);
        auto *label = new QLabel(text, this);
        label->setWordWrap(true);
        label->setMinimumWidth(280);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(label);
        m_buttons = new QHBoxLayout;
        m_buttons->addStretch();
        v->addLayout(m_buttons);
        v->setSizeConstraint(QLayout::SetFixedSize);   // FormBorderStyle.Fixed3D, no maximise
    }

    QPushButton *addButton(const QString &text, int result)
    {
        auto *b = new QPushButton(text, this);
        b->setAutoDefault(false);
        b->setMinimumWidth(48);
        connect(b, &QPushButton::clicked, this, [this, result] { done(result); });
        m_buttons->addWidget(b);
        return b;
    }

private:
    QHBoxLayout *m_buttons;
};

} // namespace

namespace Msg {

void timed(QWidget *parent, const QString &text, const QString &title, int ms)
{
    auto *d = new MessageDialog(parent, text, title);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->addButton("OK", QDialog::Accepted)->setDefault(true);
    d->setModal(false);
    QTimer::singleShot(qMax(ms, 300), d, &QDialog::close);
    d->show();
    d->raise();
}

void ok(QWidget *parent, const QString &text, const QString &title)
{
    MessageDialog d(parent, text, title);
    d.addButton("OK", QDialog::Accepted)->setDefault(true);   // Escape also closes (C#: CancelButton = OK)
    d.exec();
}

bool yesNo(QWidget *parent, const QString &text, const QString &title)
{
    MessageDialog d(parent, text, title);
    d.addButton("Yes", 1);
    d.addButton("No", 2)->setDefault(true);   // Enter on a destructive question = No
    return d.exec() == 1;                     // X / Escape = No
}

int addEditDelete(QWidget *parent, const QString &text, const QString &title)
{
    MessageDialog d(parent, text, title);
    d.addButton("Add", 1);
    d.addButton("Edit", 2);
    d.addButton("Del", 3);
    const int r = d.exec();
    return (r >= 1 && r <= 3) ? r : 0;        // X / Escape = 0 (nothing)
}

} // namespace Msg
