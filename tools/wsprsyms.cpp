// Prints the 162 channel symbols for a WSJT-X style message, e.g.  wsprsyms "<G4GCI/P> IO90GU 23"
#include "wsprencode.h"
#include <QCoreApplication>
#include <QStringList>
#include <cstdio>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString err;
    const auto s = WsprEncode::encodeMessage(app.arguments().mid(1).join(' '), &err);
    if (!s) { std::printf("ERROR %s\n", qPrintable(err)); return 1; }
    for (uint8_t v : *s) std::putchar('0' + v);
    std::putchar('\n');
    return 0;
}
