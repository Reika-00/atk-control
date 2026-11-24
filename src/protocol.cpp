#include "protocol.h"

#include "types.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace atk {

namespace {
uint8_t byteAt(const QByteArray &data, int index)
{
    return static_cast<uint8_t>(data.at(index));
}
}

uint8_t checksum15(const QByteArray &payload15)
{
    if (payload15.size() != PayloadSize - 1) {
        return 0;
    }
    int total = ReportId;
    for (int i = 0; i < payload15.size(); ++i) {
        total += byteAt(payload15, i);
    }
    return static_cast<uint8_t>((ChecksumTotal - (total & 0xFF)) & 0xFF);
}

QByteArray buildFrame(uint8_t command, const QByteArray &body)
{
    if (body.size() > 14) {
        throw AtkError(QStringLiteral("HID frame body is too long"));
    }
    QByteArray payload(PayloadSize, '\0');
    payload[0] = static_cast<char>(command);
    for (int i = 0; i < body.size(); ++i) {
        payload[1 + i] = body.at(i);
    }
    payload[15] = static_cast<char>(checksum15(payload.left(15)));
    return payload;
}

QByteArray buildRead(uint16_t address, int length)
{
    if (length < 1 || length > MaxEepromChunk) {
        throw AtkError(QStringLiteral("Read length must be between 1 and 10 bytes"));
    }
    QByteArray body(4, '\0');
    body[0] = 0;
    body[1] = static_cast<char>((address >> 8) & 0xFF);
    body[2] = static_cast<char>(address & 0xFF);
    body[3] = static_cast<char>(length);
    return buildFrame(CmdReadEeprom, body);
}

QByteArray buildWrite(uint16_t address, const QByteArray &data)
{
    if (data.size() < 1 || data.size() > MaxEepromChunk) {
        throw AtkError(QStringLiteral("Write length must be between 1 and 10 bytes"));
    }
    QByteArray body(4, '\0');
    body[0] = 0;
    body[1] = static_cast<char>((address >> 8) & 0xFF);
    body[2] = static_cast<char>(address & 0xFF);
    body[3] = static_cast<char>(data.size());
    body.append(data);
    return buildFrame(CmdWriteEeprom, body);
}

bool reportIsValid(const QByteArray &payload)
{
    if (payload.size() < PayloadSize) {
        return false;
    }
    return checksum15(payload.left(15)) == byteAt(payload, 15);
}

QByteArray decodeReport(const QByteArray &raw)
{
    if (raw.size() >= WireSize) {
        const QByteArray withId = raw.mid(1, PayloadSize);
        if (byteAt(raw, 0) == ReportId && reportIsValid(withId)) {
            return withId;
        }
    }
    if (raw.size() >= PayloadSize) {
        const QByteArray withoutId = raw.left(PayloadSize);
        if (reportIsValid(withoutId)) {
            return withoutId;
        }
    }
    return {};
}

std::optional<int> decodePair(uint8_t value, uint8_t check)
{
    if (static_cast<uint8_t>((value + check) & 0xFF) != ChecksumTotal) {
        return std::nullopt;
    }
    return value;
}

QByteArray packPair(int value)
{
    const uint8_t v = static_cast<uint8_t>(value & 0xFF);
    QByteArray out(2, '\0');
    out[0] = static_cast<char>(v);
    out[1] = static_cast<char>((ChecksumTotal - v) & 0xFF);
    return out;
}

std::optional<int> decodeDpiAxis(uint8_t byte, int nibble, const QString &family)
{
    if (family == QStringLiteral("ultra")) {
        const int code = (((nibble >> 2) & 0x03) << 8) | byte;
        int base = 0;
        if (nibble & 2) {
            base = 10050 + code * 50;
        } else {
            base = (code + 1) * 10;
        }
        return (nibble & 1) ? base * 2 : base;
    }
    if (family == QStringLiteral("step50")) {
        const int count = byte | (((nibble >> 2) & 0x03) << 8);
        const int dpi = (count + 1) * 50;
        return (nibble & 1) ? dpi * 2 : dpi;
    }
    return std::nullopt;
}

QPair<uint8_t, uint8_t> encodeDpiAxis(int dpi, const QString &family)
{
    if (family == QStringLiteral("ultra")) {
        int value = dpi;
        int doubled = 0;
        if (value > 30000) {
            doubled = 1;
            value = static_cast<int>(std::lround(value / 2.0));
        }
        if (value <= 10000) {
            const int code = value / 10 - 1;
            return {static_cast<uint8_t>(code & 0xFF),
                    static_cast<uint8_t>((((code >> 8) << 2) | doubled) & 0x0F)};
        }
        const int code = (value - 10050) / 50;
        return {static_cast<uint8_t>(code & 0xFF),
                static_cast<uint8_t>((((code >> 8) << 2) | 2 | doubled) & 0x0F)};
    }
    const int doubled = dpi > 30000 ? 1 : 0;
    const int count = static_cast<int>(std::lround(dpi / (doubled ? 100.0 : 50.0))) - 1;
    return {static_cast<uint8_t>(count & 0xFF),
            static_cast<uint8_t>(((((count >> 8) & 0x03) << 2) | (doubled ? 1 : 0)) & 0x0F)};
}

QByteArray packDpiStage(int x, int y, const QString &family)
{
    const auto xb = encodeDpiAxis(x, family);
    const auto yb = encodeDpiAxis(y, family);
    const uint8_t mode = static_cast<uint8_t>(((yb.second & 0x0F) << 4) | (xb.second & 0x0F));
    const uint8_t total = static_cast<uint8_t>((xb.first + yb.first + mode) & 0xFF);
    QByteArray out(4, '\0');
    out[0] = static_cast<char>(xb.first);
    out[1] = static_cast<char>(yb.first);
    out[2] = static_cast<char>(mode);
    out[3] = static_cast<char>((ChecksumTotal - total) & 0xFF);
    return out;
}

std::optional<QPair<int, int>> unpackDpiStage(const QByteArray &data, const QString &family)
{
    if (data.size() < 4) {
        return std::nullopt;
    }
    int total = 0;
    for (int i = 0; i < 4; ++i) {
        total += byteAt(data, i);
    }
    if ((total & 0xFF) != ChecksumTotal) {
        return std::nullopt;
    }
    const auto x = decodeDpiAxis(byteAt(data, 0), byteAt(data, 2) & 0x0F, family);
    const auto y = decodeDpiAxis(byteAt(data, 1), (byteAt(data, 2) >> 4) & 0x0F, family);
    if (!x || !y) {
        return std::nullopt;
    }
    return QPair<int, int>(*x, *y);
}

QVector<int> dpiValues(int minDpi, int maxDpi, const QString &family)
{
    QVector<int> values;
    if (family == QStringLiteral("ultra")) {
        for (int v = minDpi; v <= std::min(maxDpi, 10000); v += 10) {
            values.append(v);
        }
        for (int v = 10050; v <= std::min(maxDpi, 30000); v += 50) {
            values.append(v);
        }
        for (int v = 30100; v <= maxDpi; v += 100) {
            values.append(v);
        }
    } else {
        for (int v = minDpi; v <= std::min(maxDpi, 10000); v += 50) {
            values.append(v);
        }
        for (int v = 10050; v <= std::min(maxDpi, 30000); v += 50) {
            values.append(v);
        }
        for (int v = 30100; v <= maxDpi; v += 100) {
            values.append(v);
        }
    }
    values.erase(std::remove_if(values.begin(), values.end(),
                                [minDpi](int value) { return value < minDpi; }),
                 values.end());
    return values;
}

QByteArray packRgb(const QString &color)
{
    QString text = color.trimmed();
    if (text.startsWith(QLatin1Char('#'))) {
        text.remove(0, 1);
    }
    if (text.size() != 6) {
        return {};
    }
    bool ok = false;
    const int r = text.mid(0, 2).toInt(&ok, 16);
    if (!ok) {
        return {};
    }
    const int g = text.mid(2, 2).toInt(&ok, 16);
    if (!ok) {
        return {};
    }
    const int b = text.mid(4, 2).toInt(&ok, 16);
    if (!ok) {
        return {};
    }
    const int total = (r + g + b) & 0xFF;
    QByteArray out(4, '\0');
    out[0] = static_cast<char>(r);
    out[1] = static_cast<char>(g);
    out[2] = static_cast<char>(b);
    out[3] = static_cast<char>((ChecksumTotal - total) & 0xFF);
    return out;
}

QString unpackRgb(const QByteArray &record)
{
    if (record.size() < 4) {
        return {};
    }
    int total = 0;
    for (int i = 0; i < 4; ++i) {
        total += byteAt(record, i);
    }
    if ((total & 0xFF) != ChecksumTotal) {
        return {};
    }
    return QStringLiteral("#%1%2%3")
        .arg(byteAt(record, 0), 2, 16, QLatin1Char('0'))
        .arg(byteAt(record, 1), 2, 16, QLatin1Char('0'))
        .arg(byteAt(record, 2), 2, 16, QLatin1Char('0'));
}

QByteArray buildV2Frame(uint8_t command, int paramLength, int commandLength,
                        const QByteArray &data, int target)
{
    if (paramLength < 0 || paramLength > 0xFF || commandLength < 0 || commandLength > 0xFF) {
        throw AtkError(QStringLiteral("Invalid v2 frame length"));
    }
    if (target != V2TargetDongle && target != V2TargetMouse) {
        throw AtkError(QStringLiteral("Invalid v2 target"));
    }
    if (data.size() > 56) {
        throw AtkError(QStringLiteral("v2 data is limited to 56 bytes"));
    }
    QByteArray frame(V2FrameSize - 1, '\0');
    frame[1] = static_cast<char>(V2Marker);
    frame[2] = static_cast<char>(paramLength);
    frame[4] = static_cast<char>(target);
    frame[5] = static_cast<char>(command);
    frame[6] = static_cast<char>(commandLength);
    for (int i = 0; i < data.size(); ++i) {
        frame[7 + i] = data.at(i);
    }
    int total = 0;
    for (int i = 1; i < frame.size(); ++i) {
        total += byteAt(frame, i);
    }
    frame[0] = static_cast<char>(total & 0xFF);
    return QByteArray(1, static_cast<char>(V2ReportId)) + frame;
}

bool decodeV2Reply(const QByteArray &raw, uint8_t command, QByteArray *payload, QString *error)
{
    if (payload) {
        payload->clear();
    }
    if (error) {
        error->clear();
    }
    if (raw.isEmpty()) {
        return false;
    }
    QByteArray data = raw;
    if (byteAt(data, 0) == V2ReportId) {
        data = data.mid(1);
    } else if (byteAt(data, 0) != V2Marker) {
        return false;
    }
    if (data.size() < 7 || byteAt(data, 0) != V2Marker) {
        return false;
    }
    if (byteAt(data, 1) == V2EventStatus) {
        return false;
    }
    if (byteAt(data, 4) != command) {
        return false;
    }
    if (byteAt(data, 1) == 0xFF) {
        if (error) {
            *error = QStringLiteral("The mouse rejected command 0x%1")
                         .arg(command, 2, 16, QLatin1Char('0'));
        }
        return false;
    }
    const int length = std::min<int>(byteAt(data, 5), data.size() - 6);
    if (payload) {
        *payload = data.mid(6, length);
    }
    return true;
}

std::optional<int> pollingFromCode(uint8_t code)
{
    switch (code) {
    case 0x08:
        return 125;
    case 0x04:
        return 250;
    case 0x02:
        return 500;
    case 0x01:
        return 1000;
    case 0x10:
        return 2000;
    case 0x20:
        return 4000;
    case 0x40:
        return 8000;
    default:
        return std::nullopt;
    }
}

std::optional<int> pollingToCode(int hertz)
{
    switch (hertz) {
    case 125:
        return 0x08;
    case 250:
        return 0x04;
    case 500:
        return 0x02;
    case 1000:
        return 0x01;
    case 2000:
        return 0x10;
    case 4000:
        return 0x20;
    case 8000:
        return 0x40;
    default:
        return std::nullopt;
    }
}

QString formatBcd(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    bool ok = false;
    const uint32_t number = trimmed.toUInt(&ok, 16);
    if (!ok) {
        return trimmed;
    }
    return QStringLiteral("%1.%2")
        .arg(number >> 8, 0, 16)
        .arg(number & 0xFF, 2, 16, QLatin1Char('0'));
}

bool batteryIsPlausible(int percent, bool charging)
{
    Q_UNUSED(charging)
    return percent >= 0 && percent <= 100;
}

} // namespace atk
