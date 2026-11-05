#include "wsprencode.h"

#include <QStringList>
#include <cmath>

namespace WsprEncode {

namespace {
bool fail(QString *error, const char *msg)
{
    if (error)
        *error = QString::fromLatin1(msg);
    return false;
}
} // namespace

std::optional<uint8_t> wsprCode(QChar c)
{
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9')
        return uint8_t(u - '0');
    if (u == ' ')
        return uint8_t(36);
    if (u >= 'A' && u <= 'Z')
        return uint8_t(u - 55);
    return std::nullopt;   // C#: "character X is not allowed"
}

std::optional<QString> sanitizeCallsign(const QString &callsign, QString *error)
{
    QString c = callsign.trimmed().toUpper();
    if (c.length() > 6)                      // long callsigns trimmed to 6 characters
        c = c.left(6);
    if (c.isEmpty()) {
        fail(error, "callsign must contain at least one number");
        return std::nullopt;
    }
    // The C# checks for "multiple numbers" / "must start with a letter" threw inside a try
    // with an empty catch, so calls such as 2E0ABC still encoded (correctly). What actually
    // decides whether a call can be packed is the layout below, so that is what is checked.
    bool hasDigit = false;
    for (const QChar ch : c)
        hasDigit = hasDigit || ch.isDigit();
    if (!hasDigit) { fail(error, "callsign must contain at least one number"); return std::nullopt; }
    if (c.length() > 1 && c[1].isDigit())    // 2nd character a digit: pad with a leading space
        c.prepend(' ');
    if (c.length() < 3 || !c[2].isDigit()) {
        fail(error, "the callsign's second or third character must be a number");
        return std::nullopt;
    }
    if (c.length() > 6) {                    // e.g. "G4ABCD" -> " G4ABCD" no longer fits
        fail(error, "callsign too long for a type 1 message");
        return std::nullopt;
    }
    while (c.length() < 6)
        c += ' ';
    // packing layout: [A-Z0-9 ][A-Z0-9][0-9][A-Z ][A-Z ][A-Z ]
    auto letterOrSpace = [](QChar ch) { return ch == ' ' || (ch >= 'A' && ch <= 'Z'); };
    auto alnum = [](QChar ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'); };
    if (!(alnum(c[0]) || c[0] == ' ') || !alnum(c[1])) {
        fail(error, "callsign contains a character that is not allowed");
        return std::nullopt;
    }
    if (!letterOrSpace(c[3]) || !letterOrSpace(c[4]) || !letterOrSpace(c[5])) {
        fail(error, "callsign must end with letters (use a type 2/3 message for /P etc.)");
        return std::nullopt;
    }
    return c;
}

std::optional<QString> sanitizeLocation(const QString &locator, QString *error)
{
    const QString l = locator.toUpper();
    if (l.length() != 4) { fail(error, "location must contain exactly four characters"); return std::nullopt; }
    for (int i = 0; i < 2; ++i)
        if (l[i] < 'A' || l[i] > 'R') { fail(error, "location must start with two letters A-R"); return std::nullopt; }
    for (int i = 2; i < 4; ++i)
        if (l[i] < '0' || l[i] > '9') { fail(error, "location must end with two digits 0-9"); return std::nullopt; }
    return l;
}

const std::vector<uint8_t> &validPowerLevels()
{
    static const std::vector<uint8_t> levels = {0, 3, 7, 10, 13, 17, 20, 23, 27, 30,
                                                33, 37, 40, 43, 47, 50, 53, 57, 60};
    return levels;
}

uint8_t sanitizePower(double powerdBm)
{
    uint8_t level = 0;
    if (!std::isfinite(powerdBm))
        return level;   // C# threw "power must be finite"; callers always pass a combo value
    for (uint8_t v : validPowerLevels())
        if (powerdBm >= v)
            level = v;
    return level;
}

QString powerDescription(uint8_t dBm)
{
    const double mW = std::pow(10.0, dBm / 10.0);
    QString s = mW < 1000 ? QString("%1 dB (%2 mW)").arg(dBm).arg(QString::number(std::llround(mW)))
                          : QString("%1 dB (%2 W)").arg(dBm).arg(QString::number(std::llround(mW / 1000)));
    s.replace("01 ", "00 ");   // as C# ("1001 mW" style rounding artefacts)
    return s;
}

std::optional<std::array<uint8_t, 7>> messageBytes(const QString &callsign, const QString &locator,
                                                   double powerdBm, QString *error)
{
    const auto call = sanitizeCallsign(callsign, error);
    if (!call)
        return std::nullopt;
    const auto loc = sanitizeLocation(locator, error);
    if (!loc)
        return std::nullopt;
    const uint8_t pwr = sanitizePower(powerdBm);

    uint8_t code[6];
    for (int i = 0; i < 6; ++i) {
        const auto v = wsprCode((*call)[i]);
        if (!v) {
            fail(error, "callsign contains a character that is not allowed");
            return std::nullopt;
        }
        code[i] = *v;
    }
    // characters 4-6 must be letters or space (codes 10..36)
    for (int i = 3; i < 6; ++i)
        if (code[i] < 10) {
            fail(error, "callsign suffix must be letters");
            return std::nullopt;
        }

    // pack callsign into 28 bits
    uint32_t a = code[0];
    a = a * 36 + code[1];
    a = a * 10 + code[2];
    a = a * 27 + (code[3] - 10);
    a = a * 27 + (code[4] - 10);
    a = a * 27 + (code[5] - 10);

    // pack locator and power into 22 bits
    const QString &l = *loc;
    uint32_t b = uint32_t(180 * (179 - 10 * (l[0].unicode() - 'A') - (l[2].unicode() - '0'))
                          + 10 * (l[1].unicode() - 'A') + (l[3].unicode() - '0'));
    b = b * 128 + pwr + 64;

    std::array<uint8_t, 7> bytes{};
    bytes[3] = uint8_t((a & 0xF) << 4);
    a >>= 4;
    bytes[2] = uint8_t(a & 0xFF);
    a >>= 8;
    bytes[1] = uint8_t(a & 0xFF);
    a >>= 8;
    bytes[0] = uint8_t(a & 0xFF);
    bytes[6] = uint8_t((b & 0x3) << 6);
    b >>= 2;
    bytes[5] = uint8_t(b & 0xFF);
    b >>= 8;
    bytes[4] = uint8_t(b & 0xFF);
    b >>= 8;
    bytes[3] = uint8_t(bytes[3] | (b & 0xF));
    return bytes;
}

static uint8_t parity32(uint32_t v)
{
    uint8_t p = 0;
    for (int k = 0; k < 32; ++k) {
        p ^= uint8_t(v & 1u);
        v >>= 1;
    }
    return p;
}

Symbols convolve(const std::array<uint8_t, 7> &data)
{
    uint8_t padded[kMessageSize] = {};
    for (size_t i = 0; i < data.size(); ++i)
        padded[i] = data[i];

    Symbols out{};
    uint32_t reg0 = 0, reg1 = 0;
    int bitIndex = 0;
    for (int i = 0; i < kMessageSize && bitIndex < kBitCount; ++i) {
        for (int j = 0; j < 8 && bitIndex < kBitCount; ++j) {
            const uint32_t inputBit = ((padded[i] << j) & 0x80) ? 1u : 0u;
            reg0 = (reg0 << 1) | inputBit;
            reg1 = (reg1 << 1) | inputBit;
            out[bitIndex++] = parity32(reg0 & 0xf2d05351u);
            out[bitIndex++] = parity32(reg1 & 0xe4613c47u);
        }
    }
    return out;
}

Symbols interleave(const Symbols &data)
{
    Symbols d{};
    int i = 0;
    for (int j = 0; j < 255 && i < kBitCount; ++j) {
        uint8_t j2 = uint8_t(j), rev = 0;
        for (int k = 0; k < 8; ++k) {   // bit-reverse j
            if (j2 & 1)
                rev = uint8_t(rev | (1 << (7 - k)));
            j2 >>= 1;
        }
        if (rev < kBitCount)
            d[rev] = data[i++];
    }
    return d;
}

Symbols integrateSync(const Symbols &data)
{
    static const uint8_t sync[kBitCount] = {
        1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0,
        1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1,
        0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0,
        1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1,
        1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0};
    Symbols out{};
    for (int i = 0; i < kBitCount; ++i)
        out[i] = uint8_t(data[i] * 2 + sync[i]);
    return out;
}

std::optional<Symbols> encodeType1(const QString &callsign, const QString &locator, double powerdBm, QString *error)
{
    const auto bytes = messageBytes(callsign, locator, powerdBm, error);
    if (!bytes)
        return std::nullopt;
    return integrateSync(interleave(convolve(*bytes)));
}

QString sanitizeCallsign2(const QString &callsign)
{
    QString c = callsign.trimmed().toUpper();
    while (c.length() < 10)
        c += ' ';
    return c;
}

QString sanitizeLocation2(const QString &locator)
{
    // C# called location.PadRight(4) without using the result, so short locators stayed short
    return locator.toUpper().leftJustified(4, ' ');
}

// ---------------------------------------------------------------------------
// Type 2 / type 3 (WSJT-X wsprsim_utils.c: pack_call, pack_prefix, nhash, get_wspr_channel_symbols)
// ---------------------------------------------------------------------------

namespace {

int callChar(QChar c)   // get_callsign_character_code
{
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9') return u - '0';
    if (u >= 'A' && u <= 'Z') return u - 'A' + 10;
    return 36;          // space (and anything else)
}

// pack_call: 28-bit standard callsign (also used for the rotated 6-char locator of type 3)
std::optional<uint32_t> packCall(const QString &call)
{
    if (call.size() > 6 || call.size() < 2)
        return std::nullopt;
    QString c6(6, ' ');
    if (call.size() > 2 && call[2].isDigit()) {
        for (int i = 0; i < call.size(); ++i) c6[i] = call[i];
    } else if (call[1].isDigit()) {
        if (call.size() > 5) return std::nullopt;
        for (int i = 0; i < call.size(); ++i) c6[i + 1] = call[i];
    } else {
        return std::nullopt;
    }
    int v[6];
    for (int i = 0; i < 6; ++i) v[i] = callChar(c6[i]);
    for (int i = 3; i < 6; ++i)
        if (v[i] < 10) return std::nullopt;   // last three must be letters or space
    if (v[2] > 9) return std::nullopt;
    uint32_t n = uint32_t(v[0]);
    n = n * 36 + uint32_t(v[1]);
    n = n * 10 + uint32_t(v[2]);
    n = n * 27 + uint32_t(v[3] - 10);
    n = n * 27 + uint32_t(v[4] - 10);
    n = n * 27 + uint32_t(v[5] - 10);
    return n;
}

int roundPower(int p)   // types 2/3: clamp, then nearest level ending 0, 3 or 7
{
    static const int nu[10] = {0, -1, 1, 0, -1, 2, 1, 0, -1, 1};
    p = qBound(0, p, 60);
    return p + nu[p % 10];
}

std::array<uint8_t, 7> pack50(uint32_t n1, uint32_t n2)
{
    std::array<uint8_t, 7> d{};
    d[0] = uint8_t((n1 >> 20) & 0xff);
    d[1] = uint8_t((n1 >> 12) & 0xff);
    d[2] = uint8_t((n1 >> 4) & 0xff);
    d[3] = uint8_t(((n1 & 0x0f) << 4) | ((n2 >> 18) & 0x0f));
    d[4] = uint8_t((n2 >> 10) & 0xff);
    d[5] = uint8_t((n2 >> 2) & 0xff);
    d[6] = uint8_t((n2 & 0x03) << 6);
    return d;
}

Symbols channelSymbols(uint32_t n, uint32_t m)
{
    return integrateSync(interleave(convolve(pack50(n, m))));
}

inline uint32_t rot(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }

} // namespace

// Bob Jenkins lookup3 hashlittle(), byte-wise, seed 146, low 15 bits (WSJT-X nhash.c)
uint32_t callHash(const QString &callsign)
{
    const QByteArray key = callsign.toLatin1();
    const auto *k = reinterpret_cast<const uint8_t *>(key.constData());
    size_t length = size_t(key.size());
    uint32_t a, b, c;
    a = b = c = 0xdeadbeefu + uint32_t(length) + 146u;
    auto mix = [&] {
        a -= c; a ^= rot(c, 4);  c += b;
        b -= a; b ^= rot(a, 6);  a += c;
        c -= b; c ^= rot(b, 8);  b += a;
        a -= c; a ^= rot(c, 16); c += b;
        b -= a; b ^= rot(a, 19); a += c;
        c -= b; c ^= rot(b, 4);  b += a;
    };
    while (length > 12) {
        a += k[0] + (uint32_t(k[1]) << 8) + (uint32_t(k[2]) << 16) + (uint32_t(k[3]) << 24);
        b += k[4] + (uint32_t(k[5]) << 8) + (uint32_t(k[6]) << 16) + (uint32_t(k[7]) << 24);
        c += k[8] + (uint32_t(k[9]) << 8) + (uint32_t(k[10]) << 16) + (uint32_t(k[11]) << 24);
        mix();
        length -= 12;
        k += 12;
    }
    switch (length) {   // fall-through intended
    case 12: c += uint32_t(k[11]) << 24; [[fallthrough]];
    case 11: c += uint32_t(k[10]) << 16; [[fallthrough]];
    case 10: c += uint32_t(k[9]) << 8;   [[fallthrough]];
    case 9:  c += k[8];                  [[fallthrough]];
    case 8:  b += uint32_t(k[7]) << 24;  [[fallthrough]];
    case 7:  b += uint32_t(k[6]) << 16;  [[fallthrough]];
    case 6:  b += uint32_t(k[5]) << 8;   [[fallthrough]];
    case 5:  b += k[4];                  [[fallthrough]];
    case 4:  a += uint32_t(k[3]) << 24;  [[fallthrough]];
    case 3:  a += uint32_t(k[2]) << 16;  [[fallthrough]];
    case 2:  a += uint32_t(k[1]) << 8;   [[fallthrough]];
    case 1:  a += k[0]; break;
    case 0:  return c & 32767u;
    }
    c ^= b; c -= rot(b, 14);
    a ^= c; a -= rot(c, 11);
    b ^= a; b -= rot(a, 25);
    c ^= b; c -= rot(b, 16);
    a ^= c; a -= rot(c, 4);
    b ^= a; b -= rot(a, 14);
    c ^= b; c -= rot(b, 24);
    return c & 32767u;
}

std::optional<Symbols> encodeType2(const QString &compoundCall, int powerdBm, QString *error)
{
    const QString call = compoundCall.trimmed().toUpper();
    const int slash = call.indexOf('/');
    if (slash < 0 || call.indexOf('/', slash + 1) >= 0) {
        fail(error, "type 2 needs a callsign with one prefix or suffix, e.g. G4GCI/P or PJ4/K1ABC");
        return std::nullopt;
    }
    const QString left = call.left(slash), right = call.mid(slash + 1);
    uint32_t nPrefix = 0;   // value the decoder sees (WSJT-X unpackpfx)
    QString base;
    if (right.size() == 1 || (right.size() == 2 && right[0].isDigit() && right[1].isDigit())) {
        base = left;                                  // suffix
        if (right.size() == 1) {
            const int v = callChar(right[0]);
            if (v > 35) { fail(error, "suffix must be a letter or digit"); return std::nullopt; }
            nPrefix = 60000u + uint32_t(v);
        } else {
            nPrefix = 60000u + 26u + uint32_t(10 * right[0].digitValue() + right[1].digitValue());
        }
    } else if (left.size() >= 1 && left.size() <= 3) {
        base = right;                                 // prefix, right-aligned in 3 base-37 digits
        uint32_t m = 0;
        for (int i = 0; i < 3 - left.size(); ++i) m = 37 * m + 36;
        for (const QChar ch : left) {
            const int v = callChar(ch);
            if (v > 35) { fail(error, "prefix must be letters or digits"); return std::nullopt; }
            m = 37 * m + uint32_t(v);
        }
        nPrefix = m;
    } else {
        fail(error, "suffix must be 1 character or 2 digits; prefix 1-3 characters");
        return std::nullopt;
    }
    const auto n = packCall(base);
    if (!n) { fail(error, "base callsign cannot be packed"); return std::nullopt; }
    const uint32_t nadd = nPrefix >= 32768u ? 1u : 0u;
    const uint32_t ng = nPrefix - 32768u * nadd;
    const uint32_t ntype = uint32_t(roundPower(powerdBm)) + 1u + nadd;
    return channelSymbols(*n, 128u * ng + ntype + 64u);
}

std::optional<Symbols> encodeType3(const QString &callsign, const QString &locator6, int powerdBm, QString *error)
{
    const QString call = callsign.trimmed().toUpper();
    const QString loc = locator6.trimmed().toUpper();
    if (loc.size() != 6 || loc[0] < 'A' || loc[0] > 'R' || loc[1] < 'A' || loc[1] > 'R' || !loc[2].isDigit()
        || !loc[3].isDigit() || loc[4] < 'A' || loc[4] > 'X' || loc[5] < 'A' || loc[5] > 'X') {
        fail(error, "type 3 needs a 6-character locator (e.g. IO90GU)");
        return std::nullopt;
    }
    if (call.isEmpty()) { fail(error, "no callsign"); return std::nullopt; }
    const QString rotated = loc.mid(1) + loc[0];   // "IO90GU" -> "O90GUI", packed as a callsign
    const auto n = packCall(rotated);
    if (!n) { fail(error, "locator cannot be packed"); return std::nullopt; }
    const int ntype = -(roundPower(powerdBm) + 1);
    const uint32_t m = uint32_t(int(128 * callHash(call)) + ntype + 64);
    return channelSymbols(*n, m);
}

std::optional<Symbols> encodeMessage(const QString &message, QString *error)
{
    const QStringList w = message.simplified().toUpper().split(' ');
    if (w.size() == 3 && w[0].startsWith('<') && w[0].endsWith('>'))
        return encodeType3(w[0].mid(1, w[0].size() - 2), w[1], w[2].toInt(), error);
    if (w.size() == 2 && w[0].contains('/'))
        return encodeType2(w[0], w[1].toInt(), error);
    if (w.size() == 3)
        return encodeType1(w[0], w[1], w[2].toDouble(), error);
    fail(error, "message must be 'CALL GRID4 dBm', 'PFX/CALL dBm' or '<CALL> GRID6 dBm'");
    return std::nullopt;
}

} // namespace WsprEncode
