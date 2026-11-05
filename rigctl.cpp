#include "rigctl.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTcpSocket>

RigCtl::RigCtl(const QString &host, int port)
    : m_host(host), m_port(port)
{
}

RigCtl::~RigCtl() = default;

void RigCtl::setAddress(const QString &host, int port)
{
    if (host == m_host && port == m_port)
        return;
    m_host = host;
    m_port = port;
    m_sock.reset();
}

bool RigCtl::ensureConnected()
{
    if (m_sock && m_sock->state() == QAbstractSocket::ConnectedState)
        return true;
    m_sock = std::make_unique<QTcpSocket>();
    m_sock->connectToHost(m_host, quint16(m_port));
    if (!m_sock->waitForConnected(m_timeoutMs)) {
        m_error = QString("Cannot connect to rigctld at %1:%2 (%3) - is it running?")
                      .arg(m_host).arg(m_port).arg(m_sock->errorString());
        m_sock.reset();
        return false;
    }
    return true;
}

std::optional<QString> RigCtl::send(const QString &command)
{
    // one retry: an idle connection may have been closed by rigctld (or rigctld restarted)
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!ensureConnected())
            return std::nullopt;
        m_sock->readAll();   // discard anything left over
        const QByteArray out = command.toLatin1() + '\n';
        if (m_sock->write(out) != out.size() || !m_sock->waitForBytesWritten(m_timeoutMs)) {
            m_sock.reset();
            continue;
        }
        QByteArray reply;
        QElapsedTimer t;
        t.start();
        while (!reply.contains('\n')) {
            const int left = m_timeoutMs - int(t.elapsed());
            if (left <= 0 || !m_sock->waitForReadyRead(left))
                break;
            reply += m_sock->readAll();
        }
        if (reply.contains('\n'))
            return QString::fromLatin1(reply).trimmed();
        if (m_sock->state() != QAbstractSocket::ConnectedState) {   // dropped: reconnect and retry
            m_sock.reset();
            continue;
        }
        m_error = QString("No reply from rigctld to \"%1\" within %2 ms").arg(command).arg(m_timeoutMs);
        m_sock.reset();   // don't leave a late reply to be read as the answer to the next command
        return std::nullopt;
    }
    m_error = QString("Lost connection to rigctld at %1:%2").arg(m_host).arg(m_port);
    return std::nullopt;
}

std::optional<int> RigCtl::rprtCode(const QString &reply)
{
    static const QRegularExpression re(QStringLiteral("^RPRT\\s+(-?\\d+)$"));
    const auto m = re.match(reply.trimmed());
    if (!m.hasMatch())
        return std::nullopt;
    return m.captured(1).toInt();
}

bool RigCtl::expectOk(const QString &command)
{
    const auto r = send(command);
    if (!r)
        return false;
    const auto code = rprtCode(*r);
    if (code && *code == 0)
        return true;
    m_error = QString("rigctld refused \"%1\": %2").arg(command, *r);
    return false;
}

bool RigCtl::setFrequency(qint64 hz)
{
    if (hz <= 0) {
        m_error = "Invalid frequency";
        return false;
    }
    return expectOk(QString("F %1").arg(hz));
}

std::optional<qint64> RigCtl::frequency()
{
    const auto r = send("f");
    if (!r)
        return std::nullopt;
    bool ok = false;
    const qint64 hz = qint64(r->toDouble(&ok));   // some rigs report "14095600.000000"
    if (!ok) {
        m_error = "Unexpected reply to \"f\": " + *r;
        return std::nullopt;
    }
    return hz;
}

bool RigCtl::setPtt(bool on)
{
    return expectOk(on ? "T 1" : "T 0");
}

std::optional<bool> RigCtl::ptt()
{
    const auto r = send("t");
    if (!r)
        return std::nullopt;
    if (rprtCode(*r)) {
        m_error = "rigctld refused \"t\": " + *r;
        return std::nullopt;
    }
    return r->toInt() != 0;
}

bool RigCtl::isReachable()
{
    return frequency().has_value();
}
