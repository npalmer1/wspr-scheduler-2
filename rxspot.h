#pragma once
// Receive-side logic from RXForm.cs that does not need a window, a sound card or a database:
// parsing wsprd output, band codes, the comparison columns, timing of the 2-minute cycle,
// the wsprd command line and the wsprnet.org upload form.
//
// Differences from the C# version are listed next to each function and in PORTING_NOTES.md.
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// One row of wspr_rpt.received (C#: RXForm.decoded_data)
struct RxSpot
{
    QDateTime datetime;        // UTC, start of the 2-minute cycle, seconds = 0
    int band = -2;             // C# band code, see Rx::bandCode
    QString txSign;            // "G4GCI", "G4GCI/P", "<G4GCI/P>", or "nil rcvd"
    QString txLoc;             // "" for type 2 messages
    double frequency = 0.0;    // MHz, 6 decimals
    int power = 0;             // dBm
    int snr = 0;
    int drift = 0;
    int distance = -1;         // km, -1 = unknown
    int azimuth = -1;          // degrees, -1 = unknown
    QString reporter;
    QString reporterLoc;
    double dt = 0.0;

    bool isNil() const;
};

namespace Rx {

extern const QString kNilRcvd;   // "nil rcvd"

// ---- wsprd output -------------------------------------------------------------------------
// One wsprd stdout line: "HHMM snr dt freq drift CALL [LOC] dBm".
// Fills snr, dt, frequency, band, drift, txSign, txLoc, power, distance/azimuth from myLoc.
// nullopt for lines that are not decodes. (C# turned those into a row called "error" and then
// skipped it; the end result is the same.)
std::optional<RxSpot> parseWsprdLine(const QString &line, const QString &myLoc);

struct DecodeResult
{
    bool finished = false;     // "<DecodeFinished>" seen; false = wsprd failed or was killed
    QVector<RxSpot> spots;     // decodes in wsprd order, duplicates removed; or one "nil rcvd" row
    int skippedLines = 0;      // lines that were not decodes
};
// Whole wsprd output for one cycle. Every spot gets datetime = cycleStart, reporter, reporterLoc.
// No decodes -> one "nil rcvd" row at the dial frequency (as C#), so the grid shows the RX was listening.
// No "<DecodeFinished>" -> finished = false and no rows (C# showed a bare "Error" box in that case).
DecodeResult parseWsprdOutput(const QString &output, const QDateTime &cycleStart, double dialMHz,
                              const QString &reporter, const QString &myLoc);

// ---- bands --------------------------------------------------------------------------------
// Band code stored in received.band (C#: convertMHz). Same codes as C#:
// LF -1, MF 0, 160m 1, 80m 3, 60m 5, 40m 7, 30m 10, 22m 13, 20m 14, 17m 18, 15m 21, 12m 24,
// 10m 28, 8m 40, 6m 50, 4m 70, 2m 15, 70cm 16, 23cm 17, anything else -2.
// Fixed: C# tested "14" before "144", so every 2m spot was stored as 20m (14).
int bandCode(double mhz);

// The "Band:" filter list, same order as bandlistBox ("All", "LF", "MF", "160m", ... "23cm").
struct BandRange { QString name; double lo; double hi; };   // lo <= f < hi; "All" has lo = hi = 0
const QVector<BandRange> &bandRanges();
// Band as a transmitter names it: "2200", "630", "160" ... "20" ... "2", "70cm", "23cm"; "" if none
QString bandMetres(double mhz);

// ---- comparison columns -------------------------------------------------------------------
QString wattsText(int dBm);              // C#: dBtoWatts; the 19 WSPR levels, "" for anything else
int wattsToDbm(int watts);               // C#: convertTodBm, (int)(10*log10(W*1000))
QString cwLabel(int snr, int dBm, int compareDbm);    // C#: getCW
QString ssbLabel(int snr, int dBm, int compareDbm);   // C#: getSSB ("bordeline" spelling fixed)
int kmToMiles(int km);                   // C#: convert_to_miles (Convert.ToInt32 = round half to even)
int milesToKm(int miles);                // C# filter: (int)(miles * 1.609)
QString snrText(int snr);                // "+3", "0", "-21"

// ---- cycle timing -------------------------------------------------------------------------
// Start of the 2-minute cycle a recording begun at utcNow belongs to: the even minute, also when
// started at odd:59 (C# worked it out from the clock after recording, which gives the same answer
// unless the recording overran).
QDateTime cycleStart(const QDateTime &utcNow);
// How long to record: until cycleStart + 110 s, as C# (110 s from :00, 111 s from :59, 109 s from :01),
// but counted to the millisecond rather than from whole seconds. Never less than 60 s.
int recordDurationMs(const QDateTime &utcNow);

// ---- wsprd --------------------------------------------------------------------------------
// wsprd(.exe): in dir if given and present, else in a "wsprd" folder next to the program, else next to
// the program, else on PATH. Empty if none.
QString wsprdExecutable(const QString &dir);
// Arguments for: wsprd -a dataDir -f dial [-d|-q] [-o n] wav.
// Passed straight to QProcess, so paths with spaces work (C# built one string for cmd.exe /c).
QStringList wsprdArguments(const QString &dataDir, const QString &dialMHz, bool deep, bool quick,
                           int osd, const QString &wavPath);

// ---- wsprnet.org --------------------------------------------------------------------------
// Form fields for http://wsprnet.org/post/ (C#: Post_wsprdata), in the same order
QList<QPair<QString, QString>> wsprnetForm(const RxSpot &s, const QString &version, const QString &rcall,
                                           const QString &rgrid, const QString &rqrg);
// Should this spot be uploaded? Not "nil rcvd", not empty, and not our own call unless allowed.
// Own call now also matches "<G4GCI/P>" (type 3 decode) and the base call "G4GCI" (type 1);
// C# only compared the exact text, so own type 3 decodes were uploaded with "allow own call" off.
bool shouldUpload(const RxSpot &s, const QString &ownCall, bool allowOwnCall);
bool isOwnCall(const QString &txSign, const QString &ownCall);

// ---- debug log ----------------------------------------------------------------------------
// C# wrote C:\Users\Public\wspr_debug.txt. Same file on Windows, ~/wspr_debug.txt elsewhere.
QString defaultDebugLogPath();
void setDebugLogPath(const QString &path);
void debugLog(const QString &text);       // "yyyy-MM-dd HH:mm:ssZ text"
void clearDebugLog();

// Folder for the recordings and wsprd's own files (ALL_WSPR.TXT, hashtable.txt):
// C:/WSPR_Sked on Windows (as C#), ~/WSPR_Sked elsewhere
QString defaultDataDir();

} // namespace Rx
