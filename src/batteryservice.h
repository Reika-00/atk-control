#pragma once

#include "types.h"

#include <QObject>
#include <QVariantMap>

class BatteryService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.atk.Control.Battery")

public:
    explicit BatteryService(QObject *parent = nullptr);

public slots:
    QVariantMap GetBattery() const;

signals:
    void BatteryChanged(const QVariantMap &data);

public:
    void update(const DeviceState &state);
    void clear();

private:
    QVariantMap m_data;
};
