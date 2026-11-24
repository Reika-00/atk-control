#include "batteryservice.h"

#include <QDateTime>

BatteryService::BatteryService(QObject *parent)
    : QObject(parent)
{
    m_data.insert(QStringLiteral("connected"), false);
    m_data.insert(QStringLiteral("percent"), -1);
    m_data.insert(QStringLiteral("charging"), false);
    m_data.insert(QStringLiteral("voltage_mv"), 0);
    m_data.insert(QStringLiteral("model"), QString());
    m_data.insert(QStringLiteral("updated"), 0);
}

QVariantMap BatteryService::GetBattery() const
{
    return m_data;
}

void BatteryService::update(const DeviceState &state)
{
    QVariantMap data;
    data.insert(QStringLiteral("connected"), true);
    data.insert(QStringLiteral("percent"), state.battery.value_or(-1));
    data.insert(QStringLiteral("charging"), state.charging.value_or(false));
    data.insert(QStringLiteral("voltage_mv"), state.voltageMv.value_or(0));
    data.insert(QStringLiteral("model"), state.modelName);
    data.insert(QStringLiteral("updated"), QDateTime::currentSecsSinceEpoch());
    if (data != m_data) {
        m_data = data;
        emit BatteryChanged(m_data);
    }
}

void BatteryService::clear()
{
    QVariantMap data;
    data.insert(QStringLiteral("connected"), false);
    data.insert(QStringLiteral("percent"), -1);
    data.insert(QStringLiteral("charging"), false);
    data.insert(QStringLiteral("voltage_mv"), 0);
    data.insert(QStringLiteral("model"), QString());
    data.insert(QStringLiteral("updated"), QDateTime::currentSecsSinceEpoch());
    if (data != m_data) {
        m_data = data;
        emit BatteryChanged(m_data);
    }
}
