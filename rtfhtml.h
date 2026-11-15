#pragma once
// RTF to HTML, for the help file (C#: HelpForm showed main_help.rtf in a RichTextBox; Qt can't read RTF).
// Handles what WordPad and Word write for a help document: bold / italic / underline / strike,
// super/subscript, font size and colour, paragraph alignment and indents, bullets and numbering (as
// written in \pntext / \listtext), tabs, line breaks, tables (cells in a row side by side), hyperlinks
// (HYPERLINK fields) and PNG / JPEG pictures. Everything else (styles, headers, drawing objects,
// metafile pictures) is skipped rather than shown as code.
#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>

namespace Rtf {

struct Image
{
    QString name;   // used in <img src="...">: give it to QTextDocument::addResource
    QImage image;   // as stored in the file
    QSize shown;    // the size the document shows it at (the <img> width / height)

    // The picture for addResource: shrunk smoothly to the size it is shown at, at the screen's pixel
    // ratio. Given the full-size picture, QTextBrowser shrinks it by dropping pixels, which made fine
    // lines in a large picture look broken and grainy.
    QImage forDisplay(qreal devicePixelRatio = 1.0) const;
};

struct Html
{
    QString html;
    QList<Image> images;
    bool ok = false;   // false: not RTF (doesn't start with {\rtf)
};

Html toHtml(const QByteArray &rtf);

} // namespace Rtf
