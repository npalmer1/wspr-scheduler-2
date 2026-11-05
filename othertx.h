#pragma once
// "Non Rigctl TX" rigs - port of OtherTX.cs plus Form1.sendOtherTXRigCommand.
// A rig command string can hold several commands separated by ';'.
//   HTTP:   each command is sent as GET <base url><command>, spaces replaced by '+'
//           (base url = http://<ip>:<port>/<api path>, as Form1 built it from APIstartlabel + APIURLtextBox)
//   Serial: the whole string is written as one line (Yaesu-style "FA014095600;TX1;" stays intact)
//
// Differences from the C# version:
//  - HTTP commands are sent one after another and each is waited for. C# started them without awaiting
//    (async, not awaited), so they could arrive in any order, reply stayed null and Form1 always said "ok";
//  - an HTTP error status or timeout fails the command;
//  - serial settings are read as data,parity,stop,flow - the order the rig settings save them. C# read
//    them as parity,data,stop,flow, hit an exception and silently used 8N1 with no flow control;
//  - serial rigs no longer go through the HTTP "reachable" check first (C# built http://ip:COM3/... for
//    them, which always failed, so serial commands were never sent);
//  - for serial, a reply is optional (most CAT set commands don't answer); C# waited for an LF-terminated
//    line and called it an error otherwise.
// Calls are synchronous (a local event loop for HTTP), up to timeoutMs per command.
#include <QString>
#include <QStringList>

namespace OtherTx {

struct RigLink
{
    QString protocol;      // contains "Serial" for COM ports, anything else = HTTP (C# test was Contains("Serial"))
    QString ip;
    QString port;          // TCP port for HTTP, "COM3" etc. for serial
    QString baud;          // serial
    QString serial;        // serial "8,none,1,none"
    QString apiPath;       // HTTP path after the host, e.g. "api?cmd=" (C#: APIURLtextBox)
    bool https = false;
};

struct Result
{
    bool ok = false;
    QString reply;         // replies joined with '\n' (HTTP bodies, or what the serial device sent)
    QStringList replies;   // one per command sent (HTTP), or the one serial reply
    QString error;
};

QString baseUrl(const RigLink &rig);
Result send(const RigLink &rig, const QString &commands, int timeoutMs = 3000);

} // namespace OtherTx
