#pragma once

#include "types.h"

#include <QString>
#include <QVector>

#include <optional>

struct Capabilities
{
    QString modelName;
    QString brand;
    SensorInfo sensor;
    int linkCeiling = 1000;
    QVector<int> pollingRates;
    QVector<QPair<int, double>> lodChoices;
    QVector<int> debounceOptions;
    QVector<int> sleepOptions;
    QVector<int> dpiOptions;
    bool dpiLighting = false;
    bool stageColors = false;
    bool angle = false;
    bool performanceMode = false;
    bool sensorMax = false;
    bool buttonRemap = false;
    bool profiles = true;
    bool bottomButton = true;
    bool stageCount = true;
};

SensorInfo sensorFor(const QString &name);
SensorInfo unknownSensor();
ModelInfo modelFor(std::optional<int> cid, std::optional<int> mid, bool v2);
bool isReceiver(uint16_t productId, const QString &usbName);
QString receiverName(uint16_t productId, const QString &usbName);
int receiverCeiling(uint16_t productId, const QString &usbName);
Capabilities capabilitiesFor(const ModelInfo *model, bool viaReceiver, uint16_t productId,
                             const QString &usbName, int activePolling,
                             std::optional<int> debounce, std::optional<int> sleep,
                             bool v2, const QString &dpiFamilyOverride);
