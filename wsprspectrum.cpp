#include "wsprspectrum.h"

#include <algorithm>
#include <cmath>

namespace Spectrum {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void fft(std::vector<std::complex<double>> &a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * kPi / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

QVector<float> lineDb(const float *samples, float gainDb)
{
    static const std::vector<double> window = [] {
        std::vector<double> w(kFftSize);
        for (int i = 0; i < kFftSize; ++i)
            w[i] = 0.5 * (1 - std::cos(2 * kPi * i / (kFftSize - 1)));
        return w;
    }();
    std::vector<std::complex<double>> a(kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        a[i] = samples[i] * window[i];
    fft(a);

    const double res = double(kSampleRate) / kFftSize;
    const int binMin = int(kFreqMin / res), binMax = int(kFreqMax / res);
    QVector<float> db(binMax - binMin);
    for (int i = 0; i < db.size(); ++i) {
        const double mag = std::abs(a[binMin + i]) / kFftSize;   // NAudio's forward FFT scales by 1/N
        db[i] = float(20 * std::log10(mag + 1e-6)) + gainDb;
    }
    return db;
}

uint32_t colour(float db)
{
    const float norm = std::clamp((db + 80.0f) / 60.0f, 0.0f, 1.0f);
    auto rgb = [](int r, int g, int b) { return uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b); };
    if (norm < 0.2f) return rgb(0, 0, int(255 * (norm / 0.2f)));
    if (norm < 0.4f) return rgb(0, int(255 * ((norm - 0.2f) / 0.2f)), 255);
    if (norm < 0.6f) return rgb(0, 255, int(255 * (1 - (norm - 0.4f) / 0.2f)));
    if (norm < 0.8f) return rgb(int(255 * ((norm - 0.6f) / 0.2f)), 255, 0);
    return rgb(255, int(255 * (1 - (norm - 0.8f) / 0.2f)), 0);
}

void binsForPixel(int x, int w, int bins, int *first, int *last)
{
    *first = std::clamp(int(double(x) / w * bins), 0, bins - 1);
    *last = std::clamp(int(double(x + 1) / w * bins), *first + 1, bins);
}

} // namespace Spectrum
