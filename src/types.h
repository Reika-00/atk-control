#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QVector>

#include <cstdint>
#include <optional>
#include <stdexcept>

class AtkError : public std::runtime_error
{
public:
    explicit AtkError(const QString &message)
        : std::runtime_error(message.toStdString())
    {
    }
};

struct DeviceInfo
{
    QString path;
    QString sysPath;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    QString productName;
    QString bcdDevice;
    QString protocol;
    QString interfaceName;
    bool receiver = false;
    bool accessible = false;
    QByteArray descriptor;
};

struct SensorInfo
{
    QString name;
    QString family;
    int minDpi = 50;
    int maxDpi = 30000;
};

struct ModelInfo
{
    int cid = -1;
    int mid = -1;
    QString name;
    QString brand;
    QString sensor;
    int maxPolling = 8000;
    bool angleSupported = true;
    bool dpiLighting = true;
    bool stageColors = true;
    bool performanceMode = false;
    bool sensorMax = false;
    bool buttonRemap = false;
    bool bottomButton = true;
    bool normalizesDefaultButtons = false;
    bool v2 = false;
    QString support = QStringLiteral("experimental");
};

struct ButtonInfo
{
    QString name;
    QString action;
    QString actionId;
    QByteArray raw;
    bool readable = true;
};

struct DeviceState
{
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    QString usbName;
    QString modelName;
    QString brand;
    QString sensor;
    QString firmware;
    QString dongleFirmware;
    QString usbBcd;
    QString receiverName;
    QString rfId;
    QString protocol;
    QString dpiFamily;
    std::optional<int> cid;
    std::optional<int> mid;
    bool viaReceiver = false;
    bool wired = false;
    bool receiverOnline = false;
    bool readOnly = false;
    bool experimental = false;
    QString support;

    std::optional<int> battery;
    std::optional<int> voltageMv;
    std::optional<bool> charging;

    int dpi = 800;
    int dpiY = 800;
    int activeStage = 0;
    int stageCount = 1;
    QVector<QPair<int, int>> stages;
    QVector<QString> stageColors;

    int pollingRate = 1000;
    std::optional<int> lodCode;
    std::optional<double> lodMm;
    std::optional<int> debounceMs;
    std::optional<int> sleepSeconds;
    std::optional<int> sensorMode;
    std::optional<bool> motionSync;
    std::optional<bool> ripple;
    std::optional<bool> straightLine;
    std::optional<bool> angleEnabled;
    std::optional<bool> performanceMode;
    std::optional<int> angleDegrees;

    std::optional<int> ledMode;
    std::optional<int> ledBrightness;
    std::optional<int> ledSpeed;

    std::optional<int> profile;
    QVector<ButtonInfo> buttons;

    bool supportsAngle = false;
    bool supportsPerformanceMode = false;
    bool supportsSensorMax = false;
    bool supportsButtonRemap = false;
    bool supportsLighting = false;
    bool supportsStageColors = false;
    bool supportsStageCount = true;
    bool supportsProfiles = true;

    QVector<int> pollingRates;
    QVector<int> debounceOptions;
    QVector<int> sleepOptions;
    QVector<QPair<int, double>> lodChoices;
    QVector<int> dpiOptions;

    QString linkLabel() const
    {
        if (wired) {
            return QStringLiteral("USB cable");
        }
        if (viaReceiver) {
            return receiverName.isEmpty() ? QStringLiteral("Wireless receiver") : receiverName;
        }
        return QStringLiteral("2.4 GHz wireless");
    }
};

Q_DECLARE_METATYPE(DeviceState)
