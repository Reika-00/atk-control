#pragma once

#include "hidraw.h"
#include "models.h"
#include "types.h"

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QVector>

#include <optional>

class AtkDevice
{
public:
    explicit AtkDevice(const DeviceInfo &info, bool debug = false);
    ~AtkDevice();

    AtkDevice(const AtkDevice &) = delete;
    AtkDevice &operator=(const AtkDevice &) = delete;

    void open();
    void close();
    bool isOpen() const { return m_open; }
    const DeviceInfo &info() const { return m_info; }
    const DeviceState &state() const { return m_state; }
    void setDebug(bool debug);
    void setDpiFamilyOverride(const QString &family);
    QString dpiFamily() const { return m_caps.sensor.family; }

    DeviceState readFull();
    DeviceState readLive();

    void setPolling(int hertz);
    void setActiveStage(int index);
    void setStageDpi(int index, int x, int y);
    void setStageColor(int index, const QString &color);
    void addStage();
    void removeLastStage();
    void setLiftOffCode(int code);
    void setDebounce(int milliseconds);
    void setMotionSync(bool enabled);
    void setSleep(int seconds);
    void setStraightLine(bool enabled);
    void setRipple(bool enabled);
    void setAngle(int degrees, bool enabled);
    void setSensorMode(int mode);
    void setButtonAction(int index, const QString &actionId);
    void resetButtons();
    void setLedMode(int mode, int brightness, int speed);
    void setProfile(int profile);
    void commitSettings();
    QByteArray rawRead(uint16_t address, int length);
    void rawWrite(uint16_t address, const QByteArray &data);

    void applyDpi(const QVector<QPair<int, int>> &stages, int activeStage,
                  const QVector<QString> &colors);
    void applyPerformance(int polling, int lodCode, int debounce, bool motionSync,
                          bool ripple, bool straightLine, int sensorMode, int sleepSeconds,
                          int angleDegrees, bool angleEnabled);
    void applyButtons(const QVector<QString> &actions);

private:
    void readFullV1();
    void readFullV2();
    void readLiveV1();
    void readLiveV2();
    void readButtonsV1();
    void readButtonsV2();
    void applyDpiV2(const QByteArray &raw);
    void reloadDpiV2();
    void applyConfigV2(const QByteArray &config);
    void readBatteryV1();
    void readBatteryV2();
    void require(bool condition, const QString &message) const;
    QString fallbackName() const;

    DeviceInfo m_info;
    HidrawDevice m_hid;
    DeviceState m_state;
    ModelInfo m_model;
    Capabilities m_caps;
    bool m_open = false;
    bool m_debug = false;
    QString m_dpiFamilyOverride;
};
