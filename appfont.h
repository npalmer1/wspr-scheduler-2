#pragma once
// The program's font, for the window and the tests alike (main.cpp, tests/mainwindow_uitest.cpp).
#include <QApplication>
#include <QFont>
#include <QSettings>

inline void applyAppFont()
{
    // Slightly smaller text than Qt's default (9 pt on Windows): the window keeps the C# form's fixed
    // control positions, and at 9 pt some button and label text ran past its edges.
    // Change with the setting ui/fontPointSize (e.g. 8.5); 0 = Qt's default.
    {
#ifndef Q_OS_WIN
        // The forms are laid out for Segoe UI (Windows), at fixed positions. Linux hasn't got it, and
        // Ubuntu's own font is wider, so text ran past the controls: use the closest common font
        // (Selawik is Microsoft's free stand-in; Liberation Sans comes with Ubuntu, else
        // sudo apt install fonts-liberation), for the whole window.
        QFont::insertSubstitutions("Segoe UI", {"Selawik", "Liberation Sans", "Arimo", "Noto Sans", "DejaVu Sans"});
        QFont base = QApplication::font();
        base.setFamilies({"Segoe UI"});
        QApplication::setFont(base);
#endif
        const double pt = QSettings().value("ui/fontPointSize", 8.0).toDouble();
        if (pt > 0) {
            QFont f = QApplication::font();
            f.setPointSizeF(pt);
            QApplication::setFont(f);
        }
    }
}
