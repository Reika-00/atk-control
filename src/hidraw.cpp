#include "hidraw.h"

#include "models.h"
#include "protocol.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStringList>

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <unistd.h>

namespace {

QHash<QString, QString> readUevent(const QString &path)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return values;
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals > 0) {
            values.insert(line.left(equals), line.mid(equals + 1).trimmed());
        }
    }
    return values;
}

QByteArray readDescriptor(const QString &sysPath)
{
    QFile file(sysPath + QStringLiteral("/device/report_descriptor"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QString readBcd(const QString &sysPath)
{
    const QStringList candidates = {
        sysPath + QStringLiteral("/device/bcdDevice"),
        sysPath + QStringLiteral("/device/../../bcdDevice"),
    };
    for (const QString &candidate : candidates) {
        QFile file(candidate);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString value = QString::fromUtf8(file.readAll()).trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
    return {};
}

QString detectProtocol(const QByteArray &descriptor)
{
    const QByteArray v1Page = QByteArray::fromHex("0602ff0902");
    const QByteArray v2Page = QByteArray::fromHex("0605ff");
    const QByteArray reportId = QByteArray::fromHex("8508");
    if (descriptor.contains(v1Page) && descriptor.contains(reportId)
        && descriptor.contains(QByteArray::fromHex("9510"))) {
        return QStringLiteral("v1");
    }
    if (descriptor.contains(v2Page) && descriptor.contains(reportId)
        && descriptor.contains(QByteArray::fromHex("953f"))) {
        return QStringLiteral("v2");
    }
    return {};
}

uint8_t u8(const QByteArray &data, int index)
{
    return static_cast<uint8_t>(data.at(index));
}

} // namespace

QVector<DeviceInfo> discoverDevices()
{
    QVector<DeviceInfo> devices;
    QDir sysDir(QStringLiteral("/sys/class/hidraw"));
    const QStringList entries = sysDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &entry : entries) {
        const QString sysPath = sysDir.filePath(entry);
        const auto uevent = readUevent(sysPath + QStringLiteral("/device/uevent"));
        const QString hidId = uevent.value(QStringLiteral("HID_ID"));
        const QStringList idParts = hidId.split(QLatin1Char(':'));
        if (idParts.size() != 3) {
            continue;
        }
        bool vendorOk = false;
        bool productOk = false;
        const uint16_t vendor = static_cast<uint16_t>(idParts.at(1).toUInt(&vendorOk, 16));
        const uint16_t product = static_cast<uint16_t>(idParts.at(2).toUInt(&productOk, 16));
        if (!vendorOk || !productOk) {
            continue;
        }
        if (vendor != 0x373B && vendor != 0x3554 && vendor != 0x25A7) {
            continue;
        }

        DeviceInfo info;
        info.path = QStringLiteral("/dev/") + entry;
        info.sysPath = sysPath;
        info.vendorId = vendor;
        info.productId = product;
        info.productName = uevent.value(QStringLiteral("HID_NAME")).trimmed();
        if (info.productName.isEmpty()) {
            info.productName = QStringLiteral("ATK/VXE HID interface");
        }
        info.bcdDevice = readBcd(sysPath);
        info.descriptor = readDescriptor(sysPath);
        info.protocol = detectProtocol(info.descriptor);
        info.receiver = isReceiver(product, info.productName);
        info.accessible = QFileInfo(info.path).isWritable();
        const QFileInfo deviceLink(sysPath + QStringLiteral("/device"));
        const QString target = deviceLink.symLinkTarget();
        info.interfaceName = target.isEmpty() ? entry : QFileInfo(target).fileName();
        devices.append(info);
    }

    std::sort(devices.begin(), devices.end(), [](const DeviceInfo &a, const DeviceInfo &b) {
        auto rank = [](const DeviceInfo &info) {
            if (info.protocol == QStringLiteral("v1")) {
                return 0;
            }
            if (info.protocol == QStringLiteral("v2")) {
                return 1;
            }
            return 2;
        };
        if (rank(a) != rank(b)) {
            return rank(a) < rank(b);
        }
        if (a.receiver != b.receiver) {
            return !a.receiver;
        }
        if (a.productName != b.productName) {
            return a.productName < b.productName;
        }
        return a.path < b.path;
    });
    return devices;
}

HidrawDevice::~HidrawDevice()
{
    close();
}

void HidrawDevice::open(const QString &path)
{
    close();
    const QByteArray localPath = path.toLocal8Bit();
    m_fd = ::open(localPath.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        const int error = errno;
        if (error == EACCES || error == EPERM) {
            throw AtkError(QStringLiteral("No permission for %1. Install the ATK udev rule and reconnect the mouse.")
                               .arg(path));
        }
        if (error == ENOENT) {
            throw AtkError(QStringLiteral("%1 disappeared. Reconnect the mouse.").arg(path));
        }
        throw AtkError(QStringLiteral("Could not open %1: %2")
                           .arg(path, QString::fromLocal8Bit(std::strerror(error))));
    }
    if (flock(m_fd, LOCK_EX | LOCK_NB) != 0 && errno == EWOULDBLOCK) {
        ::close(m_fd);
        m_fd = -1;
        throw AtkError(QStringLiteral("Another application is already using %1").arg(path));
    }
    m_path = path;
}

void HidrawDevice::close()
{
    if (m_fd >= 0) {
        flock(m_fd, LOCK_UN);
        ::close(m_fd);
        m_fd = -1;
    }
    m_path.clear();
}

void HidrawDevice::writeReport(const QByteArray &report)
{
    if (m_fd < 0) {
        throw AtkError(QStringLiteral("The HID device is not open"));
    }
    if (m_debug) {
        qDebug().noquote() << ">>" << report.toHex(' ');
    }
    const ssize_t written = ::write(m_fd, report.constData(), static_cast<size_t>(report.size()));
    if (written < 0) {
        const int error = errno;
        if (error == ENODEV || error == EIO) {
            throw AtkError(QStringLiteral("The mouse disconnected"));
        }
        throw AtkError(QStringLiteral("HID write failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(error))));
    }
    if (written != report.size() && written != report.size() - 1) {
        throw AtkError(QStringLiteral("Short HID write (%1 bytes)").arg(written));
    }
}

QByteArray HidrawDevice::readReport(int timeoutMs)
{
    if (m_fd < 0) {
        return {};
    }
    pollfd pfd {};
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    const int ready = ::poll(&pfd, 1, std::max(0, timeoutMs));
    if (ready < 0) {
        if (errno == EINTR) {
            return {};
        }
        throw AtkError(QStringLiteral("HID poll failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
    }
    if (ready == 0) {
        return {};
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        throw AtkError(QStringLiteral("The mouse disconnected"));
    }
    char buffer[256];
    const ssize_t count = ::read(m_fd, buffer, sizeof(buffer));
    if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return {};
        }
        if (errno == ENODEV || errno == EIO) {
            throw AtkError(QStringLiteral("The mouse disconnected"));
        }
        throw AtkError(QStringLiteral("HID read failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
    }
    if (count == 0) {
        return {};
    }
    const QByteArray data(buffer, static_cast<int>(count));
    if (m_debug) {
        qDebug().noquote() << "<<" << data.toHex(' ');
    }
    return data;
}

void HidrawDevice::drain(int settleMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < settleMs) {
        const QByteArray data = readReport(static_cast<int>(settleMs - timer.elapsed()));
        if (data.isEmpty()) {
            break;
        }
    }
}

QByteArray HidrawDevice::exchangeV1(const QByteArray &payload,
                                    const std::function<bool(const QByteArray &)> &matcher,
                                    int attempts)
{
    if (m_fd < 0) {
        throw AtkError(QStringLiteral("The HID device is not open"));
    }
    for (int attempt = 0; attempt < std::max(1, attempts); ++attempt) {
        drain(5);
        writeReport(QByteArray(1, static_cast<char>(atk::ReportId)) + payload);
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 650) {
            const QByteArray raw = readReport(static_cast<int>(650 - timer.elapsed()));
            if (raw.isEmpty()) {
                break;
            }
            const QByteArray reply = atk::decodeReport(raw);
            if (reply.isEmpty()) {
                continue;
            }
            if (matcher(reply)) {
                return reply;
            }
        }
    }
    throw AtkError(QStringLiteral("The mouse did not answer (asleep, out of range, or another app owns it)"));
}

QByteArray HidrawDevice::readEeprom(uint16_t address, int length)
{
    const QByteArray request = atk::buildRead(address, length);
    const uint8_t hi = static_cast<uint8_t>((address >> 8) & 0xFF);
    const uint8_t lo = static_cast<uint8_t>(address & 0xFF);
    const QByteArray reply = exchangeV1(request, [hi, lo, length](const QByteArray &data) {
        return data.size() >= 5 && u8(data, 0) == atk::CmdReadEeprom && u8(data, 1) == 0
            && u8(data, 2) == hi && u8(data, 3) == lo && u8(data, 4) >= length;
    });
    return reply.mid(5, length);
}

void HidrawDevice::writeEeprom(uint16_t address, const QByteArray &data)
{
    if (m_fd < 0) {
        throw AtkError(QStringLiteral("The HID device is not open"));
    }
    drain(5);
    writeReport(QByteArray(1, static_cast<char>(atk::ReportId)) + atk::buildWrite(address, data));
    drain(30);
}

QByteArray HidrawDevice::simpleCommand(uint8_t command, int attempts)
{
    return exchangeV1(atk::buildFrame(command), [command](const QByteArray &data) {
        return data.size() >= 5 && u8(data, 0) == command && u8(data, 1) == 0;
    }, attempts);
}

BatteryReading HidrawDevice::readBattery()
{
    const QByteArray reply = simpleCommand(atk::CmdBattery);
    const int length = std::min<int>(u8(reply, 4), 10);
    const QByteArray data = reply.mid(5, length);
    BatteryReading reading;
    if (data.size() >= 1) {
        reading.percent = u8(data, 0);
    }
    if (data.size() >= 2) {
        reading.charging = u8(data, 1) != 0;
    }
    if (data.size() >= 4) {
        const int millivolts = (u8(data, 2) << 8) | u8(data, 3);
        if (millivolts > 0) {
            reading.millivolts = millivolts;
        }
    }
    return reading;
}

QPair<int, int> HidrawDevice::readIdentity()
{
    const QByteArray reply = simpleCommand(atk::CmdCidMid);
    if (u8(reply, 4) < 2) {
        throw AtkError(QStringLiteral("Short CID/MID reply"));
    }
    return {u8(reply, 5), u8(reply, 6)};
}

QString HidrawDevice::readFirmware(int attempts)
{
    const QByteArray reply = simpleCommand(atk::CmdVersion, attempts);
    const int length = std::min<int>(u8(reply, 4), 10);
    const QByteArray data = reply.mid(5, length);
    if (data.size() < 2) {
        return {};
    }
    return QStringLiteral("%1.%2")
        .arg(static_cast<int>(u8(data, 0)), 0, 16)
        .arg(static_cast<int>(u8(data, 1)), 2, 16, QLatin1Char('0'));
}

std::optional<int> HidrawDevice::readProfile()
{
    const QByteArray reply = simpleCommand(atk::CmdCurrentProfile);
    if (u8(reply, 4) >= 1 && u8(reply, 5) < 4) {
        return u8(reply, 5);
    }
    return std::nullopt;
}

void HidrawDevice::setProfile(int profile)
{
    if (profile < 0 || profile > 3) {
        throw AtkError(QStringLiteral("Profile must be between 1 and 4"));
    }
    QByteArray frame = atk::buildFrame(atk::CmdSetProfile);
    frame[4] = 1;
    frame[5] = static_cast<char>(profile);
    frame[15] = 0;
    frame[15] = static_cast<char>(atk::checksum15(frame.left(15)));
    exchangeV1(frame, [](const QByteArray &data) {
        return u8(data, 0) == atk::CmdSetProfile && u8(data, 1) == 0;
    });
}

std::optional<ReceiverReading> HidrawDevice::receiverOnline()
{
    const QByteArray reply = simpleCommand(atk::CmdReceiverOnline);
    const QByteArray data = reply.mid(5, std::max(4, static_cast<int>(u8(reply, 4))));
    if (data.size() < 4) {
        return std::nullopt;
    }
    ReceiverReading reading;
    reading.status = u8(data, 0);
    reading.online = u8(data, 0) == 1;
    reading.rfId = QStringLiteral("%1 %2 %3")
                       .arg(static_cast<int>(u8(data, 3)), 2, 16, QLatin1Char('0'))
                       .arg(static_cast<int>(u8(data, 2)), 2, 16, QLatin1Char('0'))
                       .arg(static_cast<int>(u8(data, 1)), 2, 16, QLatin1Char('0'))
                       .toUpper();
    return reading;
}

QByteArray HidrawDevice::commandV2(uint8_t command, int paramLength, int commandLength,
                                   const QByteArray &data, int target)
{
    if (m_fd < 0) {
        throw AtkError(QStringLiteral("The HID device is not open"));
    }
    if (target < 0) {
        target = m_v2Wireless ? atk::V2TargetMouse : atk::V2TargetDongle;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        drain(10);
        writeReport(atk::buildV2Frame(command, paramLength, commandLength, data, target));
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 650) {
            const QByteArray raw = readReport(static_cast<int>(650 - timer.elapsed()));
            if (raw.isEmpty()) {
                break;
            }
            QByteArray payload;
            QString error;
            if (atk::decodeV2Reply(raw, command, &payload, &error)) {
                return payload;
            }
            if (!error.isEmpty()) {
                throw AtkError(error);
            }
        }
    }
    throw AtkError(QStringLiteral("The mouse did not answer v2 command 0x%1")
                       .arg(static_cast<int>(command), 2, 16, QLatin1Char('0')));
}

QByteArray HidrawDevice::readMemoryV2(uint16_t address, int length)
{
    if (length < 1 || length > 512) {
        throw AtkError(QStringLiteral("v2 read length must be between 1 and 512"));
    }
    QByteArray out;
    while (out.size() < length) {
        const uint16_t offset = static_cast<uint16_t>(address + out.size());
        const int count = std::min(atk::V2ReadChunk, length - static_cast<int>(out.size()));
        QByteArray requested(2, '\0');
        requested[0] = static_cast<char>(offset & 0xFF);
        requested[1] = static_cast<char>((offset >> 8) & 0xFF);
        QByteArray data = requested;
        data.append(static_cast<char>(count));
        const QByteArray payload = commandV2(atk::V2CmdReadMemory, 10, 13, data);
        if (payload.size() < 3 || payload.left(2) != requested) {
            throw AtkError(QStringLiteral("Stale v2 memory reply at 0x%1")
                               .arg(offset, 4, 16, QLatin1Char('0')));
        }
        if (u8(payload, 2) < count) {
            throw AtkError(QStringLiteral("Short v2 memory reply at 0x%1")
                               .arg(offset, 4, 16, QLatin1Char('0')));
        }
        const QByteArray chunk = payload.mid(3, count);
        if (chunk.size() != count) {
            throw AtkError(QStringLiteral("Short v2 memory reply at 0x%1")
                               .arg(offset, 4, 16, QLatin1Char('0')));
        }
        out.append(chunk);
    }
    return out;
}

QPair<int, int> HidrawDevice::readIdentityV2()
{
    const QByteArray payload = commandV2(atk::V2CmdReadIdentity, 8, 6);
    if (payload.size() < 3) {
        throw AtkError(QStringLiteral("Short v2 identity reply"));
    }
    return {u8(payload, 1), u8(payload, 2)};
}

QString HidrawDevice::readVersionV2()
{
    const QByteArray payload = commandV2(atk::V2CmdReadFirmware, 5, 3);
    if (payload.size() < 3) {
        throw AtkError(QStringLiteral("Short v2 firmware reply"));
    }
    return QStringLiteral("%1.%2.%3")
        .arg(static_cast<int>(u8(payload, 0)))
        .arg(static_cast<int>(u8(payload, 1)))
        .arg(static_cast<int>(u8(payload, 2)));
}

QByteArray HidrawDevice::readConfigV2()
{
    const QByteArray payload = commandV2(atk::V2CmdReadConfig, 20, 18);
    if (payload.size() < 18) {
        throw AtkError(QStringLiteral("Short v2 configuration reply"));
    }
    return payload;
}

QByteArray HidrawDevice::readDpiV2()
{
    return readMemoryV2(1, 66);
}

int HidrawDevice::readSensorModeV2()
{
    const QByteArray payload = readMemoryV2(74, 1);
    return payload.isEmpty() ? 0 : u8(payload, 0);
}

QByteArray HidrawDevice::readLightingV2()
{
    const QByteArray payload = readMemoryV2(76, 3);
    if (payload.size() < 3) {
        throw AtkError(QStringLiteral("Short v2 lighting reply"));
    }
    return payload;
}

QByteArray HidrawDevice::readButtonsV2()
{
    const QByteArray payload = readMemoryV2(82, 30);
    if (payload.size() < 30) {
        throw AtkError(QStringLiteral("Short v2 button reply"));
    }
    return payload;
}

BatteryReading HidrawDevice::readBatteryV2()
{
    const QByteArray payload = commandV2(atk::V2CmdBattery, 2, 1);
    if (payload.isEmpty()) {
        throw AtkError(QStringLiteral("Short v2 battery reply"));
    }
    const QByteArray charge = commandV2(atk::V2CmdReadCharging, 2, 1);
    if (charge.isEmpty()) {
        throw AtkError(QStringLiteral("Short v2 charging reply"));
    }
    BatteryReading reading;
    reading.percent = u8(payload, 0);
    reading.charging = u8(charge, 0) == 1 || u8(charge, 0) == 0x3B;
    if (payload.size() >= 3) {
        const int millivolts = u8(payload, 1) | (u8(payload, 2) << 8);
        if (millivolts >= 1000 && millivolts <= 6000) {
            reading.millivolts = millivolts;
        }
    }
    return reading;
}

QString HidrawDevice::readDongleVersionV2()
{
    const QByteArray payload = commandV2(atk::V2CmdReadDongleVersion, 5, 3, {}, atk::V2TargetDongle);
    if (payload.size() < 3) {
        throw AtkError(QStringLiteral("Short v2 dongle reply"));
    }
    return QStringLiteral("%1.%2.%3")
        .arg(static_cast<int>(u8(payload, 0)))
        .arg(static_cast<int>(u8(payload, 1)))
        .arg(static_cast<int>(u8(payload, 2)));
}

bool HidrawDevice::readOnlineV2()
{
    const QByteArray payload = commandV2(atk::V2CmdReadOnline, 3, 1, {}, atk::V2TargetDongle);
    return !payload.isEmpty() && u8(payload, 0) != 0;
}

void HidrawDevice::setRateV2(int code)
{
    commandV2(atk::V2CmdSetRate, 3, 1, QByteArray(1, static_cast<char>(code)));
}

void HidrawDevice::setDpiV2(int stage, int x, int y, const QString &color, bool active)
{
    int r = 255;
    int g = 255;
    int b = 255;
    QString text = color.trimmed();
    if (text.startsWith(QLatin1Char('#'))) {
        text.remove(0, 1);
    }
    if (text.size() == 6) {
        bool ok = false;
        const int red = text.mid(0, 2).toInt(&ok, 16);
        if (ok) {
            const int green = text.mid(2, 2).toInt(&ok, 16);
            if (ok) {
                const int blue = text.mid(4, 2).toInt(&ok, 16);
                if (ok) {
                    r = red;
                    g = green;
                    b = blue;
                }
            }
        }
    }
    QByteArray data(10, '\0');
    data[0] = static_cast<char>(stage);
    data[1] = static_cast<char>(x & 0xFF);
    data[2] = static_cast<char>((x >> 8) & 0xFF);
    data[3] = static_cast<char>(y & 0xFF);
    data[4] = static_cast<char>((y >> 8) & 0xFF);
    data[5] = static_cast<char>(b);
    data[6] = static_cast<char>(g);
    data[7] = static_cast<char>(r);
    data[8] = 0;
    data[9] = static_cast<char>(active ? 1 : 0);
    commandV2(atk::V2CmdSetDpi, 12, 10, data);
}

void HidrawDevice::setLodV2(int level)
{
    QByteArray data(2, '\0');
    data[0] = 0;
    data[1] = static_cast<char>(level - 1);
    commandV2(atk::V2CmdSetLod, 4, 2, data);
}

void HidrawDevice::setMotionSyncV2(bool enabled)
{
    commandV2(atk::V2CmdSetMotionSync, 3, 1, QByteArray(1, static_cast<char>(enabled ? 1 : 0)));
}

void HidrawDevice::setAngleSnapV2(bool enabled)
{
    commandV2(atk::V2CmdSetAngle, 3, 1, QByteArray(1, static_cast<char>(enabled ? 1 : 0)));
}

void HidrawDevice::setRippleV2(bool enabled)
{
    commandV2(atk::V2CmdSetRipple, 3, 1, QByteArray(1, static_cast<char>(enabled ? 1 : 0)));
}

void HidrawDevice::setDebounceV2(int milliseconds)
{
    commandV2(atk::V2CmdSetDebounce, 3, 1,
              QByteArray(1, static_cast<char>(milliseconds & 0xFF)));
}

void HidrawDevice::setSleepV2(int seconds)
{
    QByteArray data(2, '\0');
    data[0] = static_cast<char>(seconds & 0xFF);
    data[1] = static_cast<char>((seconds >> 8) & 0xFF);
    commandV2(atk::V2CmdSetSleep, 5, 2, data);
}

void HidrawDevice::setSensorModeV2(int code)
{
    commandV2(atk::V2CmdSetSensorMode, 3, 1, QByteArray(1, static_cast<char>(code)));
}

void HidrawDevice::setLightingV2(int mode, int brightness, int speed)
{
    QByteArray data(3, '\0');
    data[0] = static_cast<char>(mode);
    data[1] = static_cast<char>(brightness);
    data[2] = static_cast<char>(speed);
    commandV2(atk::V2CmdSetLight, 6, 3, data);
}

void HidrawDevice::setProfileV2(int profile)
{
    commandV2(atk::V2CmdSetProfile, 3, 1, QByteArray(1, static_cast<char>(profile - 1)));
}

void HidrawDevice::setButtonV2(int index, const QVector<int> &action)
{
    if (action.size() < 4) {
        throw AtkError(QStringLiteral("Invalid v2 button action"));
    }
    QByteArray data(5, '\0');
    data[0] = static_cast<char>(index);
    for (int i = 0; i < 4; ++i) {
        data[1 + i] = static_cast<char>(action.at(i) & 0xFF);
    }
    commandV2(atk::V2CmdSetButton, 8, 5, data);
}

void HidrawDevice::resetV2()
{
    commandV2(atk::V2CmdReset, 2, 1);
}
