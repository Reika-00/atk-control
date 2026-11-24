#pragma once

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QVector>

#include <cstdint>
#include <optional>

namespace atk {

constexpr int ReportId = 0x08;
constexpr int PayloadSize = 16;
constexpr int WireSize = 17;
constexpr int ChecksumTotal = 0x55;
constexpr int MaxEepromChunk = 10;

constexpr int V2ReportId = 0x08;
constexpr int V2FrameSize = 64;
constexpr int V2Marker = 0x72;
constexpr int V2EventStatus = 0x73;
constexpr int V2TargetDongle = 0;
constexpr int V2TargetMouse = 1;
constexpr int V2ReadChunk = 10;

constexpr uint8_t CmdReadEeprom = 0x08;
constexpr uint8_t CmdWriteEeprom = 0x07;
constexpr uint8_t CmdBattery = 0x04;
constexpr uint8_t CmdVersion = 0x12;
constexpr uint8_t CmdCidMid = 0x10;
constexpr uint8_t CmdCurrentProfile = 0x0E;
constexpr uint8_t CmdSetProfile = 0x0F;
constexpr uint8_t CmdReceiverOnline = 0x03;

constexpr uint16_t AddrSystem = 0x0000;
constexpr uint16_t AddrLiftOff = 0x000A;
constexpr uint16_t AddrDpiBase = 0x000C;
constexpr uint16_t AddrStageColorBase = 0x002C;
constexpr uint16_t AddrDpiLight = 0x004C;
constexpr uint16_t AddrAdvanced = 0x00A9;
constexpr uint16_t AddrAngle = 0x00BD;
constexpr uint16_t AddrSensorMode = 0x00B5;

constexpr uint8_t V2CmdSetRate = 0x01;
constexpr uint8_t V2CmdSetDpi = 0x02;
constexpr uint8_t V2CmdSetLod = 0x03;
constexpr uint8_t V2CmdSetButton = 0x05;
constexpr uint8_t V2CmdBattery = 0x07;
constexpr uint8_t V2CmdReadConfig = 0x09;
constexpr uint8_t V2CmdSetProfile = 0x0A;
constexpr uint8_t V2CmdSetAngle = 0x0B;
constexpr uint8_t V2CmdSetRipple = 0x0C;
constexpr uint8_t V2CmdSetMotionSync = 0x0D;
constexpr uint8_t V2CmdReadCharging = 0x0F;
constexpr uint8_t V2CmdReset = 0x13;
constexpr uint8_t V2CmdSetLight = 0x14;
constexpr uint8_t V2CmdSetSleep = 0x15;
constexpr uint8_t V2CmdSetDebounce = 0x16;
constexpr uint8_t V2CmdReadMemory = 0x17;
constexpr uint8_t V2CmdReadFirmware = 0x1C;
constexpr uint8_t V2CmdSetSensorMode = 0x1F;
constexpr uint8_t V2CmdReadIdentity = 0x4A;
constexpr uint8_t V2CmdReadDongleVersion = 0x88;
constexpr uint8_t V2CmdReadOnline = 0x81;

uint8_t checksum15(const QByteArray &payload15);
QByteArray buildFrame(uint8_t command, const QByteArray &body = {});
QByteArray buildRead(uint16_t address, int length);
QByteArray buildWrite(uint16_t address, const QByteArray &data);
bool reportIsValid(const QByteArray &payload);
QByteArray decodeReport(const QByteArray &raw);

std::optional<int> decodePair(uint8_t value, uint8_t check);
QByteArray packPair(int value);

std::optional<int> decodeDpiAxis(uint8_t byte, int nibble, const QString &family);
QPair<uint8_t, uint8_t> encodeDpiAxis(int dpi, const QString &family);
QByteArray packDpiStage(int x, int y, const QString &family);
std::optional<QPair<int, int>> unpackDpiStage(const QByteArray &data, const QString &family);
QVector<int> dpiValues(int minDpi, int maxDpi, const QString &family);

QByteArray packRgb(const QString &color);
QString unpackRgb(const QByteArray &record);

QByteArray buildV2Frame(uint8_t command, int paramLength, int commandLength,
                        const QByteArray &data = {}, int target = V2TargetDongle);
bool decodeV2Reply(const QByteArray &raw, uint8_t command, QByteArray *payload, QString *error);

std::optional<int> pollingFromCode(uint8_t code);
std::optional<int> pollingToCode(int hertz);

QString formatBcd(const QString &value);
bool batteryIsPlausible(int percent, bool charging);

} // namespace atk
