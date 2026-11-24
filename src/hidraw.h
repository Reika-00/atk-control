#pragma once

#include "types.h"

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

struct BatteryReading
{
    std::optional<int> percent;
    std::optional<bool> charging;
    std::optional<int> millivolts;
};

struct ReceiverReading
{
    bool online = false;
    int status = 0;
    QString rfId;
};

QVector<DeviceInfo> discoverDevices();

class HidrawDevice
{
public:
    HidrawDevice() = default;
    ~HidrawDevice();

    HidrawDevice(const HidrawDevice &) = delete;
    HidrawDevice &operator=(const HidrawDevice &) = delete;

    void open(const QString &path);
    void close();
    bool isOpen() const { return m_fd >= 0; }
    QString path() const { return m_path; }
    void setDebug(bool debug) { m_debug = debug; }
    void setV2Wireless(bool wireless) { m_v2Wireless = wireless; }

    QByteArray readEeprom(uint16_t address, int length);
    void writeEeprom(uint16_t address, const QByteArray &data);
    QByteArray simpleCommand(uint8_t command, int attempts = 3);
    BatteryReading readBattery();
    QPair<int, int> readIdentity();
    QString readFirmware(int attempts = 1);
    std::optional<int> readProfile();
    void setProfile(int profile);
    std::optional<ReceiverReading> receiverOnline();

    QByteArray commandV2(uint8_t command, int paramLength, int commandLength,
                         const QByteArray &data = {}, int target = -1);
    QByteArray readMemoryV2(uint16_t address, int length);
    QPair<int, int> readIdentityV2();
    QString readVersionV2();
    QByteArray readConfigV2();
    QByteArray readDpiV2();
    int readSensorModeV2();
    QByteArray readLightingV2();
    QByteArray readButtonsV2();
    BatteryReading readBatteryV2();
    QString readDongleVersionV2();
    bool readOnlineV2();

    void setRateV2(int code);
    void setDpiV2(int stage, int x, int y, const QString &color, bool active);
    void setLodV2(int level);
    void setMotionSyncV2(bool enabled);
    void setAngleSnapV2(bool enabled);
    void setRippleV2(bool enabled);
    void setDebounceV2(int milliseconds);
    void setSleepV2(int seconds);
    void setSensorModeV2(int code);
    void setLightingV2(int mode, int brightness, int speed);
    void setProfileV2(int profile);
    void setButtonV2(int index, const QVector<int> &action);
    void resetV2();

private:
    QByteArray exchangeV1(const QByteArray &payload,
                          const std::function<bool(const QByteArray &)> &matcher,
                          int attempts = 3);
    void writeReport(const QByteArray &report);
    QByteArray readReport(int timeoutMs);
    void drain(int settleMs);

    int m_fd = -1;
    bool m_debug = false;
    bool m_v2Wireless = false;
    QString m_path;
};
