#pragma once
// Port of Maidenhead.cs (MaidenheadLocator / LatLng)
// Copyright (c) 2011, Yves Goergen, http://unclassified.software/source/maidenheadlocator
// Copying and distribution of this file, with or without modification, are permitted provided the
// copyright notice and this notice are preserved. This file is offered as-is, without any warranty.
// Based on a Perl module by Dirk Koopman, G1TLH, 2002-11-07.
//
// Same results as the C# version for valid input. Differences:
//  - an invalid locator gives std::nullopt. C# returned the point (91, 181) instead, and Distance/Azimuth
//    then measured to it as if it were real (due north, past the pole) - anything plotted from it lands
//    off the top of a map;
//  - distance/azimuth clamp a rounding error that made C# return NaN for some very close, non-identical points;
//  - LatLngToLocator clamps lat 90 / long 180 (C# produced 'S' characters there);
//  - toString() puts N/S on latitude and E/W on longitude (C# had them swapped).
#include <QString>
#include <optional>

namespace Maidenhead {

struct LatLng
{
    double lat = 0.0;    // -90..90, north positive
    double lng = 0.0;    // -180..180, east positive
    bool operator==(const LatLng &o) const { return lat == o.lat && lng == o.lng; }
    QString toString() const;   // "50.854N 1.458W"
};

// 4, 6, 8 or 10 characters (case and surrounding spaces ignored); centre of the square
std::optional<LatLng> toLatLng(const QString &locator);
bool isValid(const QString &locator);

// ext: 0 = 6 characters, 1 = 8, 2 = 10 (C#: LatLngToLocator(lat, long, Ext))
QString toLocator(double lat, double lng, int ext = 0);
inline QString toLocator(const LatLng &p, int ext = 0) { return toLocator(p.lat, p.lng, ext); }

// Great-circle distance in km (earth radius 6367 km, as C#) and initial bearing in degrees 0..360
double distanceKm(const LatLng &a, const LatLng &b);
double azimuthDeg(const LatLng &a, const LatLng &b);
std::optional<double> distanceKm(const QString &a, const QString &b);
std::optional<double> azimuthDeg(const QString &a, const QString &b);

double radToDeg(double rad);
double degToRad(double deg);

} // namespace Maidenhead
