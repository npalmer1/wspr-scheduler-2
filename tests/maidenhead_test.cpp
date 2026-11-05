// Maidenhead port vs values produced by running the original Maidenhead.cs (.NET 8) - no database needed.
#include "maidenhead.h"

#include <QCoreApplication>
#include <cmath>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct LL { const char *loc; double lat, lng; };          // C# gives 91/181 for invalid locators
static const LL kLL[] = {
    {"IO90", 50.5, -1},
    {"IO90GU", 50.85416666666666, -1.4583333333333428},
    {"io90gu", 50.85416666666666, -1.4583333333333428},
    {"IO90GU42", 50.84375, -1.4625000000000057},
    {"IO90GU42KX", 50.84574652777778, -1.4630208333333314},
    {"FN42", 42.5, -71},
    {"AA00", -89.5, -179},
    {"RR99", 89.5, 179},
    {"RR99XX", 89.97916666666666, 179.95833333333331},
    {"JJ00AA", 0.020833333333328596, 0.04166666666665719},
    {"QF56OD", -33.854166666666664, 151.20833333333331},
    {"KC90", -69.5, 39},
    {"AR09", 89.5, -179},
    {"DP0POL", 91, 181},
    {"IO9", 91, 181},
    {"IO90G", 91, 181},
    {"SS00", 91, 181},
    {"IO90YY", 91, 181},
    {"", 91, 181},
    {" IO90 ", 50.5, -1},
};
struct DA { const char *a, *b; double km, az; };
static const DA kDA[] = {
    {"IO90GU", "FN42AX", 5215.1740084861085, 289.26073338782675},
    {"IO90GU", "QF56OD", 17098.439136430417, 59.96521559721319},
    {"IO90", "IO91", 111.1251134744659, 0},
    {"IO90GU", "IO90GU", 0, 0},
    {"IO90GU", "IO90GV", 4.630213061403779, 0},
    {"AA00", "RR99", 19891.412237254302, 358.99996192934753},
    {"JJ00", "JJ00AA", 119.06365680654758, 243.43749113318412},
    {"IO90GU", "KC90", 13771.967123293658, 164.1040303608754},
    {"IO90GU", "DP0POL", 4239.064579206895, 0.06943856525391251},
};
struct LOC { double lat, lng; int ext; const char *loc; };
static const LOC kLOC[] = {
    {50.854, -1.481, 0, "IO90GU"},
    {50.854, -1.481, 1, "IO90GU24"},
    {50.854, -1.481, 2, "IO90GU24GX"},
    {0, 0, 0, "JJ00AA"},
    {0, 0, 1, "JJ00AA00"},
    {0, 0, 2, "JJ00AA00AA"},
    {-33.86, 151.21, 0, "QF56OD"},
    {-33.86, 151.21, 1, "QF56OD53"},
    {-33.86, 151.21, 2, "QF56OD53EO"},
    {41.71, -72.73, 0, "FN31PR"},
    {41.71, -72.73, 1, "FN31PR20"},
    {41.71, -72.73, 2, "FN31PR20JJ"},
    {89.99, 179.99, 0, "RR99XX"},
    {89.99, 179.99, 1, "RR99XX87"},
    {89.99, 179.99, 2, "RR99XX87TO"},
    {-89.99, -179.99, 0, "AA00AA"},
    {-89.99, -179.99, 1, "AA00AA12"},
    {-89.99, -179.99, 2, "AA00AA12EJ"},
    {51.5, -0.0001, 0, "IO91XM"},
    {51.5, -0.0001, 1, "IO91XM90"},
    {51.5, -0.0001, 2, "IO91XM90XA"},
};

static bool near(double a, double b) { return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b)); }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace Maidenhead;
    for (const LL &r : kLL) {
        const auto p = toLatLng(r.loc);
        if (r.lat == 91 && r.lng == 181)
            CHECK(!p);                                   // invalid -> nullopt instead of the (91,181) point
        else
            CHECK(p && near(p->lat, r.lat) && near(p->lng, r.lng));
    }
    for (const DA &r : kDA) {
        const auto d = distanceKm(r.a, r.b), a = azimuthDeg(r.a, r.b);
        if (QString(r.b) == "DP0POL")
            CHECK(!d && !a);                             // C# measured to the (91,181) point: 4239 km, due north
        else {
            if (!d || !near(*d, r.km) || !near(*a, r.az))
                std::printf("     %s-%s got %.12g km %.12g deg\n", r.a, r.b, d.value_or(-1), a.value_or(-1));
            CHECK(d && near(*d, r.km) && a && near(*a, r.az));
        }
    }
    for (const LOC &r : kLOC)
        CHECK(toLocator(r.lat, r.lng, r.ext) == QString(r.loc));

    // fixes over the C# version
    CHECK(toLocator(90, 180) == "RR99XX");                        // C#: 'S' characters
    const LatLng a{50.854, -1.481};
    const double tiny = distanceKm(a, LatLng{50.854 + 1e-9, -1.481});
    CHECK(!std::isnan(tiny) && tiny < 0.001);                     // C# could give NaN here
    CHECK((LatLng{50.854, -1.481}.toString() == "50.854N 1.481W"));
    CHECK(isValid("io90gu") && !isValid("IO90G") && !isValid("SS00"));

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
