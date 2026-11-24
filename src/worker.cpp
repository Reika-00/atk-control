#include "worker.h"

#include "device.h"

#include <QDebug>

DeviceWorker::DeviceWorker(QObject *parent)
    : QObject(parent)
{
}

DeviceWorker::~DeviceWorker()
{
    closeDevice();
}

void DeviceWorker::setDebug(bool debug)
{
    m_debug = debug;
    if (m_device) {
        m_device->setDebug(debug);
    }
}

void DeviceWorker::fail(const QString &message)
{
    emit errorOccurred(message);
    emit logMessage(message);
}

void DeviceWorker::openDevice(const DeviceInfo &info)
{
    emit busyChanged(true);
    try {
        if (m_device) {
            m_device.reset();
        }
        auto device = std::make_unique<AtkDevice>(info, m_debug);
        device->open();
        m_device = std::move(device);
        const DeviceState state = m_device->state();
        emit logMessage(QStringLiteral("Connected to %1 on %2").arg(state.modelName, info.path));
        emit deviceOpened(state);
        emit stateChanged(state);
    } catch (const std::exception &error) {
        m_device.reset();
        fail(QString::fromUtf8(error.what()));
        emit deviceClosed();
    }
    emit busyChanged(false);
}

void DeviceWorker::closeDevice()
{
    if (m_device) {
        m_device.reset();
        emit deviceClosed();
    }
}

void DeviceWorker::refresh()
{
    run(QStringLiteral("Refresh"), [] {});
}

void DeviceWorker::refreshLive()
{
    if (!m_device) {
        return;
    }
    try {
        const DeviceState state = m_device->readLive();
        emit liveStateChanged(state);
    } catch (const std::exception &error) {
        emit logMessage(QStringLiteral("Live refresh: %1").arg(QString::fromUtf8(error.what())));
    }
}

void DeviceWorker::run(const QString &label, const std::function<void()> &operation)
{
    if (!m_device) {
        fail(QStringLiteral("No ATK/VXE mouse is connected"));
        return;
    }
    emit busyChanged(true);
    try {
        operation();
        const DeviceState state = m_device->readFull();
        emit stateChanged(state);
        emit logMessage(QStringLiteral("%1: done").arg(label));
    } catch (const std::exception &error) {
        fail(QStringLiteral("%1: %2").arg(label, QString::fromUtf8(error.what())));
    }
    emit busyChanged(false);
}

void DeviceWorker::setDpiFamily(const QString &family)
{
    if (!m_device) {
        fail(QStringLiteral("No ATK/VXE mouse is connected"));
        return;
    }
    m_device->setDpiFamilyOverride(family);
    refresh();
}

void DeviceWorker::applyDpi(const QVector<QPair<int, int>> &stages, int activeStage,
                            const QVector<QString> &colors)
{
    run(QStringLiteral("Apply DPI"), [this, stages, activeStage, colors] {
        m_device->applyDpi(stages, activeStage, colors);
    });
}

void DeviceWorker::applyPerformance(int polling, int lodCode, int debounce, bool motionSync,
                                    bool ripple, bool straightLine, int sensorMode, int sleepSeconds,
                                    int angleDegrees, bool angleEnabled)
{
    run(QStringLiteral("Apply performance settings"), [=] {
        m_device->applyPerformance(polling, lodCode, debounce, motionSync, ripple, straightLine,
                                   sensorMode, sleepSeconds, angleDegrees, angleEnabled);
    });
}

void DeviceWorker::applyButtons(const QVector<QString> &actions)
{
    run(QStringLiteral("Apply button assignments"), [this, actions] {
        m_device->applyButtons(actions);
    });
}

void DeviceWorker::resetButtons()
{
    run(QStringLiteral("Reset button assignments"), [this] {
        m_device->resetButtons();
    });
}

void DeviceWorker::setLedMode(int mode, int brightness, int speed)
{
    run(QStringLiteral("Apply lighting"), [=] {
        m_device->setLedMode(mode, brightness, speed);
    });
}

void DeviceWorker::setProfile(int profile)
{
    run(QStringLiteral("Switch profile"), [=] {
        m_device->setProfile(profile);
    });
}

void DeviceWorker::commitSettings()
{
    run(QStringLiteral("Save profile"), [this] {
        m_device->commitSettings();
    });
}

void DeviceWorker::addStage()
{
    run(QStringLiteral("Add DPI stage"), [this] {
        m_device->addStage();
    });
}

void DeviceWorker::removeLastStage()
{
    run(QStringLiteral("Remove DPI stage"), [this] {
        m_device->removeLastStage();
    });
}

void DeviceWorker::rawRead(uint16_t address, int length)
{
    if (!m_device) {
        fail(QStringLiteral("No ATK/VXE mouse is connected"));
        return;
    }
    emit busyChanged(true);
    try {
        const QByteArray data = m_device->rawRead(address, length);
        emit rawDataRead(data);
    } catch (const std::exception &error) {
        fail(QStringLiteral("Raw read: %1").arg(QString::fromUtf8(error.what())));
    }
    emit busyChanged(false);
}

void DeviceWorker::rawWrite(uint16_t address, const QByteArray &data)
{
    if (!m_device) {
        fail(QStringLiteral("No ATK/VXE mouse is connected"));
        return;
    }
    emit busyChanged(true);
    try {
        m_device->rawWrite(address, data);
        emit logMessage(QStringLiteral("Raw write at 0x%1: done")
                            .arg(address, 4, 16, QLatin1Char('0')));
        const DeviceState state = m_device->readFull();
        emit stateChanged(state);
    } catch (const std::exception &error) {
        fail(QStringLiteral("Raw write: %1").arg(QString::fromUtf8(error.what())));
    }
    emit busyChanged(false);
}
