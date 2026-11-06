// RTF to HTML for the help window - see rtfhtml.h.
#include "rtfhtml.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <cctype>

namespace {

// Windows-1252 0x80-0x9F (the rest of the code page is the same as Latin-1)
const char16_t kCp1252[32] = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};

QChar fromCodePage(int b, bool symbolFont)
{
    if (symbolFont) {   // the Symbol font's bullets, as used for lists
        if (b == 0xB7) return QChar(0x2022);
        if (b == 0xA7) return QChar(0x25AA);
    }
    if (b >= 0x80 && b < 0xA0)
        return QChar(kCp1252[b - 0x80]);
    return QChar(b);
}

enum class Dest { Normal, Skip, FontTable, ColorTable, Pict, FieldInst };

struct CharFmt
{
    bool bold = false, italic = false, underline = false, strike = false, super = false, sub = false;
    int fs = 24;      // half-points
    int color = 0;    // colour table index, 0 = automatic
    int font = -1;
    bool operator==(const CharFmt &o) const
    {
        return bold == o.bold && italic == o.italic && underline == o.underline && strike == o.strike
               && super == o.super && sub == o.sub && fs == o.fs && color == o.color && font == o.font;
    }
};

struct ParaFmt
{
    QString align;    // left / center / right / justify
    int li = 0, fi = 0, ri = 0, sb = 0, sa = 0;   // twips
    bool inTable = false;
};

struct State
{
    Dest dest = Dest::Normal;
    CharFmt chr;
    ParaFmt para;
    int uc = 1;
    QString link;
    // picture
    bool png = false, jpeg = false;
    int picw = 0, pich = 0, picwgoal = 0, pichgoal = 0, scalex = 100, scaley = 100;
};

class Converter
{
public:
    explicit Converter(const QByteArray &in) : m_in(in) {}
    Rtf::Html run();

private:
    void controlWord(const QString &word, bool hasArg, int arg);
    void text(QChar c);
    void flushRun();
    void endParagraph(bool forceEmpty = false);
    void endCell();
    void endRow();
    void closeTable();
    void groupEnd(const State &closing);
    QString runStyle(const CharFmt &f) const;
    bool symbolFont() const { return m_fontNames.value(st().chr.font).contains("Symbol", Qt::CaseInsensitive); }
    State &st() { return m_stack.last(); }
    const State &st() const { return m_stack.last(); }

    const QByteArray &m_in;
    int m_pos = 0;
    QVector<State> m_stack;
    int m_skipChars = 0;            // characters still to skip after \uN
    bool m_starred = false;         // \* seen: the next control word starts a destination
    // tables
    QHash<int, QString> m_fontNames;
    int m_tableFont = -1;
    QString m_fontText;
    QStringList m_colors;           // an empty entry (usually the first) = automatic
    int m_r = 0, m_g = 0, m_b = 0;
    bool m_rgbSeen = false;
    // pictures
    QByteArray m_hex;
    // fields
    QString m_fieldInst;
    // output
    QString m_run;                  // text not yet written, all in one format
    CharFmt m_runFmt;
    QString m_runLink;
    QString m_para;                 // the paragraph so far (HTML)
    QString m_cell;                 // table cell so far (paragraphs inside a cell)
    QStringList m_cells;            // cells of this row
    QString m_table;                // rows so far
    bool m_inTableHtml = false;
    QString m_out;
    QList<Rtf::Image> m_images;
    int m_defaultFont = 0;
};

QString Converter::runStyle(const CharFmt &f) const
{
    QStringList s;
    if (f.bold) s << "font-weight:bold";
    if (f.italic) s << "font-style:italic";
    if (f.underline && f.strike) s << "text-decoration:underline line-through";
    else if (f.underline) s << "text-decoration:underline";
    else if (f.strike) s << "text-decoration:line-through";
    if (f.super) s << "vertical-align:super";
    if (f.sub) s << "vertical-align:sub";
    if (f.fs != 24) s << QString("font-size:%1pt").arg(f.fs / 2.0);
    if (f.color >= 0 && f.color < m_colors.size() && !m_colors[f.color].isEmpty()) s << "color:" + m_colors[f.color];
    if (f.font >= 0 && f.font != m_defaultFont && m_fontNames.contains(f.font)
        && !m_fontNames[f.font].contains("Symbol", Qt::CaseInsensitive))
        s << "font-family:'" + m_fontNames[f.font] + "'";
    return s.join(';');
}

void Converter::flushRun()
{
    if (m_run.isEmpty())
        return;
    QString t = m_run.toHtmlEscaped();
    t.replace("  ", "&nbsp; ");
    const QString style = runStyle(m_runFmt);
    if (!style.isEmpty())
        t = "<span style=\"" + style + "\">" + t + "</span>";
    if (!m_runLink.isEmpty())
        t = "<a href=\"" + m_runLink.toHtmlEscaped() + "\">" + t + "</a>";
    m_para += t;
    m_run.clear();
}

void Converter::text(QChar c)
{
    if (m_skipChars > 0) {
        --m_skipChars;
        return;
    }
    switch (st().dest) {
    case Dest::Skip:
        return;
    case Dest::FontTable:
        m_fontText += c;
        return;
    case Dest::ColorTable:
        if (c == ';') {
            m_colors << (m_rgbSeen ? QString("#%1%2%3").arg(m_r, 2, 16, QChar('0')).arg(m_g, 2, 16, QChar('0')).arg(m_b, 2, 16, QChar('0'))
                                   : QString());
            m_r = m_g = m_b = 0;
            m_rgbSeen = false;
        }
        return;
    case Dest::Pict:
        if (std::isxdigit(c.unicode() & 0x7F) && c.unicode() < 0x80)
            m_hex += char(c.unicode());
        return;
    case Dest::FieldInst:
        m_fieldInst += c;
        return;
    case Dest::Normal:
        break;
    }
    if (!(m_runFmt == st().chr) || m_runLink != st().link) {
        flushRun();
        m_runFmt = st().chr;
        m_runLink = st().link;
    }
    m_run += c;
}

void Converter::endParagraph(bool forceEmpty)
{
    flushRun();
    const ParaFmt &p = st().para;
    if (p.inTable) {   // a paragraph inside a table cell
        m_cell += (m_cell.isEmpty() ? QString() : QString("<br>")) + m_para;
        m_para.clear();
        return;
    }
    closeTable();
    QStringList s;
    s << QString("margin-top:%1px").arg(p.sb / 15) << QString("margin-bottom:%1px").arg(p.sa / 15);
    if (p.li) s << QString("margin-left:%1px").arg(p.li / 15);
    if (p.ri) s << QString("margin-right:%1px").arg(p.ri / 15);
    if (p.fi) s << QString("text-indent:%1px").arg(p.fi / 15);
    if (!p.align.isEmpty()) s << "text-align:" + p.align;
    const QString body = m_para.isEmpty() && !forceEmpty ? QString("&nbsp;") : m_para;
    m_out += "<p style=\"" + s.join(';') + "\">" + body + "</p>\n";
    m_para.clear();
}

void Converter::endCell()
{
    flushRun();
    m_cell += (m_cell.isEmpty() || m_para.isEmpty() ? QString() : QString("<br>")) + m_para;
    m_para.clear();
    m_cells << m_cell;
    m_cell.clear();
}

void Converter::endRow()
{
    flushRun();
    if (!m_para.isEmpty() || !m_cell.isEmpty())
        endCell();
    if (!m_inTableHtml) {
        m_table = "<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" style=\"border-collapse:collapse;margin:6px 0\">\n";
        m_inTableHtml = true;
    }
    m_table += "<tr>";
    for (const QString &c : std::as_const(m_cells))
        m_table += "<td valign=\"top\">" + (c.isEmpty() ? QString("&nbsp;") : c) + "</td>";
    m_table += "</tr>\n";
    m_cells.clear();
}

void Converter::closeTable()
{
    if (!m_inTableHtml)
        return;
    m_out += m_table + "</table>\n";
    m_table.clear();
    m_inTableHtml = false;
}

void Converter::groupEnd(const State &closing)
{
    if (closing.dest == Dest::FontTable && m_tableFont >= 0) {
        // {\f1\fswiss Arial;} - the name is the text before ';'
        QString name = m_fontText;
        const int semi = name.indexOf(';');
        if (semi >= 0)
            name = name.left(semi);
        m_fontNames.insert(m_tableFont, name.trimmed());
        m_fontText.clear();
        m_tableFont = -1;
    } else if (closing.dest == Dest::Pict && !m_stack.isEmpty() && st().dest != Dest::Pict) {
        if ((closing.png || closing.jpeg) && !m_hex.isEmpty()) {
            QImage img;
            if (img.loadFromData(QByteArray::fromHex(m_hex))) {
                // shown at the size set in the document (twips, 15 to a pixel), else the picture's own
                int w = closing.picwgoal > 0 ? closing.picwgoal / 15 : img.width();
                int h = closing.pichgoal > 0 ? closing.pichgoal / 15 : img.height();
                w = w * closing.scalex / 100;
                h = h * closing.scaley / 100;
                const QString name = QString("rtfimage%1").arg(m_images.size() + 1);
                m_images << Rtf::Image{name, img};
                flushRun();
                m_para += QString("<img src=\"%1\" width=\"%2\" height=\"%3\">").arg(name).arg(w).arg(h);
            }
        }
        m_hex.clear();
    }
}

void Converter::controlWord(const QString &w, bool hasArg, int arg)
{
    State &s = st();
    if (m_starred) {
        m_starred = false;
        if (w == "shppict") return;   // Word's picture (the \nonshppict copy is skipped)
        if (w == "fldinst") {
            s.dest = Dest::FieldInst;
            m_fieldInst.clear();
            return;
        }
        s.dest = Dest::Skip;
        return;
    }
    if (s.dest == Dest::Skip)
        return;

    // destinations
    static const QStringList skip = {"stylesheet", "info", "header", "headerl", "headerr", "headerf", "footer",
                                     "footerl", "footerr", "footerf", "footnote", "nonshppict", "object", "listtable",
                                     "listoverridetable", "rsidtbl", "revtbl", "xmlnstbl", "filetbl", "pgdsctbl",
                                     "themedata", "colorschememapping", "datastore", "latentstyles", "mmathPr",
                                     "generator", "userprops", "docvar", "bkmkstart", "bkmkend", "pn", "annotation",
                                     "atnid", "atnauthor", "comment", "private", "template", "xe", "tc"};
    if (skip.contains(w)) { s.dest = Dest::Skip; return; }
    if (w == "fonttbl") { s.dest = Dest::FontTable; return; }
    if (w == "colortbl") { s.dest = Dest::ColorTable; return; }
    if (w == "pict") {
        s.dest = Dest::Pict;
        s.png = s.jpeg = false;
        s.picw = s.pich = s.picwgoal = s.pichgoal = 0;
        s.scalex = s.scaley = 100;
        m_hex.clear();
        return;
    }
    if (w == "fldrslt") {
        static const QRegularExpression re("HYPERLINK\\s+\"([^\"]+)\"");
        const auto m = re.match(m_fieldInst);
        QString url = m.hasMatch() ? m.captured(1) : QString();
        if (!url.isEmpty() && !url.contains(':'))
            url = "https://" + url;
        s.link = url;
        s.dest = Dest::Normal;
        return;
    }

    switch (s.dest) {
    case Dest::FontTable:
        if (w == "f" && hasArg) m_tableFont = arg;
        return;
    case Dest::ColorTable:
        if (w == "red") { m_r = arg; m_rgbSeen = true; }
        else if (w == "green") { m_g = arg; m_rgbSeen = true; }
        else if (w == "blue") { m_b = arg; m_rgbSeen = true; }
        return;
    case Dest::Pict:
        if (w == "pngblip") s.png = true;
        else if (w == "jpegblip") s.jpeg = true;
        else if (w == "picw") s.picw = arg;
        else if (w == "pich") s.pich = arg;
        else if (w == "picwgoal") s.picwgoal = arg;
        else if (w == "pichgoal") s.pichgoal = arg;
        else if (w == "picscalex" && arg > 0) s.scalex = arg;
        else if (w == "picscaley" && arg > 0) s.scaley = arg;
        return;
    case Dest::FieldInst:
    case Dest::Skip:
        return;
    case Dest::Normal:
        break;
    }

    const bool on = !hasArg || arg != 0;
    CharFmt &c = s.chr;
    ParaFmt &p = s.para;
    if (w == "par") endParagraph();
    else if (w == "line") { flushRun(); m_para += "<br>"; }
    else if (w == "page" || w == "sect") endParagraph();
    else if (w == "tab") text(QChar(0x2003));   // em space
    else if (w == "pard") p = ParaFmt();
    else if (w == "plain") { c = CharFmt(); c.font = m_defaultFont; }
    else if (w == "b") c.bold = on;
    else if (w == "i") c.italic = on;
    else if (w == "ul" || w == "uld" || w == "uldb" || w == "ulw" || w == "ulth" || w == "uldash") c.underline = on;
    else if (w == "ulnone") c.underline = false;
    else if (w == "strike" || w == "striked") c.strike = on;
    else if (w == "super") { c.super = true; c.sub = false; }
    else if (w == "sub") { c.sub = true; c.super = false; }
    else if (w == "nosupersub") c.super = c.sub = false;
    else if (w == "up") { c.super = !hasArg || arg != 0; c.sub = false; }   // raised by half-points (LibreOffice)
    else if (w == "dn") { c.sub = !hasArg || arg != 0; c.super = false; }
    else if (w == "fs" && hasArg && arg > 0) c.fs = arg;
    else if (w == "cf") c.color = arg;
    else if (w == "f") c.font = arg;
    else if (w == "deff") m_defaultFont = arg;
    else if (w == "ql") p.align = "left";
    else if (w == "qc") p.align = "center";
    else if (w == "qr") p.align = "right";
    else if (w == "qj") p.align = "justify";
    else if (w == "li") p.li = arg;
    else if (w == "fi") p.fi = arg;
    else if (w == "ri") p.ri = arg;
    else if (w == "sb") p.sb = arg;
    else if (w == "sa") p.sa = arg;
    else if (w == "intbl") p.inTable = true;
    else if (w == "cell") endCell();
    else if (w == "row") endRow();
    else if (w == "uc") s.uc = arg;
    else if (w == "u") {
        text(QChar(char16_t(arg < 0 ? arg + 65536 : arg)));
        m_skipChars = s.uc;
    }
    else if (w == "emdash") text(QChar(0x2014));
    else if (w == "endash") text(QChar(0x2013));
    else if (w == "bullet") text(QChar(0x2022));
    else if (w == "lquote") text(QChar(0x2018));
    else if (w == "rquote") text(QChar(0x2019));
    else if (w == "ldblquote") text(QChar(0x201C));
    else if (w == "rdblquote") text(QChar(0x201D));
}

Rtf::Html Converter::run()
{
    Rtf::Html result;
    if (!m_in.trimmed().startsWith("{\\rtf"))
        return result;
    m_stack << State();
    const int n = m_in.size();
    while (m_pos < n) {
        const char ch = m_in[m_pos];
        if (ch == '{') {
            ++m_pos;
            if (st().dest == Dest::Normal)
                flushRun();
            State copy = st();
            m_stack << copy;
            continue;
        }
        if (ch == '}') {
            ++m_pos;
            if (m_stack.size() <= 1)
                break;   // end of the document
            if (st().dest == Dest::Normal)
                flushRun();
            const State closing = m_stack.takeLast();
            groupEnd(closing);
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            ++m_pos;
            continue;
        }
        if (ch != '\\') {
            ++m_pos;
            text(QChar(uchar(ch)));
            continue;
        }
        // control symbol or word
        ++m_pos;
        if (m_pos >= n)
            break;
        const char c = m_in[m_pos];
        if (c == '\\' || c == '{' || c == '}') { ++m_pos; text(QLatin1Char(c)); continue; }
        if (c == '*') { ++m_pos; m_starred = true; continue; }
        if (c == '~') { ++m_pos; text(QChar(0x00A0)); continue; }
        if (c == '_') { ++m_pos; text(QChar(0x2011)); continue; }
        if (c == '-') { ++m_pos; continue; }
        if (c == '\r' || c == '\n') { ++m_pos; controlWord("par", false, 0); continue; }
        if (c == '\'') {
            const int v = m_pos + 2 < n ? QByteArray(m_in.mid(m_pos + 1, 2)).toInt(nullptr, 16) : 0;
            m_pos += 3;
            if (m_skipChars > 0) { --m_skipChars; continue; }
            if (st().dest == Dest::Pict) continue;
            text(fromCodePage(v, symbolFont()));
            continue;
        }
        if (!std::isalpha(uchar(c))) { ++m_pos; continue; }
        int start = m_pos;
        while (m_pos < n && std::isalpha(uchar(m_in[m_pos])))
            ++m_pos;
        const QString word = QString::fromLatin1(m_in.mid(start, m_pos - start));
        bool hasArg = false;
        int arg = 0;
        if (m_pos < n && (m_in[m_pos] == '-' || std::isdigit(uchar(m_in[m_pos])))) {
            start = m_pos;
            ++m_pos;
            while (m_pos < n && std::isdigit(uchar(m_in[m_pos])))
                ++m_pos;
            arg = m_in.mid(start, m_pos - start).toInt();
            hasArg = true;
        }
        if (m_pos < n && m_in[m_pos] == ' ')
            ++m_pos;   // the space ending a control word is part of it
        if (word == "bin" && hasArg) {   // binary data: skip it
            m_pos += qMax(0, arg);
            continue;
        }
        controlWord(word, hasArg, arg);
    }
    while (m_stack.size() > 1)
        m_stack.removeLast();
    flushRun();
    if (!m_para.isEmpty())
        endParagraph();
    closeTable();

    QString body = "font-size:12pt";
    if (m_fontNames.contains(m_defaultFont))
        body += ";font-family:'" + m_fontNames[m_defaultFont] + "'";
    result.html = "<html><body style=\"" + body + "\">\n" + m_out + "</body></html>";
    result.images = m_images;
    result.ok = true;
    return result;
}

} // namespace

namespace Rtf {
Html toHtml(const QByteArray &rtf)
{
    return Converter(rtf).run();
}
} // namespace Rtf
