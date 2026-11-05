#include "spectrumform.h"

#include "rxspot.h"
#include "wsprspectrum.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QtEndian>
#include <cstring>

namespace {
const QColor kSlate(47, 79, 79);   // Color.DarkSlateGray

double xForHz(double hz, int w)
{
    return (hz - Spectrum::kFreqMin) / (Spectrum::kFreqMax - Spectrum::kFreqMin) * w;
}
} // namespace

// ---- WaterfallView ------------------------------------------------------------------------

WaterfallView::WaterfallView(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(200, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void WaterfallView::resizeEvent(QResizeEvent *)
{
    const QSize s(qMax(1, width()), qMax(1, height() - kScale));
    if (m_img.size() == s)
        return;
    QImage n(s, QImage::Format_RGB32);
    n.fill(Qt::black);
    if (!m_img.isNull()) {   // keep the history: stretch sideways, keep rows
        QPainter p(&n);
        p.drawImage(QRect(0, 0, s.width(), m_img.height()), m_img);
    }
    m_img = n;
}

void WaterfallView::clear()
{
    m_img.fill(Qt::black);
    m_marks.clear();
    update();
}

void WaterfallView::addLine(const QVector<float> &db)
{
    if (m_img.isNull() || db.isEmpty())
        return;
    const int w = m_img.width(), h = m_img.height();
    const qsizetype bpl = m_img.bytesPerLine();
    uchar *bits = m_img.bits();
    if (h > 1)
        std::memmove(bits + bpl, bits, size_t(bpl) * (h - 1));
    auto *row = reinterpret_cast<QRgb *>(m_img.scanLine(0));
    for (int x = 0; x < w; ++x) {
        int a, b;
        Spectrum::binsForPixel(x, w, db.size(), &a, &b);
        float best = db[a];
        for (int i = a + 1; i < b; ++i)
            best = qMax(best, db[i]);
        row[x] = 0xff000000u | Spectrum::colour(best);
    }
    ++m_lines;
    // forget marks that have scrolled off the bottom
    while (!m_marks.isEmpty() && m_lines - m_marks.first().line > h + 20)
        m_marks.removeFirst();
    update();
}

void WaterfallView::markCycle(const QString &label)
{
    m_marks.append({m_lines, label});
    update();
}

void WaterfallView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int w = width();
    p.fillRect(0, 0, w, kScale, kSlate);
    p.drawImage(0, kScale, m_img);

    // frequency scale and lines through the waterfall (C#: DrawFrequencyScale)
    QFont big("Arial", 9, QFont::Bold), small("Arial", 7);
    for (int f = 1300; f <= 1700; f += 10) {
        const double x = xForHz(f, w);
        if (f % 100 == 0) {
            p.setPen(Qt::red);
            p.drawLine(QPointF(x, 14), QPointF(x, kScale));
            p.setPen(QColor(255, 0, 0, 200));
            p.drawLine(QPointF(x, kScale), QPointF(x, height()));
            p.setFont(big);
            p.setPen(Qt::yellow);
            const double lx = f == 1300 ? 2 : f == 1700 ? x - 30 : x - 14;
            p.drawText(QPointF(lx, 12), QString::number(f));
        } else if (f % 50 == 0) {
            p.setPen(Qt::red);
            p.drawLine(QPointF(x, 16), QPointF(x, kScale));
            p.setPen(QColor(200, 0, 0, 120));
            p.drawLine(QPointF(x, kScale), QPointF(x, height()));
            p.setFont(small);
            p.setPen(Qt::yellow);
            p.drawText(QPointF(x - 10, 12), QString::number(f));
        } else {
            p.setPen(QColor(139, 0, 0));
            p.drawLine(QPointF(x, 18), QPointF(x, kScale));
            p.setPen(QColor(100, 0, 0, 40));
            for (int y = kScale; y < height(); y += 10)
                p.drawLine(QPointF(x, y), QPointF(x, y + 2));
        }
    }

    // cycle starts: white line, label scrolling with it (C#: DrawCycleLine + time label)
    QFont lf("Segoe UI", 8, QFont::Bold);
    p.setFont(lf);
    const QFontMetrics fm(lf);
    for (const Mark &m : std::as_const(m_marks)) {
        const int y = kScale + (m_lines - m.line);
        if (y > height())
            continue;
        p.fillRect(0, y - 1, w, 3, Qt::white);
        const int ly = y - 12;
        const int bg = qMax(fm.horizontalAdvance(m.text) + 4, int(xForHz(1350, w)) + 6);
        p.fillRect(0, ly, bg, 12, kSlate);
        p.setPen(Qt::yellow);
        p.drawText(QRect(1, ly, bg, 12), Qt::AlignLeft | Qt::AlignVCenter, m.text);
    }
}

void WaterfallView::mouseMoveEvent(QMouseEvent *e)
{
    emit hoverHz(Spectrum::kFreqMin + e->position().x() * (Spectrum::kFreqMax - Spectrum::kFreqMin) / qMax(1, width()));
}

// ---- SpectrumForm -------------------------------------------------------------------------

SpectrumForm::SpectrumForm(QWidget *parent)
    : QWidget(parent, Qt::Window), m_view(new WaterfallView(this)), readTimer(new QTimer(this)),
      m_fft(Spectrum::kFftSize, 0.0f)
{
    setWindowTitle("WSPR Spectrum");
    resize(1200, 590);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, kSlate);
    setPalette(pal);
    setAutoFillBackground(true);

    auto mk = [this](const QString &text, const QColor &c, int pt = 0) {
        auto *l = new QLabel(text, this);
        QFont f = l->font();
        f.setBold(true);
        if (pt)
            f.setPointSize(pt);
        l->setFont(f);
        l->setStyleSheet(QString("color: %1;").arg(c.name()));
        return l;
    };
    freqLabel = mk("1300 - 1700 Hz  (WSPR)", Qt::cyan, 8);
    MHzfreqLabel = mk("...", QColor(255, 165, 0), 8);
    cycleTimeLabel = mk("00:00:00", Qt::yellow);
    statusLabel = mk("Waiting for WAV file...", Qt::yellow);
    gainLabel = mk("Gain: 0 dB", Qt::white);
    freqLabel->setObjectName("freqLabel");
    MHzfreqLabel->setObjectName("MHzfreqLabel");
    statusLabel->setObjectName("statusLabel");
    gainLabel->setObjectName("gainLabel");
    cycleTimeLabel->setObjectName("cycleTimeLabel");
    freqLabel->setMinimumWidth(150);
    statusLabel->setMinimumWidth(220);

    gainControl = new QSlider(Qt::Horizontal, this);
    gainControl->setObjectName("gainControl");
    gainControl->setRange(-40, 40);
    gainControl->setValue(0);
    gainControl->setTickInterval(10);
    gainControl->setTickPosition(QSlider::TicksBelow);
    gainControl->setFixedWidth(160);
    connect(gainControl, &QSlider::valueChanged, this, [this](int v) {
        m_gainDb = float(v);
        gainLabel->setText(QString("Gain: %1 dB").arg(v > 0 ? "+" + QString::number(v) : QString::number(v)));
    });

    auto *left = new QVBoxLayout;
    left->setSpacing(0);
    left->addWidget(freqLabel);
    left->addWidget(MHzfreqLabel);
    auto *top = new QHBoxLayout;
    top->addLayout(left);
    top->addSpacing(10);
    top->addWidget(cycleTimeLabel);
    top->addSpacing(30);
    top->addWidget(statusLabel);
    top->addStretch();
    top->addWidget(gainLabel);
    top->addWidget(gainControl);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(10, 4, 10, 10);
    v->addLayout(top);
    v->addWidget(m_view, 1);

    connect(m_view, &WaterfallView::hoverHz, this, [this](double hz) {
        freqLabel->setText(QString("%1 Hz").arg(hz, 0, 'f', 1));
        bool ok = false;
        const double dial = m_freq.toDouble(&ok);
        if (ok && dial > 0)
            MHzfreqLabel->setText(QString("%1 MHz").arg(dial + hz / 1e6, 0, 'f', 6));
    });

    readTimer->setInterval(500);
    connect(readTimer, &QTimer::timeout, this, &SpectrumForm::ReadTimer_Tick);
}

void SpectrumForm::setWavPath(const QString &path, const QDateTime &cycleStart)
{
    m_wavPath = path;
    m_cycleStart = cycleStart.isValid() ? cycleStart : Rx::cycleStart(QDateTime::currentDateTimeUtc());
}

void SpectrumForm::setFrequency(const QString &mhz) { m_freq = mhz; }

void SpectrumForm::closeEvent(QCloseEvent *e)
{
    if (m_allowClose) {
        e->accept();
        return;
    }
    e->ignore();   // C#: FormClosing -> Cancel, Hide
    hide();
}

void SpectrumForm::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    readTimer->start();
}

void SpectrumForm::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    readTimer->stop();
}

void SpectrumForm::readNow() { ReadTimer_Tick(); }

void SpectrumForm::ReadTimer_Tick()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    cycleTimeLabel->setText(now.toString("HH:mm:ss") + " utc   (" + QDateTime::currentDateTime().toString("HH:mm") + " lt)");
    if (m_wavPath.isEmpty() || !QFileInfo::exists(m_wavPath)) {
        statusLabel->setText("Waiting for WAV file...");
        return;
    }
    if (m_wavPath != m_lastWavPath) {
        m_lastWavPath = m_wavPath;
        m_filePos = 0;
        m_fftPos = 0;
        m_fileComplete = QDateTime();
        m_view->markCycle(m_cycleStart.toString("HH:mm") + " started on:  " + m_freq + " ^^");
        statusLabel->setText("New file: " + QFileInfo(m_wavPath).fileName());
    }

    QFile f(m_wavPath);
    if (!f.open(QIODevice::ReadOnly)) {
        statusLabel->setText("Error: " + f.errorString());
        return;
    }
    if (m_filePos == 0)
        m_filePos = 44;   // skip the WAV header
    const qint64 available = f.size() - m_filePos;
    if (available < 2) {
        if (!m_fileComplete.isValid())
            m_fileComplete = now;
        statusLabel->setText(QString("Idle or TX in progress (%1s)").arg(m_fileComplete.secsTo(now)));
        return;
    }
    m_fileComplete = QDateTime();
    f.seek(m_filePos);
    const QByteArray data = f.read(available & ~qint64(1));
    m_filePos += data.size();
    statusLabel->setText(QString("%1  %2KB").arg(QFileInfo(m_wavPath).fileName()).arg(m_filePos / 1024));

    const auto *s = reinterpret_cast<const qint16 *>(data.constData());
    const int n = int(data.size() / 2);
    for (int i = 0; i < n; ++i) {
        m_fft[m_fftPos++] = qFromLittleEndian(s[i]) / 32768.0f;
        if (m_fftPos >= Spectrum::kFftSize) {
            m_fftPos = 0;
            m_view->addLine(Spectrum::lineDb(m_fft.constData(), m_gainDb));
        }
    }
}
