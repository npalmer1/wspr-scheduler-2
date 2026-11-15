// Rtf::toHtml (help file): small hand-made cases, plus any RTF files named on the command line, which
// are converted, shown in a QTextBrowser and saved as <file>.png (and .html) for looking at.
#include "rtfhtml.h"

#include <QApplication>
#include <QFile>
#include <QTextBrowser>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static QString html(const char *rtf) { return Rtf::toHtml(QByteArray(rtf)).html; }

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    CHECK(!Rtf::toHtml("plain text").ok);
    QString h = html("{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}Hello {\\b bold} and {\\i italic}\\par}");
    CHECK(h.contains("Hello ") && h.contains("font-weight:bold\">bold</span>") && h.contains("font-style:italic\">italic"));
    CHECK(h.contains("font-family:'Arial'") && !h.contains("fonttbl") && !h.contains("Arial;"));
    // escapes, code page, unicode with its fallback character skipped, HTML special characters
    h = html("{\\rtf1\\ansi \\'93q\\'94 caf\\u233? \\{x\\} a\\\\b <tag> & \\emdash\\par}");
    CHECK(h.contains(QString::fromUtf8("“q” café {x} a\\b &lt;tag&gt; &amp; —")));
    CHECK(!h.contains("?"));
    // starred and unknown destinations are skipped, not shown
    h = html("{\\rtf1{\\*\\generator Riched20;}{\\stylesheet{\\s0 Normal;}}{\\info{\\title T}}Body\\par}");
    CHECK(h.contains("Body") && !h.contains("Riched") && !h.contains("Normal") && !h.contains(">T<"));
    // colour table, alignment, indents
    h = html("{\\rtf1{\\colortbl ;\\red255\\green0\\blue0;}\\pard\\qc\\li720 {\\cf1 red}\\par\\pard after\\par}");
    CHECK(h.contains("color:#ff0000\">red") && h.contains("text-align:center") && h.contains("margin-left:48px"));
    CHECK(h.count("<p ") == 2 && !h.section("<p ", 2).contains("center"));
    // Symbol-font bullets from \pntext
    h = html("{\\rtf1{\\fonttbl{\\f0 Calibri;}{\\f1 Symbol;}}{\\pntext\\f1\\'B7\\tab}One\\par}");
    CHECK(h.contains(QString::fromUtf8("•")) && h.contains("One"));
    // hyperlink field
    h = html("{\\rtf1{\\field{\\*\\fldinst{HYPERLINK \"https://example.org/x\"}}{\\fldrslt{link text}}} after\\par}");
    CHECK(h.contains("<a href=\"https://example.org/x\">link text</a>") && h.contains(" after") && !h.contains("HYPERLINK"));
    // table
    h = html("{\\rtf1\\trowd\\cellx1000\\cellx2000\\pard\\intbl A\\cell B\\cell\\row\\pard Next\\par}");
    CHECK(h.contains("<td valign=\"top\">A</td><td valign=\"top\">B</td>") && h.indexOf("</table>") < h.indexOf("Next"));
    // a PNG picture (1x1, red)
    const QByteArray png = QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==");
    const Rtf::Html p = Rtf::toHtml("{\\rtf1 x{\\*\\shppict{\\pict\\pngblip\\picwgoal300\\pichgoal150 " + png.toHex()
                                    + "}}{\\nonshppict{\\pict\\wmetafile8 0102}}y\\par}");
    CHECK(p.images.size() == 1 && !p.images[0].image.isNull());
    // shrunk smoothly to the size it is shown at (and at twice that on a 2x screen); never enlarged
    {
        Rtf::Image big{"x", QImage(400, 200, QImage::Format_RGB32), QSize(100, 50)};
        big.image.fill(Qt::white);
        CHECK(big.forDisplay(1.0).size() == QSize(100, 50) && big.forDisplay(2.0).size() == QSize(200, 100)
              && big.forDisplay(2.0).devicePixelRatio() == 2.0);
        Rtf::Image small{"y", QImage(50, 25, QImage::Format_RGB32), QSize(100, 50)};
        CHECK(small.forDisplay(1.0).size() == QSize(50, 25));
    }
    CHECK(p.html.contains("<img src=\"rtfimage1\" width=\"20\" height=\"10\">") && p.html.contains("x<img") && !p.html.contains("0102"));
    // a floating picture (Word: text wrapping on - a shape, often inside a SHAPE field); its settings
    // aren't text, and the \shprslt copy isn't shown twice
    const Rtf::Html sp = Rtf::toHtml(
        "{\\rtf1 a{\\field{\\*\\fldinst{SHAPE \\\\* MERGEFORMAT}}{\\fldrslt{{\\shp{\\*\\shpinst\\shpleft0\\shpright2115"
        "{\\sp{\\sn shapeType}{\\sv 75}}{\\sp{\\sn pib}{\\sv {\\pict\\picscalex50\\picscaley50\\pngblip\\picwgoal600\\pichgoal300 "
        + png.toHex() + "}}}}{\\shprslt{\\*\\do\\dobxcolumn{\\pict\\pngblip " + png.toHex() + "}}}}}}}b\\par}");
    CHECK(sp.images.size() == 1 && sp.html.contains("<img src=\"rtfimage1\" width=\"20\" height=\"10\">"));
    CHECK(!sp.html.contains("shapeType") && !sp.html.contains("75") && !sp.html.contains("MERGEFORMAT"));
    // sub/superscript, Word style and LibreOffice style
    h = html("{\\rtf1 H{\\sub 2}O x{\\up6 2} y{\\dn4 3}\\par}");
    CHECK(h.count("vertical-align:sub") == 2 && h.count("vertical-align:super") == 1);
    // \bin data skipped
    CHECK(html("{\\rtf1 a{\\*\\blob\\bin3 }}}b\\par}").contains("ab") || html("{\\rtf1 a\\bin3 xyzb\\par}").contains("ab"));

    // files given on the command line: render for a look
    for (int i = 1; i < argc; ++i) {
        QFile f(QString::fromLocal8Bit(argv[i]));
        if (!f.open(QIODevice::ReadOnly)) { std::printf("can't read %s\n", argv[i]); ++failures; continue; }
        const Rtf::Html r = Rtf::toHtml(f.readAll());
        CHECK(r.ok);
        QTextBrowser b;
        for (const Rtf::Image &img : r.images)
            b.document()->addResource(QTextDocument::ImageResource, QUrl(img.name), img.image);
        b.setHtml(r.html);
        b.resize(700, 900);
        b.show();
        QFile out(QString::fromLocal8Bit(argv[i]) + ".html");
        if (out.open(QIODevice::WriteOnly)) out.write(r.html.toUtf8());
        b.grab().save(QString::fromLocal8Bit(argv[i]) + ".png");
        std::printf("     %s: %d characters of HTML, %d pictures\n", argv[i], int(r.html.size()), int(r.images.size()));
    }
    std::printf(failures ? "rtf_test: %d FAILED\n" : "rtf_test: all passed\n", failures);
    return failures ? 1 : 0;
}
