// Reference values produced by running the original C# Encode.cs and Encryption.cs (.NET 8).
// Regenerate them the same way if either C# file changes.
#include "credentials.h"
#include "wsprencode.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct SymCase { const char *call, *loc; double dBm; const char *symbols; };   // symbols empty = C# threw
static const SymCase kSym[] = {
    {"G4GCI", "IO90", 23, "330002201220113222102101133020020230030300022230132033010221301222013032303010232010332223123010201002023223021312130031232201132220030320310000220112121120033022"},
    {"K1ABC", "FN42", 37, "330020001020131222100323133220200032012322002232110233210221321222033030301210212032132003323032203020201023021112330231212221332000010320132222202332323320031222"},
    {"G4GCI", "IO90", 0, "330002201220113022122303133222000232030300002230112233010223321022033232323012212012312221123210201200023023001110110031232203112222030322330200220112121120033022"},
    {"W1AW", "FN31", 60, "332022001022333222302101333200020012030120200230332013012023103020013212101010210010112201301010221002001023201332110213212203312220230300312202200330301300033020"},
    {"2E0ABC", "IO91", 10, "112022023222133022102101111202220230210322020210332013032023321002033210101232210212332023301232021222021003203310312233212001110220232102130222220312101302031200"},
    {"M0XYZ", "JO01", 33, "310220021002111022120103133000020210010120222010110031032203123220013030303210032230132023323032203202021221023312310031012021132220010122112220220112123120233222"},
    {"g4gci", "io90", 25, "330002201220113222102101133020020230030300022230132033010221301222013032303010232010332223123010201002023223021312130031232201132220030320310000220112121120033022"},
    {"VK2RG", "QF56", 17, "332000001020113222120123313220220212212320200030330011232023101220211030103012232232310023301012001022003003003312110011230223332222232120130022020330323120013202"},
    {"G4GCI/P", "IO90", 23, ""},
    {"AB", "IO90", 23, ""},
    {"G44CI", "IO90", 23, ""},
    {"G4GCI", "IO90GU", 23, ""},
};
struct EncCase { const char *plain, *cipher; };
static const EncCase kEnc[] = {
    {u8"wspr", "ZR79X3h5OJgkQsqODCRfsA=="},
    {u8"", "KwMFs8JjvQOcBBUlo3Qw1w=="},
    {u8"Passw0rd!", "gPmsxbW/mtj5I7Ix4TbG+Q=="},
    {u8"a much longer password over sixteen bytes", "ekt3R7kyd3lmJPH8Qy6tkJsagUAWtbiK5/KdT/Fnft8rUjEbMZxJHGfIxd3LGxP5"},
    {u8"£ü€", "Dej9pKo3tZ+IvgbiLxTpyg=="},
};
struct PwrCase { int dBm; const char *text; };
static const PwrCase kPwr[] = {
    {0, u8"0 dB (1 mW)"},
    {1, u8"1 dB (1 mW)"},
    {2, u8"2 dB (2 mW)"},
    {3, u8"3 dB (2 mW)"},
    {4, u8"4 dB (3 mW)"},
    {5, u8"5 dB (3 mW)"},
    {6, u8"6 dB (4 mW)"},
    {7, u8"7 dB (5 mW)"},
    {8, u8"8 dB (6 mW)"},
    {9, u8"9 dB (8 mW)"},
    {10, u8"10 dB (10 mW)"},
    {11, u8"11 dB (13 mW)"},
    {12, u8"12 dB (16 mW)"},
    {13, u8"13 dB (20 mW)"},
    {14, u8"14 dB (25 mW)"},
    {15, u8"15 dB (32 mW)"},
    {16, u8"16 dB (40 mW)"},
    {17, u8"17 dB (50 mW)"},
    {18, u8"18 dB (63 mW)"},
    {19, u8"19 dB (79 mW)"},
    {20, u8"20 dB (100 mW)"},
    {21, u8"21 dB (126 mW)"},
    {22, u8"22 dB (158 mW)"},
    {23, u8"23 dB (200 mW)"},
    {24, u8"24 dB (251 mW)"},
    {25, u8"25 dB (316 mW)"},
    {26, u8"26 dB (398 mW)"},
    {27, u8"27 dB (500 mW)"},
    {28, u8"28 dB (631 mW)"},
    {29, u8"29 dB (794 mW)"},
    {30, u8"30 dB (1 W)"},
    {31, u8"31 dB (1 W)"},
    {32, u8"32 dB (2 W)"},
    {33, u8"33 dB (2 W)"},
    {34, u8"34 dB (3 W)"},
    {35, u8"35 dB (3 W)"},
    {36, u8"36 dB (4 W)"},
    {37, u8"37 dB (5 W)"},
    {38, u8"38 dB (6 W)"},
    {39, u8"39 dB (8 W)"},
    {40, u8"40 dB (10 W)"},
    {41, u8"41 dB (13 W)"},
    {42, u8"42 dB (16 W)"},
    {43, u8"43 dB (20 W)"},
    {44, u8"44 dB (25 W)"},
    {45, u8"45 dB (32 W)"},
    {46, u8"46 dB (40 W)"},
    {47, u8"47 dB (50 W)"},
    {48, u8"48 dB (63 W)"},
    {49, u8"49 dB (79 W)"},
    {50, u8"50 dB (100 W)"},
    {51, u8"51 dB (126 W)"},
    {52, u8"52 dB (158 W)"},
    {53, u8"53 dB (200 W)"},
    {54, u8"54 dB (251 W)"},
    {55, u8"55 dB (316 W)"},
    {56, u8"56 dB (398 W)"},
    {57, u8"57 dB (500 W)"},
    {58, u8"58 dB (631 W)"},
    {59, u8"59 dB (794 W)"},
    {60, u8"60 dB (1000 W)"},
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // --- WSPR type 1 encoder: identical symbols to the C# version ---
    for (const SymCase &c : kSym) {
        QString err;
        const auto s = WsprEncode::encodeType1(c.call, c.loc, c.dBm, &err);
        if (*c.symbols == 0) {
            CHECK(!s && !err.isEmpty());   // C# threw here; the port reports an error instead
            std::printf("     %-8s %-6s -> rejected: %s\n", c.call, c.loc, qPrintable(err));
            continue;
        }
        QString got;
        if (s)
            for (uint8_t v : *s) got += QChar('0' + v);
        if (got != QString::fromLatin1(c.symbols))
            std::printf("     mismatch %s %s %g\n       got %s\n", c.call, c.loc, c.dBm, qPrintable(got));
        CHECK(got == QString::fromLatin1(c.symbols));
    }
    CHECK(WsprEncode::sanitizePower(25) == 23 && WsprEncode::sanitizePower(-3) == 0 && WsprEncode::sanitizePower(99) == 60);
    CHECK(WsprEncode::sanitizeLocation2("io9") == "IO9 ");
    CHECK(WsprEncode::sanitizeCallsign2("g4gci/p") == "G4GCI/P   ");

    // --- Power descriptions ---
    for (const PwrCase &p : kPwr) {
        const QString got = WsprEncode::powerDescription(uint8_t(p.dBm));
        if (got != QString::fromUtf8(p.text))
            std::printf("     power %d: got '%s' want '%s'\n", p.dBm, qPrintable(got), p.text);
        CHECK(got == QString::fromUtf8(p.text));
    }

    // --- AES-128 against FIPS-197 appendix C.1 ---
    const QByteArray key = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
    const QByteArray pt = QByteArray::fromHex("00112233445566778899aabbccddeeff");
    const QByteArray ct = Security::aes128CbcEncrypt(pt, key, QByteArray(16, '\0'));
    CHECK(ct.left(16).toHex() == "69c4e0d86a7b0430d8cdb78070b4c55a");
    CHECK(Security::aes128CbcDecrypt(ct, key, QByteArray(16, '\0')) == pt);

    // --- Encryption.cs: same ciphertext as .NET, and round trip ---
    for (const EncCase &e : kEnc) {
        const QString plain = QString::fromUtf8(e.plain);
        CHECK(Security::encrypt(plain) == QString::fromLatin1(e.cipher));
        CHECK(Security::decrypt(QString::fromLatin1(e.cipher)) == plain);
    }
    CHECK(!Security::decrypt("not base64 !!"));
    CHECK(!Security::decrypt("ZR79X3h5OJgkQsqODCRfsA==", "wrongkeywrongkey"));   // bad padding -> rejected

    // --- ~/DBcredential in the C# format ---
    QTemporaryDir home;
    CHECK(!Security::readDbPassword(home.path()));
    CHECK(Security::writeDbCredential("admin", "s3cret wspr", home.path()));
    CHECK(Security::readDbPassword(home.path()) == QString("s3cret wspr"));
    {
        QFile f(QDir(home.path()).filePath("DBcredential"));
        f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        f.write("db_user: admin db_pass: ZR79X3h5OJgkQsqODCRfsA==\nG4GCI\n");   // as written by the C# app
    }
    CHECK(Security::readDbPassword(home.path()) == QString("wspr"));

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
