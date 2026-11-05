#include "maidenhead.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace Maidenhead {

namespace {
constexpr double kPi = 3.141592653589793;

// C#: Math.IEEERemainder(x, y)
double ieeeRemainder(double x, double y)
{
    return std::remainder(x, y);
}
} // namespace

double radToDeg(double rad) { return rad / kPi * 180; }
double degToRad(double deg) { return deg / 180 * kPi; }

QString LatLng::toString() const
{
    return QString("%1%2 %3%4")
        .arg(std::abs(lat), 0, 'f', 3).arg(lat >= 0 ? 'N' : 'S')
        .arg(std::abs(lng), 0, 'f', 3).arg(lng >= 0 ? 'E' : 'W');
}

std::optional<LatLng> toLatLng(const QString &locator)
{
    static const QRegularExpression re4("^[A-R]{2}[0-9]{2}$"), re6("^[A-R]{2}[0-9]{2}[A-X]{2}$"),
        re8("^[A-R]{2}[0-9]{2}[A-X]{2}[0-9]{2}$"), re10("^[A-R]{2}[0-9]{2}[A-X]{2}[0-9]{2}[A-X]{2}$");
    const QString l = locator.trimmed().toUpper();
    auto c = [&](int i) { return double(l[i].unicode()); };
    LatLng p;
    if (re4.match(l).hasMatch()) {
        p.lng = (c(0) - 'A') * 20 + (c(2) - '0' + 0.5) * 2 - 180;
        p.lat = (c(1) - 'A') * 10 + (c(3) - '0' + 0.5) - 90;
    } else if (re6.match(l).hasMatch()) {
        p.lng = (c(0) - 'A') * 20 + (c(2) - '0') * 2 + (c(4) - 'A' + 0.5) / 12 - 180;
        p.lat = (c(1) - 'A') * 10 + (c(3) - '0') + (c(5) - 'A' + 0.5) / 24 - 90;
    } else if (re8.match(l).hasMatch()) {
        p.lng = (c(0) - 'A') * 20 + (c(2) - '0') * 2 + (c(4) - 'A' + 0.0) / 12 + (c(6) - '0' + 0.5) / 120 - 180;
        p.lat = (c(1) - 'A') * 10 + (c(3) - '0') + (c(5) - 'A' + 0.0) / 24 + (c(7) - '0' + 0.5) / 240 - 90;
    } else if (re10.match(l).hasMatch()) {
        p.lng = (c(0) - 'A') * 20 + (c(2) - '0') * 2 + (c(4) - 'A' + 0.0) / 12 + (c(6) - '0' + 0.0) / 120
                + (c(8) - 'A' + 0.5) / 120 / 24 - 180;
        p.lat = (c(1) - 'A') * 10 + (c(3) - '0') + (c(5) - 'A' + 0.0) / 24 + (c(7) - '0' + 0.0) / 240
                + (c(9) - 'A' + 0.5) / 240 / 24 - 90;
    } else {
        return std::nullopt;   // C#: (91, 181)
    }
    return p;
}

bool isValid(const QString &locator)
{
    return toLatLng(locator).has_value();
}

QString toLocator(double lat, double lng, int ext)
{
    // keep inside the grid: lat 90 / long 180 would give an 'S' field
    lat = std::clamp(lat, -90.0, 90.0 - 1e-9);
    lng = std::clamp(lng, -180.0, 180.0 - 1e-9);

    QString loc;
    lat += 90;
    lng += 180;
    loc += QChar('A' + int(std::floor(lng / 20)));
    loc += QChar('A' + int(std::floor(lat / 10)));
    lng = ieeeRemainder(lng, 20);
    if (lng < 0) lng += 20;
    lat = ieeeRemainder(lat, 10);
    if (lat < 0) lat += 10;

    loc += QChar('0' + int(std::floor(lng / 2)));
    loc += QChar('0' + int(std::floor(lat / 1)));
    lng = ieeeRemainder(lng, 2);
    if (lng < 0) lng += 2;
    lat = ieeeRemainder(lat, 1);
    if (lat < 0) lat += 1;

    loc += QChar('A' + int(std::floor(lng * 12)));
    loc += QChar('A' + int(std::floor(lat * 24)));
    lng = ieeeRemainder(lng, 1.0 / 12);
    if (lng < 0) lng += 1.0 / 12;
    lat = ieeeRemainder(lat, 1.0 / 24);
    if (lat < 0) lat += 1.0 / 24;

    if (ext >= 1) {
        loc += QChar('0' + int(std::floor(lng * 120)));
        loc += QChar('0' + int(std::floor(lat * 240)));
        lng = ieeeRemainder(lng, 1.0 / 120);
        if (lng < 0) lng += 1.0 / 120;
        lat = ieeeRemainder(lat, 1.0 / 240);
        if (lat < 0) lat += 1.0 / 240;
    }
    if (ext >= 2) {
        loc += QChar('A' + int(std::floor(lng * 120 * 24)));
        loc += QChar('A' + int(std::floor(lat * 240 * 24)));
    }
    return loc;
}

namespace {
// central angle (radians), same formula as C# with the cosine clamped to [-1, 1]
double centralAngle(double hn, double he, double n, double e)
{
    double co = std::cos(he - e) * std::cos(hn) * std::cos(n) + std::sin(hn) * std::sin(n);
    co = std::clamp(co, -1.0, 1.0);
    double ca = std::atan(std::abs(std::sqrt(1 - co * co) / co));
    if (co < 0)
        ca = kPi - ca;
    return ca;
}
} // namespace

double distanceKm(const LatLng &a, const LatLng &b)
{
    if (a == b)
        return 0;
    return 6367 * centralAngle(degToRad(a.lat), degToRad(a.lng), degToRad(b.lat), degToRad(b.lng));
}

double azimuthDeg(const LatLng &a, const LatLng &b)
{
    if (a == b)
        return 0;
    const double hn = degToRad(a.lat), he = degToRad(a.lng), n = degToRad(b.lat), e = degToRad(b.lng);
    const double ca = centralAngle(hn, he, n, e);
    const double si = std::sin(e - he) * std::cos(n) * std::cos(hn);
    const double co = std::sin(n) - std::sin(hn) * std::cos(ca);
    double az = std::atan(std::abs(si / co));
    if (co < 0) az = kPi - az;
    if (si < 0) az = -az;
    if (az < 0) az = az + 2 * kPi;
    return radToDeg(az);
}

std::optional<double> distanceKm(const QString &a, const QString &b)
{
    const auto pa = toLatLng(a), pb = toLatLng(b);
    if (!pa || !pb)
        return std::nullopt;
    return distanceKm(*pa, *pb);
}

std::optional<double> azimuthDeg(const QString &a, const QString &b)
{
    const auto pa = toLatLng(a), pb = toLatLng(b);
    if (!pa || !pb)
        return std::nullopt;
    return azimuthDeg(*pa, *pb);
}

} // namespace Maidenhead
