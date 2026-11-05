#pragma once
// TCP client for the WiFi Arduino antenna switch / filter board - port of Arduino.cs (ArduinoComms).
// Protocol (port 5000 by default), one command per connection, reply is one line:
//   "set-pin <n> high|low\n"  ->  "Pin: <n> set: high|low"
//   "filter:<freq>\n"         ->  board specific ("error" if not understood)
//
// Differences from the C# version:
//  - failures are reported. C# set ok = (response != "error" || response != ""), which is always true,
//    so a switch that did not answer ("Switching error") was treated as switched and the TX went ahead
//    on whatever antenna was selected before;
//  - a set-pin reply must confirm the pin and level that were asked for;
//  - connect and reply have timeouts (C# could block on Stream.Read);
//  - the reply is read to the end of the line.
// Note: the sketch in Arduino.cs reads only one digit of the pin number, so pins 10-13 select pins 1
// (see tools/arduino_switch_fixed.ino for a corrected sketch).
#include <QString>

namespace ArduinoSwitch {

struct Result
{
    bool ok = false;
    QString reply;    // what the board answered (trimmed), or empty
    QString error;    // why it failed
};

// C#: ArduinoComms(ip, port, pin, high, "switch", "")  - channel n uses pin n-1 (Form1.Arduino_Comms_1)
Result setPin(const QString &ip, int port, int pin, bool high, int timeoutMs = 3000);
// C#: ArduinoComms(ip, port, 0, high, "filter", freq)
Result setFilter(const QString &ip, int port, const QString &freq, int timeoutMs = 3000);

// One command on a fresh connection, reply up to the first newline (or the board closing the connection)
Result exchange(const QString &ip, int port, const QString &command, int timeoutMs = 3000);

} // namespace ArduinoSwitch
