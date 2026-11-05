#pragma once
// Waterfall of the recording in progress, 1300-1700 Hz (C#: SpectrumForm).
// Follows the WAV file the receive window is writing, one line per 16384 samples (1.37 s).
//
// Same look and controls as the C# window: dark slate background, frequency scale with red lines at
// 50/100 Hz, cycle start marked by a white line and "HH:MM started on: <MHz> ^^", gain -40..+40 dB,
// mouse position shown in Hz and MHz, closing only hides it.
// Differences:
//  - the MHz under the mouse is worked out in double precision (C# used float: about 1 Hz steps at
//    14 MHz, 16 Hz at 144 MHz);
//  - when the window is narrower than the 547 bins, each pixel shows the strongest bin it covers
//    (C# picked one bin and could miss a signal);
//  - resizing keeps the picture (C# cleared it);
//  - the cycle time is the cycle the file belongs to (C# rounded the clock up to an even minute).
#include <QDateTime>
#include <QImage>
#include <QVector>
#include <QWidget>

class QLabel;
class QSlider;
class QTimer;

class WaterfallView : public QWidget
{
    Q_OBJECT
public:
    explicit WaterfallView(QWidget *parent = nullptr);
    void addLine(const QVector<float> &db);       // newest line at the top, older ones scroll down
    void markCycle(const QString &label);          // white line + label on the next line
    void clear();
    int lineCount() const { return m_lines; }
    QImage image() const { return m_img; }

signals:
    void hoverHz(double hz);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;

private:
    static constexpr int kScale = 22;              // height of the frequency scale strip
    struct Mark { int line; QString text; };
    QImage m_img;                                  // waterfall below the scale, newest row 0
    QVector<Mark> m_marks;
    int m_lines = 0;
};

class SpectrumForm : public QWidget
{
    Q_OBJECT
public:
    explicit SpectrumForm(QWidget *parent = nullptr);

    // C#: Spectrumform.wavPath = ...; cycleStart labels the new file
    void setWavPath(const QString &path, const QDateTime &cycleStart = QDateTime());
    void setFrequency(const QString &mhz);         // C#: Spectrumform.freq = ...
    void setAllowClose(bool a) { m_allowClose = a; }
    WaterfallView *view() const { return m_view; }
    void readNow();                                // one timer tick (tests)

protected:
    void closeEvent(QCloseEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    void ReadTimer_Tick();

    WaterfallView *m_view;
    QLabel *freqLabel, *MHzfreqLabel, *statusLabel, *gainLabel, *cycleTimeLabel;
    QSlider *gainControl;
    QTimer *readTimer;
    QString m_wavPath, m_lastWavPath, m_freq;
    QDateTime m_cycleStart;
    qint64 m_filePos = 0;
    QVector<float> m_fft;
    int m_fftPos = 0;
    float m_gainDb = 0.0f;
    QDateTime m_fileComplete;
    bool m_allowClose = false;
};
