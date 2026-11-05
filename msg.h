#pragma once
// Replacement for the C# MessageClass + MessageForm (Msg.TMessageBox / OKMessageBox / ynMessageBox /
// editMessageBox). Same look as MessageForm: OldLace background, fixed size, centred on the main window.
//
// Differences from the C# version:
//  - timed messages are shown on the UI thread (C# TMessageBox called ShowDialog from Task.Run, a
//    background thread, which is why they could appear behind the main window);
//  - no busy-wait after the dialog closes (C# looped on `while (!mForm.reply) {}`);
//  - closing with the window's X counts as OK / No / Cancel instead of being able to leave `reply` unset.
#include <QString>

class QWidget;

namespace Msg {
// Closes itself after ms milliseconds; does not block (C#: TMessageBox / TCMessageBox)
void timed(QWidget *parent, const QString &text, const QString &title, int ms);
// Blocking OK box (C#: OKMessageBox)
void ok(QWidget *parent, const QString &text, const QString &title = QString());
// Blocking Yes/No; true = Yes (C#: ynMessageBox)
bool yesNo(QWidget *parent, const QString &text, const QString &title = QString());
// Add / Edit / Del choice: 1 add, 2 edit, 3 delete, 0 closed (C#: editMessageBox)
int addEditDelete(QWidget *parent, const QString &text, const QString &title = QString());
} // namespace Msg
