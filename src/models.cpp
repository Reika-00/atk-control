#include "models.h"

#include "protocol.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace {

ModelInfo makeModel(int cid, int mid, const QString &name, const QString &brand,
                    const QString &sensor, int maxPolling = 8000,
                    bool angle = true, bool dpiLight = true, bool stageColors = true,
                    bool performance = false, bool sensorMax = false,
                    bool buttonRemap = false, bool bottomButton = true,
                    bool normalizesButtons = false, bool v2 = false,
                    const QString &support = QStringLiteral("experimental"))
{
    ModelInfo model;
    model.cid = cid;
    model.mid = mid;
    model.name = name;
    model.brand = brand;
    model.sensor = sensor;
    model.maxPolling = maxPolling;
    model.angleSupported = angle;
    model.dpiLighting = dpiLight;
    model.stageColors = stageColors;
    model.performanceMode = performance;
    model.sensorMax = sensorMax;
    model.buttonRemap = buttonRemap;
    model.bottomButton = bottomButton;
    model.normalizesDefaultButtons = normalizesButtons;
    model.v2 = v2;
    model.support = support;
    return model;
}

const QVector<ModelInfo> &v1Models()
{
    static const QVector<ModelInfo> models = {
        makeModel(2, 47, QStringLiteral("MAD R MAJOR"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, false),
        makeModel(2, 49, QStringLiteral("MAD R"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3395"), 8000, true, true, true, false, false, false, false),
        makeModel(2, 54, QStringLiteral("MAD R +"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3395"), 8000, true, true, true, false, true, true, false,
                  false, false, QStringLiteral("verified")),
        makeModel(2, 55, QStringLiteral("MAD R MAJOR+"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, false),
        makeModel(2, 133, QStringLiteral("MAD R"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, true, false, false, false, false),
        makeModel(2, 7, QStringLiteral("VXE R1 Pro"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395")),
        makeModel(2, 8, QStringLiteral("VXE R1 Pro"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395"), 8000, true, true, true, false, true, true, true,
                  false, false, QStringLiteral("verified")),
        makeModel(2, 11, QStringLiteral("VXE R1"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395")),
        makeModel(2, 12, QStringLiteral("VXE R1"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395")),
        makeModel(2, 27, QStringLiteral("VXE R1 Pro Max"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395")),
        makeModel(2, 28, QStringLiteral("VXE R1 Pro Max"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395")),
        makeModel(2, 125, QStringLiteral("VXE R1 Pro"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3950Ultra")),
        makeModel(2, 126, QStringLiteral("VXE R1 Pro Max"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3950Ultra")),
        makeModel(2, 127, QStringLiteral("VXE R1"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3950Ultra")),
        makeModel(2, 39, QStringLiteral("ATK X1 Pro Max"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950")),
        makeModel(2, 83, QStringLiteral("ATK A9 Plus NearLink"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3395"), 1000),
        makeModel(2, 31, QStringLiteral("VXE R1 SE+"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395SE"), 1000, true, true, true, false, false, true, true),
        makeModel(1, 3, QStringLiteral("ATK A9 Ultimate"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, true, false, false, true,
                  true, false, false, QStringLiteral("verified")),
        makeModel(2, 114, QStringLiteral("VXE R1 Ultra"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395")),
        makeModel(2, 36, QStringLiteral("ATK F1 Pro"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 37, QStringLiteral("ATK F1 Pro Max"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 38, QStringLiteral("ATK X1 Pro"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 40, QStringLiteral("ATK Z1 Pro"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 41, QStringLiteral("ATK Z1 Pro Max"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 42, QStringLiteral("ATK F1 Ultimate"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, false, false, false, false,
                  false, false, false, QStringLiteral("recognised")),
        makeModel(2, 43, QStringLiteral("ATK X1 Ultra"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 45, QStringLiteral("ATK F1 Ultra"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, false, false, false, false,
                  false, false, false, QStringLiteral("recognised")),
        makeModel(2, 46, QStringLiteral("ATK X1 Ultimate"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, true, false, false, false,
                  false, false, false, QStringLiteral("recognised")),
        makeModel(2, 50, QStringLiteral("ATK F1 Extreme"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, false, false, false, false,
                  false, false, false, QStringLiteral("recognised")),
        makeModel(2, 53, QStringLiteral("ATK Z1 Ultra"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 64, QStringLiteral("ATK U2 Mini"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 65, QStringLiteral("ATK U2 Ultimate"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 66, QStringLiteral("ATK U2 Ultra"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 85, QStringLiteral("ATK A9 Pro Max"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3395Ultra"), 1000, true, true, false, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 87, QStringLiteral("ATK A9 Ultra"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, true, true, false, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 99, QStringLiteral("ATK A9"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3395SE"), 1000, true, true, false, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 100, QStringLiteral("ATK U2 Pro Max"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3395Ultra"), 8000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 29, QStringLiteral("VXE R1 SE"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395SE"), 1000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 63, QStringLiteral("VXE R1S+"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3311"), 1000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 72, QStringLiteral("VXE V3"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3311"), 1000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 73, QStringLiteral("VXE V3 Pro"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395SE"), 1000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 121, QStringLiteral("VXE X3"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3315"), 1000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 122, QStringLiteral("VXE X3 Pro"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395SE"), 1000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
        makeModel(2, 123, QStringLiteral("VXE X3 Turbo"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395SE"), 1000, true, true, true, false, false, false, true,
                  false, false, QStringLiteral("recognised")),
    };
    return models;
}

const QVector<ModelInfo> &v2Models()
{
    static const QVector<ModelInfo> models = {
        makeModel(2, 1, QStringLiteral("ATK Dragonfly A9 Pro Max 2.0"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3395Ultra"), 8000, false, true, true, true, true, true, true,
                  false, true),
        makeModel(2, 14, QStringLiteral("ATK Dragonfly A9 Pro Max 2.0"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950Ultra"), 8000, false, true, true, true, true, true, true,
                  false, true),
        makeModel(2, 27, QStringLiteral("ATK Dragonfly A9 Pro Max 2.0"), QStringLiteral("ATK"),
                  QStringLiteral("PAW3950DM"), 8000, false, true, true, true, true, true, true,
                  false, true),
        makeModel(2, 8, QStringLiteral("VXE R1 NK"), QStringLiteral("VXE"),
                  QStringLiteral("PAW3395SE"), 1000, false, false, false, false, false, true, true,
                  true, true),
    };
    return models;
}

const QHash<QString, SensorInfo> &sensorTable()
{
    static const QHash<QString, SensorInfo> sensors = {
        {QStringLiteral("PAW3395"), {QStringLiteral("PAW3395"), QStringLiteral("step50"), 100, 30000}},
        {QStringLiteral("PAW3395SE"), {QStringLiteral("PAW3395SE"), QStringLiteral("step50"), 100, 18000}},
        {QStringLiteral("PAW3950"), {QStringLiteral("PAW3950"), QStringLiteral("step50"), 50, 36000}},
        {QStringLiteral("PAW3950Ultra"), {QStringLiteral("PAW3950Ultra"), QStringLiteral("ultra"), 10, 42000}},
        {QStringLiteral("PAW3950DM"), {QStringLiteral("PAW3950DM"), QStringLiteral("step50"), 50, 36000}},
        {QStringLiteral("PAW3395Ultra"), {QStringLiteral("PAW3395Ultra"), QStringLiteral("step50"), 100, 30000}},
        {QStringLiteral("PAW3311"), {QStringLiteral("PAW3311"), QStringLiteral("step50"), 200, 15000}},
        {QStringLiteral("PAW3315"), {QStringLiteral("PAW3315"), QStringLiteral("step50"), 200, 16000}},
        {QStringLiteral("CORE26K"), {QStringLiteral("CORE26K"), QStringLiteral("step50"), 50, 26000}},
    };
    return sensors;
}

const QHash<uint16_t, QPair<int, QString>> &receiverTable()
{
    static const QHash<uint16_t, QPair<int, QString>> receivers = {
        {0x1040, {8000, QStringLiteral("MAD 8K Dongle")}},
        {0x101A, {1000, QStringLiteral("ATK 1K Dongle")}},
        {0x101B, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x1031, {8000, QStringLiteral("ATK 8K Dongle Light")}},
        {0x10C9, {1000, QStringLiteral("ATK NearLink Dongle")}},
        {0x111A, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x111B, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x1159, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x115E, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11A7, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11A9, {4000, QStringLiteral("ATK 4K Dongle")}},
        {0x11C8, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11CD, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11CE, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11D8, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11D9, {8000, QStringLiteral("ATK 8K Wireless Dongle")}},
        {0x11DC, {8000, QStringLiteral("ATK 8K NANO Dongle")}},
        {0x11F4, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11FD, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x11FE, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x1216, {1000, QStringLiteral("ATK NK NANO Dongle")}},
        {0x1278, {8000, QStringLiteral("Compx 8K Dongle-L")}},
        {0x1279, {8000, QStringLiteral("Compx 8K Dongle")}},
        {0x127A, {8000, QStringLiteral("Compx 8K Dongle")}},
        {0x127B, {8000, QStringLiteral("Compx 8K Dongle")}},
        {0x12CC, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x12E9, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x1303, {8000, QStringLiteral("ATK 8K Dongle")}},
        {0x104D, {1000, QStringLiteral("ATK 1K Dongle")}},
        {0x1085, {1000, QStringLiteral("VXE 1K Dongle")}},
        {0xF400, {1000, QStringLiteral("VXE 1K Dongle")}},
        {0xF505, {4000, QStringLiteral("VGN/VXE 4K Dongle")}},
        {0xF50B, {1000, QStringLiteral("VXE 1K Dongle")}},
        {0xF58A, {1000, QStringLiteral("VXE 1K Dongle")}},
        {0xF58D, {4000, QStringLiteral("VXE 4K Dongle")}},
        {0xF58E, {1000, QStringLiteral("VXE 1K Dongle")}},
    };
    return receivers;
}

void appendUnique(QVector<int> &values, int value)
{
    if (!values.contains(value)) {
        values.append(value);
        std::sort(values.begin(), values.end());
    }
}

} // namespace

SensorInfo sensorFor(const QString &name)
{
    const auto it = sensorTable().constFind(name);
    if (it != sensorTable().constEnd()) {
        return it.value();
    }
    return unknownSensor();
}

SensorInfo unknownSensor()
{
    return {QStringLiteral("Unknown sensor"), QStringLiteral("step50"), 50, 30000};
}

ModelInfo modelFor(std::optional<int> cid, std::optional<int> mid, bool v2)
{
    if (cid && mid) {
        const auto &models = v2 ? v2Models() : v1Models();
        for (const ModelInfo &model : models) {
            if (model.cid == *cid && model.mid == *mid) {
                return model;
            }
        }
    }
    ModelInfo unknown;
    unknown.cid = cid.value_or(-1);
    unknown.mid = mid.value_or(-1);
    unknown.name.clear();
    unknown.brand = QStringLiteral("ATK");
    unknown.sensor = QStringLiteral("Unknown sensor");
    unknown.support = QStringLiteral("unknown");
    unknown.v2 = v2;
    return unknown;
}

bool isReceiver(uint16_t productId, const QString &usbName)
{
    if (receiverTable().contains(productId)) {
        return true;
    }
    const QString lowered = usbName.toLower();
    return lowered.contains(QStringLiteral("dongle"))
        || lowered.contains(QStringLiteral("receiver"))
        || lowered.contains(QStringLiteral("8k"))
        || lowered.contains(QStringLiteral("4k"));
}

QString receiverName(uint16_t productId, const QString &usbName)
{
    const auto it = receiverTable().constFind(productId);
    if (it != receiverTable().constEnd()) {
        return it.value().second;
    }
    return usbName.isEmpty() ? QStringLiteral("Wireless receiver") : usbName;
}

int receiverCeiling(uint16_t productId, const QString &usbName)
{
    const auto it = receiverTable().constFind(productId);
    if (it != receiverTable().constEnd()) {
        return it.value().first;
    }
    return isReceiver(productId, usbName) ? 8000 : 1000;
}

Capabilities capabilitiesFor(const ModelInfo *model, bool viaReceiver, uint16_t productId,
                             const QString &usbName, int activePolling,
                             std::optional<int> debounce, std::optional<int> sleep,
                             bool v2, const QString &dpiFamilyOverride)
{
    Capabilities caps;
    caps.modelName = model && !model->name.isEmpty() ? model->name : usbName;
    caps.brand = model && !model->name.isEmpty() ? model->brand
                                                 : (usbName.contains(QStringLiteral("VXE"), Qt::CaseInsensitive)
                                                        ? QStringLiteral("VXE")
                                                        : QStringLiteral("ATK"));
    caps.sensor = model && !model->name.isEmpty() ? sensorFor(model->sensor) : unknownSensor();
    if (!dpiFamilyOverride.isEmpty()) {
        caps.sensor.family = dpiFamilyOverride;
    }

    int ceiling = 1000;
    if (viaReceiver) {
        ceiling = std::min(receiverCeiling(productId, usbName),
                           model && !model->name.isEmpty() ? model->maxPolling : 8000);
    } else if (model && !model->name.isEmpty()) {
        ceiling = v2 ? model->maxPolling : std::min(model->maxPolling, std::max(1000, activePolling));
    } else {
        ceiling = 1000;
    }
    caps.linkCeiling = ceiling;

    for (int rate : {125, 250, 500, 1000, 2000, 4000, 8000}) {
        if (rate <= ceiling) {
            caps.pollingRates.append(rate);
        }
    }
    appendUnique(caps.pollingRates, activePolling);

    caps.lodChoices = {{1, 1.0}, {2, 2.0}};
    if (caps.sensor.name == QStringLiteral("PAW3950")
        || caps.sensor.name == QStringLiteral("PAW3950DM")) {
        caps.lodChoices.prepend({3, 0.7});
    }

    caps.debounceOptions = {0, 1, 2, 4, 8, 15, 20};
    if (debounce) {
        appendUnique(caps.debounceOptions, *debounce);
    }
    caps.sleepOptions = {30, 60, 120, 180, 300, 600, 1200, 1800};
    if (sleep) {
        appendUnique(caps.sleepOptions, *sleep);
    }
    caps.dpiOptions = atk::dpiValues(caps.sensor.minDpi, caps.sensor.maxDpi, caps.sensor.family);

    caps.dpiLighting = model && !model->name.isEmpty() ? model->dpiLighting : false;
    caps.stageColors = model && !model->name.isEmpty() ? model->stageColors : false;
    caps.angle = model && !model->name.isEmpty() ? model->angleSupported : false;
    caps.performanceMode = model && !model->name.isEmpty() ? model->performanceMode : false;
    caps.sensorMax = model && !model->name.isEmpty() ? model->sensorMax : false;
    caps.buttonRemap = model && !model->name.isEmpty() ? model->buttonRemap : false;
    caps.bottomButton = model && !model->name.isEmpty() ? model->bottomButton : true;
    caps.stageCount = !v2;
    caps.profiles = true;
    return caps;
}
