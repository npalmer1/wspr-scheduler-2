#pragma once
// Number-crunching part of SpectrumForm.cs, without any drawing: 16384-point FFT of 12 kHz audio
// (Hann window, as C#), dB magnitudes of the WSPR window 1300-1700 Hz, and the C# colour scale.
// C# used NAudio.Dsp.FastFourierTransform; this is a plain radix-2 FFT giving the same magnitudes
// (NAudio's forward FFT divides by N; the same scaling is applied here so the colours match).
#include <QVector>
#include <complex>
#include <cstdint>
#include <vector>

namespace Spectrum {

constexpr int kSampleRate = 12000;
constexpr int kFftSize = 16384;              // 1.37 s per waterfall line, 0.73 Hz per bin
constexpr double kFreqMin = 1300.0;
constexpr double kFreqMax = 1700.0;

// In-place radix-2 FFT, n must be a power of two
void fft(std::vector<std::complex<double>> &a);

// One waterfall line: kFftSize samples (-1..1) -> dB per bin from kFreqMin to kFreqMax, plus gainDb.
// Same scale as C# (NAudio forward FFT normalises by N).
QVector<float> lineDb(const float *samples, float gainDb);

// C# MagnitudeToColor on norm = (dB + 80) / 60, clamped: black-blue-cyan-green-yellow-red. Returns 0xRRGGBB.
uint32_t colour(float db);

// Bin range [first, last) covering pixel column x of a w-pixel line (at least one bin)
void binsForPixel(int x, int w, int bins, int *first, int *last);

} // namespace Spectrum
