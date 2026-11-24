#pragma once

#include "types.h"

#include <QByteArray>
#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

class AtkDevice;

class DeviceWorker : public QObject
{
    Q_OBJECT

public:
    explicit DeviceWorker(QObject *parent = nullptr);
    ~DeviceWorker() override;

    void setDebug(bool debug);
    bool hasDevice() const { return m_device != nullptr; }

    void openDevice(const DeviceInfo &info);
    void closeDevice();
    void refresh();
    void refreshLive();
    void setDpiFamily(const QString &family);
    void applyDpi(const QVector<QPair<int, int>> &stages, int activeStage,
                  const QVector<QString> &colors);
    void applyPerformance(int polling, int lodCode, int debounce, bool motionSync,
                          bool ripple, bool straightLine, int sensorMode, int sleepSeconds,
                          int angleDegrees, bool angleEnabled);
    void applyButtons(const QVector<QString> &actions);
    void resetButtons();
    void setLedMode(int mode, int brightness, int speed);
    void setProfile(int profile);
    void commitSettings();
    void addStage();
    void removeLastStage();
    void rawRead(uint16_t address, int length);
    void rawWrite(uint16_t address, const QByteArray &data);

signals:
    void stateChanged(const DeviceState &state);
    void liveStateChanged(const DeviceState &state);
    void deviceOpened(const DeviceState &state);
    void deviceClosed();
    void errorOccurred(const QString &message);
    void logMessage(const QString &message);
    void busyChanged(bool busy);
    void rawDataRead(const QByteArray &data);

private:
    void run(const QString &label, const std::function<void()> &operation);
    void fail(const QString &message);

    std::unique_ptr<AtkDevice> m_device;
    bool m_debug = false;
};
