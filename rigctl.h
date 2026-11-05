#pragma once
// Client for Hamlib's rigctld (TCP, default 127.0.0.1:4532) - port of RigCtlD.cs plus the
// sendRigCommand / sendTXRigCommand helpers in Form1.
//
// Differences from the C# version:
//  - rigctld's "RPRT n" answer is checked. C# sendTXRigCommand returned "ok" for any reply except a
//    dropped connection, so "RPRT -1" (rig refused the frequency or PTT) was treated as success;
//  - every wait has a timeout (C# Stream.Read could block the UI forever if rigctld stopped answering);
//  - "connection refused" is reported, not thrown (C# created the TcpClient outside its try block);
//  - the reply is read up to the end of the line (C# did one Read(), which can return half a reply);
//  - one connection is kept open and re-opened if it drops (C# connected for every command).
// Calls are synchronous; with rigctld on the same PC they take a few milliseconds.
#include <QString>
#include <memory>
#include <optional>

class QTcpSocket;

class RigCtl
{
public:
    explicit RigCtl(const QString &host = "127.0.0.1", int port = 4532);
    ~RigCtl();

    void setAddress(const QString &host, int port);
    void setTimeout(int ms) { m_timeoutMs = ms; }

    // Raw command, e.g. "f" or "F 14095600". Returns the reply without the trailing newline.
    std::optional<QString> send(const QString &command);

    bool setFrequency(qint64 hz);            // "F <Hz>"   (C#: "F " + TXFrequency)
    std::optional<qint64> frequency();       // "f"
    bool setPtt(bool on);                    // "T 1" / "T 0"
    std::optional<bool> ptt();               // "t"
    bool isReachable();                      // connects (or reuses the connection) and asks "f"

    QString lastError() const { return m_error; }

    // "RPRT 0" -> 0, "RPRT -11" -> -11, anything else -> nullopt
    static std::optional<int> rprtCode(const QString &reply);

private:
    bool ensureConnected();
    bool expectOk(const QString &command);

    QString m_host;
    int m_port;
    int m_timeoutMs = 2000;
    std::unique_ptr<QTcpSocket> m_sock;
    QString m_error;
};
