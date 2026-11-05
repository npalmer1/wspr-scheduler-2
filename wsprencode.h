#pragma once
// WSPR type 1 encoder - C++ port of WsprSharp Encode.cs
// (MIT License, Copyright (c) 2021 Scott W Harden; modifications N. Palmer 2025)
//
// Same algorithm and same results as the C# version. One difference: the C# Sanitize*
// functions swallowed their own exceptions and carried on with a half-cleaned callsign
// (which then crashed in WsprCode); here every function reports the problem via *error.
#include <QString>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace WsprEncode {

constexpr int kBitCount = 162;     // WSPR_BIT_COUNT
constexpr int kMessageSize = 11;   // WSPR_MESSAGE_SIZE

using Symbols = std::array<uint8_t, kBitCount>;   // tone numbers 0..3

// Full pipeline: callsign + 4-char locator + power -> 162 tone numbers
std::optional<Symbols> encodeType1(const QString &callsign, const QString &locator, double powerdBm,
                                   QString *error = nullptr);

// The individual steps, as in Encode.cs
std::optional<std::array<uint8_t, 7>> messageBytes(const QString &callsign, const QString &locator,
                                                   double powerdBm, QString *error = nullptr);   // GetMessageBytes
std::optional<QString> sanitizeCallsign(const QString &callsign, QString *error = nullptr);   // 6 chars, padded
std::optional<QString> sanitizeLocation(const QString &locator, QString *error = nullptr);    // 4 chars
uint8_t sanitizePower(double powerdBm);                    // nearest valid level at or below
const std::vector<uint8_t> &validPowerLevels();            // GetValidPowerLevels
QString powerDescription(uint8_t dBm);                     // GetPowerDescription
std::optional<uint8_t> wsprCode(QChar c);                  // WsprCode

Symbols convolve(const std::array<uint8_t, 7> &data);      // Convolve
Symbols interleave(const Symbols &data);                   // Interleave
Symbols integrateSync(const Symbols &data);                // IntegrateSyncValues

// Type 2/3 helpers kept from Encode.cs
QString sanitizeCallsign2(const QString &callsign);        // up to 10 chars, right-padded
QString sanitizeLocation2(const QString &locator);         // upper case, padded to 4

// ---- Type 2 and 3 messages (replaces the external wspr_enc.exe used via WsprMsg) ----
// Same packing as WSJT-X (wsprsim_utils.c / wsprcode); symbols checked against wsprcode.
//   Type 2: compound callsign + power, no locator:  "PJ4/K1ABC 37", "G4GCI/P 23", "G4GCI/12 10"
//           prefix 1-3 letters/digits, or suffix of one letter/digit or two digits
//   Type 3: hashed callsign + 6-character locator + power: "<G4GCI/P> IO90GU 23"
// Power is rounded to the nearest valid level (0,3,7,10,...60) as WSJT-X does for types 2/3.
std::optional<Symbols> encodeType2(const QString &compoundCall, int powerdBm, QString *error = nullptr);
std::optional<Symbols> encodeType3(const QString &callsign, const QString &locator6, int powerdBm,
                                   QString *error = nullptr);
// Any WSJT-X style message text: "K1ABC FN42 37", "PJ4/K1ABC 37" or "<PJ4/K1ABC> FN42AX 37"
std::optional<Symbols> encodeMessage(const QString &message, QString *error = nullptr);

uint32_t callHash(const QString &callsign);                // 15-bit WSPR hash (lookup3, seed 146)

} // namespace WsprEncode
