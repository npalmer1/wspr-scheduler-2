#include "mainwindow.h"
#include <QApplication>
#include <QFont>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("G4GCI");
    QCoreApplication::setApplicationName("WSPR Sked");   // QSettings: db/host, db/user, db/password

    // Slightly smaller text than Qt's default (9 pt on Windows): the window keeps the C# form's fixed
    // control positions, and at 9 pt some button and label text ran past its edges.
    // Change with the setting ui/fontPointSize (e.g. 8.5); 0 = Qt's default.
    {
        const double pt = QSettings().value("ui/fontPointSize", 8.0).toDouble();
        if (pt > 0) {
            QFont f = QApplication::font();
            f.setPointSizeF(pt);
            QApplication::setFont(f);
        }
    }
    MainWindow w;
    w.show();
    return app.exec();
}
