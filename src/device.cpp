#include "device.h"

#include "protocol.h"

#include <QRegularExpression>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>

namespace {

uint8_t u8(const QByteArray &data, int index)
{
    return static_cast<uint8_t>(data.at(index));
}

bool containsInt(const QVector<int> &values, int value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

void appendUniqueInt(QVector<int> &values, int value)
{
    if (!values.contains(value)) {
        values.append(value);
        std::sort(values.begin(), values.end());
    }
}

std::optional<double> lodMm(int code)
{
    switch (code) {
    case 1:
        return 1.0;
    case 2:
        return 2.0;
    case 3:
        return 0.7;
    default:
        return std::nullopt;
    }
}

struct ButtonAction
{
    const char *id;
    int keyClass;
    int value1;
    int value2;
    const char *label;
};

const ButtonAction v1Actions[] = {
    {"left", 0x01, 0x01, 0x00, "Left click"},
    {"right", 0x01, 0x02, 0x00, "Right click"},
    {"middle", 0x01, 0x04, 0x00, "Middle click"},
    {"back", 0x01, 0x08, 0x00, "Back"},
    {"forward", 0x01, 0x10, 0x00, "Forward"},
    {"dpi_cycle", 0x02, 0x01, 0x00, "DPI cycle"},
    {"dpi_up", 0x02, 0x02, 0x00, "DPI up"},
    {"dpi_down", 0x02, 0x03, 0x00, "DPI down"},
    {"disabled", 0x00, 0x00, 0x00, "Disabled"},
};

const char *v1ButtonNames[] = {"Left", "Right", "Middle", "Back", "Forward", "Bottom"};
const char *v1DefaultActions[] = {"left", "right", "middle", "back", "forward", "dpi_cycle"};

const ButtonAction *v1ActionForId(const QString &id)
{
    for (const ButtonAction &action : v1Actions) {
        if (id == QLatin1String(action.id)) {
            return &action;
        }
    }
    return nullptr;
}

const ButtonAction *v1ActionForRecord(int keyClass, int value1, int value2)
{
    for (const ButtonAction &action : v1Actions) {
        if (action.keyClass == keyClass && action.value1 == value1 && action.value2 == value2) {
            return &action;
        }
    }
    return nullptr;
}

QString v1ActionLabel(int keyClass, int value1, int value2)
{
    if (const ButtonAction *action = v1ActionForRecord(keyClass, value1, value2)) {
        return QString::fromLatin1(action->label);
    }
    if (keyClass == 0x01) {
        return QStringLiteral("Mouse action 0x%1").arg(value1, 2, 16, QLatin1Char('0'));
    }
    if (keyClass == 0x02) {
        return QStringLiteral("DPI control");
    }
    static const char *classes[] = {"Disabled", "Mouse button", "DPI control", "Horizontal scroll",
                                    "Fire key", "Shortcut", "Macro", "Polling rate",
                                    "Lighting", "Profile", "DPI lock", "Wheel"};
    QString label = keyClass >= 0 && keyClass < 12 ? QString::fromLatin1(classes[keyClass])
                                                   : QStringLiteral("Action");
    return QStringLiteral("%1 (0x%2, 0x%3)")
        .arg(label)
        .arg(value1, 2, 16, QLatin1Char('0'))
        .arg(value2, 2, 16, QLatin1Char('0'));
}

QByteArray packButtonRecord(int keyClass, int value1, int value2)
{
    const int total = (keyClass + value1 + value2) & 0xFF;
    QByteArray out(4, '\0');
    out[0] = static_cast<char>(keyClass & 0xFF);
    out[1] = static_cast<char>(value1 & 0xFF);
    out[2] = static_cast<char>(value2 & 0xFF);
    out[3] = static_cast<char>((0x55 - total) & 0xFF);
    return out;
}

int buttonAddress(int index)
{
    static const int bases[] = {0x0060, 0x0068, 0x0070};
    return bases[index / 2] + (index % 2) * 4;
}

struct V2Action
{
    const char *id;
    int a;
    int b;
    int c;
    int d;
    const char *label;
};

const V2Action v2Actions[] = {
    {"left", 1, 0, 0, 0, "Left click"},
    {"right", 1, 0, 1, 0, "Right click"},
    {"middle", 1, 0, 2, 0, "Middle click"},
    {"back", 1, 0, 3, 0, "Back"},
    {"forward", 1, 0, 4, 0, "Forward"},
    {"dpi_cycle", 10, 0, 0, 0, "DPI cycle"},
    {"dpi_up", 11, 0, 0, 0, "DPI up"},
    {"dpi_down", 12, 0, 0, 0, "DPI down"},
    {"disabled", 255, 0, 0, 0, "Disabled"},
};

const V2Action *v2ActionForId(const QString &id)
{
    for (const V2Action &action : v2Actions) {
        if (id == QLatin1String(action.id)) {
            return &action;
        }
    }
    return nullptr;
}

const V2Action *v2ActionForRecord(int a, int b, int c, int d)
{
    for (const V2Action &action : v2Actions) {
        if (action.a == a && action.b == b && action.c == c && action.d == d) {
            return &action;
        }
    }
    return nullptr;
}

QString v2DefaultButtonId(int index)
{
    static const char *defaults[] = {"left", "right", "middle", "back", "forward", "dpi_cycle"};
    return index >= 0 && index < 6 ? QString::fromLatin1(defaults[index]) : QString();
}

int v2RateToCode(int hertz)
{
    switch (hertz) {
    case 1000:
        return 0;
    case 500:
        return 1;
    case 250:
        return 2;
    case 125:
        return 3;
    case 8000:
        return 4;
    case 4000:
        return 5;
    case 2000:
        return 6;
    default:
        return -1;
    }
}

int v2CodeToRate(int code)
{
    switch (code) {
    case 0:
        return 1000;
    case 1:
        return 500;
    case 2:
        return 250;
    case 3:
        return 125;
    case 4:
        return 8000;
    case 5:
        return 4000;
    case 6:
        return 2000;
    default:
        return 1000;
    }
}

int v2ModeToCode(int mode)
{
    switch (mode) {
    case 0:
        return 0;
    case 1:
        return 2;
    case 2:
        return 1;
    default:
        return 0;
    }
}

int v2CodeToMode(int code)
{
    switch (code) {
    case 0:
        return 0;
    case 1:
        return 2;
    case 2:
        return 1;
    default:
        return 0;
    }
}

int v2SensorModeToCode(int mode)
{
    switch (mode) {
    case 0:
        return 0;
    case 1:
        return 4;
    case 2:
        return 5;
    default:
        return 0;
    }
}

int v2CodeToSensorMode(int code)
{
    switch (code) {
    case 0:
        return 0;
    case 4:
        return 1;
    case 5:
        return 2;
    default:
        return 0;
    }
}

int nearestIndex(const QVector<int> &values, int value)
{
    if (values.isEmpty()) {
        return 0;
    }
    int best = 0;
    int bestDistance = std::abs(values.at(0) - value);
    for (int i = 1; i < values.size(); ++i) {
        const int distance = std::abs(values.at(i) - value);
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

struct LightingV1
{
    int mode = 0;
    int brightness = 0;
    int speed = 0;
};

std::optional<LightingV1> decodeLightingV1(const QByteArray &raw)
{
    if (raw.size() < 8) {
        return std::nullopt;
    }
    const std::optional<int> effect = raw.mid(0, 2) == QByteArray(2, '\0')
        ? std::optional<int>(0)
        : atk::decodePair(u8(raw, 0), u8(raw, 1));
    const std::optional<int> brightnessRaw = raw.mid(2, 2) == QByteArray(2, '\0')
        ? std::optional<int>(0)
        : atk::decodePair(u8(raw, 2), u8(raw, 3));
    const std::optional<int> speedRaw = atk::decodePair(u8(raw, 4), u8(raw, 5));
    const std::optional<int> enabled = raw.mid(6, 2) == QByteArray(2, '\0')
        ? std::optional<int>(0)
        : atk::decodePair(u8(raw, 6), u8(raw, 7));
    if (!effect || !brightnessRaw || !speedRaw || !enabled) {
        return std::nullopt;
    }
    if (*effect < 0 || *effect > 2 || (*enabled != 0 && *enabled != 1)) {
        return std::nullopt;
    }
    LightingV1 lighting;
    lighting.mode = (*effect == 0 || *enabled == 0) ? 0 : (*effect == 2 ? 2 : 1);
    if (lighting.mode == 0) {
        lighting.brightness = 0;
        lighting.speed = 0;
    } else {
        lighting.brightness = *brightnessRaw <= 0x10 ? 0 : (*brightnessRaw <= 0x80 ? 1 : 2);
        lighting.speed = *speedRaw >= 5 ? 2 : (*speedRaw >= 3 ? 1 : 0);
    }
    return lighting;
}

QString colorFromV2(const QByteArray &raw, int offset)
{
    return QStringLiteral("#%1%2%3")
        .arg(static_cast<int>(u8(raw, offset + 6)), 2, 16, QLatin1Char('0'))
        .arg(static_cast<int>(u8(raw, offset + 5)), 2, 16, QLatin1Char('0'))
        .arg(static_cast<int>(u8(raw, offset + 4)), 2, 16, QLatin1Char('0'));
}

bool validColor(const QString &color)
{
    static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    return pattern.match(color.trimmed()).hasMatch();
}

} // namespace

AtkDevice::AtkDevice(const DeviceInfo &info, bool debug)
    : m_info(info)
    , m_debug(debug)
{
    m_hid.setDebug(debug);
    m_hid.setV2Wireless(info.receiver);
}

AtkDevice::~AtkDevice()
{
    close();
}

void AtkDevice::open()
{
    close();
    m_hid.setDebug(m_debug);
    m_hid.setV2Wireless(m_info.receiver);
    m_hid.open(m_info.path);
    m_open = true;
    try {
        readFull();
    } catch (...) {
        close();
        throw;
    }
}

void AtkDevice::close()
{
    m_hid.close();
    m_open = false;
}

void AtkDevice::setDebug(bool debug)
{
    m_debug = debug;
    m_hid.setDebug(debug);
}

void AtkDevice::setDpiFamilyOverride(const QString &family)
{
    if (family == QStringLiteral("step50") || family == QStringLiteral("ultra")) {
        m_dpiFamilyOverride = family;
    } else {
        m_dpiFamilyOverride.clear();
    }
}

DeviceState AtkDevice::readFull()
{
    if (m_info.protocol == QStringLiteral("v2")) {
        readFullV2();
    } else {
        readFullV1();
    }
    return m_state;
}

DeviceState AtkDevice::readLive()
{
    if (m_info.protocol == QStringLiteral("v2")) {
        readLiveV2();
    } else {
        readLiveV1();
    }
    return m_state;
}

QString AtkDevice::fallbackName() const
{
    QString name = m_info.productName.trimmed();
    if (name.isEmpty()) {
        return QStringLiteral("ATK/VXE Mouse");
    }
    const QStringList prefixes = {QStringLiteral("compx"), QStringLiteral("atk"), QStringLiteral("vxe")};
    for (const QString &prefix : prefixes) {
        if (name.startsWith(prefix, Qt::CaseInsensitive)) {
            const QString rest = name.mid(prefix.size()).trimmed();
            if (!rest.isEmpty()) {
                return rest;
            }
        }
    }
    return name;
}

void AtkDevice::require(bool condition, const QString &message) const
{
    if (!condition) {
        throw AtkError(message);
    }
}

void AtkDevice::readBatteryV1()
{
    try {
        const BatteryReading battery = m_hid.readBattery();
        if (battery.percent && battery.charging
            && atk::batteryIsPlausible(*battery.percent, *battery.charging)) {
            m_state.battery = *battery.percent;
            m_state.charging = *battery.charging;
        } else {
            m_state.battery.reset();
            m_state.charging.reset();
        }
        m_state.voltageMv = battery.millivolts;
    } catch (const AtkError &) {
    }
}

void AtkDevice::readFullV1()
{
    m_state = DeviceState {};
    m_state.vendorId = m_info.vendorId;
    m_state.productId = m_info.productId;
    m_state.usbName = m_info.productName;
    m_state.usbBcd = atk::formatBcd(m_info.bcdDevice);
    m_state.protocol = QStringLiteral("v1");
    m_state.viaReceiver = m_info.receiver;
    m_state.wired = !m_info.receiver;
    m_state.receiverName = m_info.receiver ? receiverName(m_info.productId, m_info.productName) : QString();
    m_state.receiverOnline = m_info.receiver;

    std::optional<int> cid;
    std::optional<int> mid;
    QString firmware;
    try {
        const auto identity = m_hid.readIdentity();
        cid = identity.first;
        mid = identity.second;
        firmware = m_hid.readFirmware(2);
    } catch (const AtkError &) {
    }

    m_model = modelFor(cid, mid, false);
    const bool known = !m_model.name.isEmpty();
    m_state.cid = cid;
    m_state.mid = mid;
    m_state.modelName = known ? m_model.name : fallbackName();
    m_state.brand = known ? m_model.brand
                          : (m_info.productName.contains(QStringLiteral("VXE"), Qt::CaseInsensitive)
                                 ? QStringLiteral("VXE")
                                 : QStringLiteral("ATK"));
    m_state.sensor = known ? m_model.sensor : QStringLiteral("Unknown sensor");
    m_state.firmware = firmware;

    const QByteArray system = m_hid.readEeprom(atk::AddrSystem, 10);
    if (system.size() < 10) {
        throw AtkError(QStringLiteral("The mouse returned a short system record"));
    }
    const int polling = atk::pollingFromCode(u8(system, 0)).value_or(1000);
    int stageCount = 1;
    int active = 0;
    if (const auto count = atk::decodePair(u8(system, 2), u8(system, 3))) {
        stageCount = std::clamp(*count, 1, 8);
    }
    if (const auto value = atk::decodePair(u8(system, 4), u8(system, 5))) {
        active = std::clamp(*value, 0, stageCount - 1);
    }

    m_caps = capabilitiesFor(known ? &m_model : nullptr, m_info.receiver, m_info.productId,
                             m_info.productName, polling, std::nullopt, std::nullopt, false,
                             m_dpiFamilyOverride);
    m_state.dpiFamily = m_caps.sensor.family;

    QVector<QPair<int, int>> stages;
    for (int index = 0; index < stageCount; ++index) {
        const QByteArray raw = m_hid.readEeprom(static_cast<uint16_t>(atk::AddrDpiBase + index * 4), 4);
        const auto stage = atk::unpackDpiStage(raw, m_caps.sensor.family);
        if (!stage) {
            throw AtkError(QStringLiteral("DPI stage %1 failed its checksum").arg(index + 1));
        }
        stages.append(*stage);
    }
    m_state.stages = stages;
    m_state.stageCount = stageCount;
    m_state.activeStage = active;
    m_state.dpi = stages.at(active).first;
    m_state.dpiY = stages.at(active).second;
    m_state.pollingRate = polling;
    m_state.pollingRates = m_caps.pollingRates;
    m_state.dpiOptions = m_caps.dpiOptions;
    m_state.debounceOptions = m_caps.debounceOptions;
    m_state.sleepOptions = m_caps.sleepOptions;
    m_state.lodChoices = m_caps.lodChoices;

    readBatteryV1();

    try {
        const QByteArray advanced = m_hid.readEeprom(atk::AddrAdvanced, 10);
        if (advanced.size() >= 10) {
            if (const auto debounce = atk::decodePair(u8(advanced, 0), u8(advanced, 1))) {
                m_state.debounceMs = *debounce;
                appendUniqueInt(m_state.debounceOptions, *debounce);
            }
            if (const auto motion = atk::decodePair(u8(advanced, 2), u8(advanced, 3));
                motion && (*motion == 0 || *motion == 1)) {
                m_state.motionSync = *motion != 0;
            }
            if (const auto units = atk::decodePair(u8(advanced, 4), u8(advanced, 5)); units && *units > 0) {
                m_state.sleepSeconds = *units * 10;
                appendUniqueInt(m_state.sleepOptions, *m_state.sleepSeconds);
            }
            if (const auto straight = atk::decodePair(u8(advanced, 6), u8(advanced, 7));
                straight && (*straight == 0 || *straight == 1)) {
                m_state.straightLine = *straight != 0;
            }
            if (const auto ripple = atk::decodePair(u8(advanced, 8), u8(advanced, 9));
                ripple && (*ripple == 0 || *ripple == 1)) {
                m_state.ripple = *ripple != 0;
            }
        }
    } catch (const AtkError &) {
    }

    try {
        const QByteArray raw = m_hid.readEeprom(atk::AddrLiftOff, 2);
        if (const auto code = atk::decodePair(u8(raw, 0), u8(raw, 1))) {
            m_state.lodCode = *code;
            m_state.lodMm = lodMm(*code);
            if (!std::any_of(m_state.lodChoices.begin(), m_state.lodChoices.end(),
                             [code](const QPair<int, double> &item) { return item.first == *code; })) {
                m_state.lodChoices.append({*code, m_state.lodMm.value_or(0.0)});
            }
        }
    } catch (const AtkError &) {
    }

    try {
        const QByteArray raw = m_hid.readEeprom(atk::AddrAngle, 4);
        const auto angle = atk::decodePair(u8(raw, 0), u8(raw, 1));
        const auto enabled = atk::decodePair(u8(raw, 2), u8(raw, 3));
        if (angle && enabled && (*enabled == 0 || *enabled == 1)) {
            const int signedAngle = *angle > 0x7F ? *angle - 0x100 : *angle;
            if (signedAngle >= -30 && signedAngle <= 30) {
                m_state.angleDegrees = signedAngle;
                m_state.angleEnabled = *enabled != 0;
                m_state.supportsAngle = true;
            }
        }
    } catch (const AtkError &) {
    }

    try {
        const QByteArray raw = m_hid.readEeprom(atk::AddrSensorMode, 6);
        if (raw.size() >= 6 && u8(raw, 2) == 0x06 && u8(raw, 3) == 0x4F) {
            if (const auto mode = atk::decodePair(u8(raw, 4), u8(raw, 5));
                mode && *mode >= 0 && *mode <= 3) {
                m_state.sensorMode = *mode;
                m_state.performanceMode = *mode != 0;
                m_state.supportsPerformanceMode = true;
            }
        }
    } catch (const AtkError &) {
    }

    try {
        const QByteArray raw = m_hid.readEeprom(atk::AddrDpiLight, 8);
        if (const auto lighting = decodeLightingV1(raw)) {
            m_state.ledMode = lighting->mode;
            m_state.ledBrightness = lighting->brightness;
            m_state.ledSpeed = lighting->speed;
            m_state.supportsLighting = true;
        }
    } catch (const AtkError &) {
    }

    try {
        QVector<QString> colors;
        for (int index = 0; index < stageCount; ++index) {
            const uint16_t groupAddress = static_cast<uint16_t>(atk::AddrStageColorBase + (index / 2) * 8);
            const QByteArray group = m_hid.readEeprom(groupAddress, 8);
            if (group.size() < 8) {
                break;
            }
            const QString color = atk::unpackRgb(group.mid((index % 2) * 4, 4));
            if (color.isEmpty()) {
                break;
            }
            colors.append(color);
        }
        if (colors.size() == stageCount) {
            m_state.stageColors = colors;
            m_state.supportsStageColors = true;
        }
    } catch (const AtkError &) {
    }

    try {
        if (const auto profile = m_hid.readProfile()) {
            m_state.profile = *profile + 1;
        }
    } catch (const AtkError &) {
    }

    readButtonsV1();

    if (m_info.receiver) {
        try {
            if (const auto receiver = m_hid.receiverOnline()) {
                m_state.receiverOnline = receiver->online;
                m_state.rfId = receiver->rfId;
            }
        } catch (const AtkError &) {
        }
    }

    m_state.supportsAngle = m_state.supportsAngle || m_caps.angle;
    m_state.supportsPerformanceMode = m_state.supportsPerformanceMode || m_caps.performanceMode;
    m_state.supportsLighting = m_state.supportsLighting || m_caps.dpiLighting;
    m_state.supportsStageColors = m_state.supportsStageColors || m_caps.stageColors;
    m_state.supportsButtonRemap = m_caps.buttonRemap
        || std::any_of(m_state.buttons.begin(), m_state.buttons.end(),
                       [](const ButtonInfo &button) { return button.readable; });
    m_state.supportsSensorMax = m_caps.sensorMax;
    m_state.supportsStageCount = true;
    m_state.supportsProfiles = true;
    m_state.support = known ? m_model.support : QStringLiteral("unknown");
    m_state.experimental = m_state.support != QStringLiteral("verified");
    m_state.readOnly = false;
}

void AtkDevice::readFullV2()
{
    m_state = DeviceState {};
    m_state.vendorId = m_info.vendorId;
    m_state.productId = m_info.productId;
    m_state.usbName = m_info.productName;
    m_state.usbBcd = atk::formatBcd(m_info.bcdDevice);
    m_state.protocol = QStringLiteral("v2");
    m_state.viaReceiver = m_info.receiver;
    m_state.wired = !m_info.receiver;
    m_state.receiverName = m_info.receiver ? receiverName(m_info.productId, m_info.productName) : QString();
    m_state.receiverOnline = m_info.receiver;

    std::optional<int> cid;
    std::optional<int> mid;
    QString firmware;
    try {
        const auto identity = m_hid.readIdentityV2();
        cid = identity.first;
        mid = identity.second;
        firmware = m_hid.readVersionV2();
    } catch (const AtkError &) {
    }
    m_model = modelFor(cid, mid, true);
    const bool known = !m_model.name.isEmpty();
    m_state.cid = cid;
    m_state.mid = mid;
    m_state.modelName = known ? m_model.name : fallbackName();
    m_state.brand = known ? m_model.brand
                          : (m_info.productName.contains(QStringLiteral("VXE"), Qt::CaseInsensitive)
                                 ? QStringLiteral("VXE")
                                 : QStringLiteral("ATK"));
    m_state.sensor = known ? m_model.sensor : QStringLiteral("Unknown sensor");
    m_state.firmware = firmware;
    if (m_info.receiver) {
        try {
            m_state.dongleFirmware = m_hid.readDongleVersionV2();
        } catch (const AtkError &) {
        }
    }

    bool answered = false;
    QByteArray config;
    try {
        config = m_hid.readConfigV2();
        applyConfigV2(config);
        answered = true;
    } catch (const AtkError &) {
    }

    m_caps = capabilitiesFor(known ? &m_model : nullptr, m_info.receiver, m_info.productId,
                             m_info.productName, m_state.pollingRate, m_state.debounceMs,
                             m_state.sleepSeconds, true, m_dpiFamilyOverride);
    m_state.dpiFamily = m_caps.sensor.family;
    m_state.pollingRates = m_caps.pollingRates;
    m_state.dpiOptions = m_caps.dpiOptions;
    m_state.debounceOptions = m_caps.debounceOptions;
    m_state.sleepOptions = m_caps.sleepOptions;
    m_state.lodChoices = m_caps.lodChoices;

    try {
        applyDpiV2(m_hid.readDpiV2());
        answered = true;
    } catch (const AtkError &) {
    }
    try {
        const int rawMode = m_hid.readSensorModeV2();
        m_state.sensorMode = v2CodeToSensorMode(rawMode);
        m_state.performanceMode = *m_state.sensorMode != 0;
        m_state.supportsPerformanceMode = true;
        answered = true;
    } catch (const AtkError &) {
    }
    try {
        const QByteArray raw = m_hid.readLightingV2();
        const int mode = v2CodeToMode(u8(raw, 0));
        m_state.ledMode = mode;
        if (mode == 0) {
            m_state.ledBrightness = 0;
            m_state.ledSpeed = 0;
        } else {
            static const QVector<int> brightnessValues = {85, 170, 255};
            static const QVector<int> speedValues = {10, 15, 20};
            m_state.ledBrightness = nearestIndex(brightnessValues, u8(raw, 1));
            m_state.ledSpeed = nearestIndex(speedValues, u8(raw, 2));
        }
        m_state.supportsLighting = true;
        answered = true;
    } catch (const AtkError &) {
    }

    readButtonsV2();
    readBatteryV2();
    if (m_info.receiver) {
        try {
            m_state.receiverOnline = m_hid.readOnlineV2();
        } catch (const AtkError &) {
        }
    }

    if (!answered) {
        throw AtkError(QStringLiteral("The 0xFF05 mouse did not answer"));
    }

    if (m_state.lodCode && !std::any_of(m_state.lodChoices.begin(), m_state.lodChoices.end(),
                                        [this](const QPair<int, double> &item) {
                                            return item.first == *m_state.lodCode;
                                        })) {
        m_state.lodChoices.append({*m_state.lodCode, m_state.lodMm.value_or(0.0)});
    }

    m_state.supportsAngle = false;
    m_state.supportsPerformanceMode = m_state.supportsPerformanceMode || m_caps.performanceMode;
    m_state.supportsLighting = m_state.supportsLighting || m_caps.dpiLighting;
    m_state.supportsStageColors = m_caps.stageColors;
    m_state.supportsButtonRemap = m_caps.buttonRemap
        || std::any_of(m_state.buttons.begin(), m_state.buttons.end(),
                       [](const ButtonInfo &button) { return button.readable; });
    m_state.supportsSensorMax = m_caps.sensorMax;
    m_state.supportsStageCount = false;
    m_state.supportsProfiles = true;
    m_state.support = known ? m_model.support : QStringLiteral("unknown");
    m_state.experimental = m_state.support != QStringLiteral("verified");
    m_state.readOnly = false;
}

void AtkDevice::readLiveV1()
{
    readBatteryV1();
    try {
        const QByteArray system = m_hid.readEeprom(atk::AddrSystem, 10);
        if (system.size() >= 10) {
            const int polling = atk::pollingFromCode(u8(system, 0)).value_or(m_state.pollingRate);
            m_state.pollingRate = polling;
            int stageCount = m_state.stageCount;
            int active = m_state.activeStage;
            if (const auto count = atk::decodePair(u8(system, 2), u8(system, 3))) {
                stageCount = std::clamp(*count, 1, 8);
            }
            if (const auto value = atk::decodePair(u8(system, 4), u8(system, 5))) {
                active = std::clamp(*value, 0, stageCount - 1);
            }
            if (stageCount != m_state.stageCount || active != m_state.activeStage) {
                QVector<QPair<int, int>> stages;
                bool ok = true;
                for (int index = 0; index < stageCount; ++index) {
                    try {
                        const QByteArray raw = m_hid.readEeprom(
                            static_cast<uint16_t>(atk::AddrDpiBase + index * 4), 4);
                        const auto stage = atk::unpackDpiStage(raw, m_caps.sensor.family);
                        if (!stage) {
                            ok = false;
                            break;
                        }
                        stages.append(*stage);
                    } catch (const AtkError &) {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    m_state.stages = stages;
                    m_state.stageCount = stageCount;
                    m_state.activeStage = active;
                    m_state.dpi = stages.at(active).first;
                    m_state.dpiY = stages.at(active).second;
                }
            }
        }
    } catch (const AtkError &) {
    }
    if (m_info.receiver) {
        try {
            if (const auto receiver = m_hid.receiverOnline()) {
                m_state.receiverOnline = receiver->online;
                m_state.rfId = receiver->rfId;
            }
        } catch (const AtkError &) {
        }
    }
}

void AtkDevice::readLiveV2()
{
    readBatteryV2();
    try {
        applyConfigV2(m_hid.readConfigV2());
    } catch (const AtkError &) {
    }
    try {
        const QByteArray active = m_hid.readMemoryV2(1, 1);
        if (!active.isEmpty() && u8(active, 0) != m_state.activeStage) {
            reloadDpiV2();
        }
    } catch (const AtkError &) {
    }
    if (m_info.receiver) {
        try {
            m_state.receiverOnline = m_hid.readOnlineV2();
        } catch (const AtkError &) {
        }
    }
}

void AtkDevice::readBatteryV2()
{
    try {
        const BatteryReading battery = m_hid.readBatteryV2();
        if (battery.percent && battery.charging
            && atk::batteryIsPlausible(*battery.percent, *battery.charging)) {
            m_state.battery = *battery.percent;
            m_state.charging = *battery.charging;
        } else {
            m_state.battery.reset();
            m_state.charging.reset();
        }
        m_state.voltageMv = battery.millivolts;
    } catch (const AtkError &) {
    }
}

void AtkDevice::applyConfigV2(const QByteArray &config)
{
    if (config.size() < 12) {
        return;
    }
    m_state.profile = u8(config, 0) + 1;
    m_state.pollingRate = v2CodeToRate(u8(config, 2));
    m_state.lodCode = u8(config, 5) + 1;
    m_state.lodMm = lodMm(*m_state.lodCode);
    m_state.motionSync = u8(config, 6) != 0;
    m_state.angleEnabled = u8(config, 7) != 0;
    m_state.straightLine = u8(config, 7) != 0;
    m_state.ripple = u8(config, 8) != 0;
    m_state.sleepSeconds = u8(config, 9) | (u8(config, 10) << 8);
    m_state.debounceMs = u8(config, 11);
    appendUniqueInt(m_state.debounceOptions, *m_state.debounceMs);
    if (*m_state.sleepSeconds > 0) {
        appendUniqueInt(m_state.sleepOptions, *m_state.sleepSeconds);
    }
}

void AtkDevice::applyDpiV2(const QByteArray &raw)
{
    if (raw.size() < 2) {
        return;
    }
    const int declaredCount = std::clamp(static_cast<int>(u8(raw, 1)), 1, 8);
    QVector<QPair<int, int>> stages;
    QVector<QString> colors;
    for (int index = 0; index < declaredCount; ++index) {
        const int offset = 2 + index * 8;
        if (offset + 7 >= raw.size()) {
            break;
        }
        const int x = u8(raw, offset) | (u8(raw, offset + 1) << 8);
        const int y = u8(raw, offset + 2) | (u8(raw, offset + 3) << 8);
        stages.append({x, y});
        colors.append(colorFromV2(raw, offset));
    }
    if (stages.isEmpty()) {
        return;
    }
    int active = std::min(static_cast<int>(u8(raw, 0)), static_cast<int>(stages.size()) - 1);
    while (stages.size() > active + 1 && stages.last() == qMakePair(0, 0)) {
        stages.removeLast();
        colors.removeLast();
    }
    const int count = stages.size();
    active = std::min(active, count - 1);
    m_state.stages = stages;
    m_state.stageColors = colors;
    m_state.stageCount = count;
    m_state.activeStage = active;
    m_state.dpi = stages.at(active).first;
    m_state.dpiY = stages.at(active).second;
}

void AtkDevice::reloadDpiV2()
{
    applyDpiV2(m_hid.readDpiV2());
}

void AtkDevice::readButtonsV1()
{
    QVector<ButtonInfo> buttons;
    for (int group = 0; group < 3; ++group) {
        QByteArray raw;
        try {
            raw = m_hid.readEeprom(static_cast<uint16_t>(0x0060 + group * 8), 8);
        } catch (const AtkError &) {
            raw.clear();
        }
        for (int slot = 0; slot < 2; ++slot) {
            const int index = group * 2 + slot;
            if (index >= 6) {
                continue;
            }
            if (index == 5 && !m_caps.bottomButton) {
                continue;
            }
            ButtonInfo button;
            button.name = QString::fromLatin1(v1ButtonNames[index]);
            if (raw.size() < 8) {
                button.action = QStringLiteral("Unreadable");
                button.readable = false;
                buttons.append(button);
                continue;
            }
            const QByteArray record = raw.mid(slot * 4, 4);
            const int keyClass = u8(record, 0);
            const int value1 = u8(record, 1);
            const int value2 = u8(record, 2);
            const int check = u8(record, 3);
            if (((keyClass + value1 + value2 + check) & 0xFF) != 0x55) {
                button.action = QStringLiteral("Unreadable");
                button.raw = record;
                button.readable = false;
                buttons.append(button);
                continue;
            }
            button.raw = record;
            button.action = v1ActionLabel(keyClass, value1, value2);
            if (const ButtonAction *action = v1ActionForRecord(keyClass, value1, value2)) {
                button.actionId = QString::fromLatin1(action->id);
            }
            button.readable = true;
            buttons.append(button);
        }
    }
    m_state.buttons = buttons;
}

void AtkDevice::readButtonsV2()
{
    QByteArray raw;
    try {
        raw = m_hid.readButtonsV2();
    } catch (const AtkError &) {
        return;
    }
    QVector<ButtonInfo> buttons;
    for (int index = 0; index < 6; ++index) {
        const int offset = index * 5;
        if (offset + 5 > raw.size()) {
            break;
        }
        const int a = u8(raw, offset);
        const int b = u8(raw, offset + 1);
        const int c = u8(raw, offset + 2);
        const int d = u8(raw, offset + 3);
        ButtonInfo button;
        button.name = QString::fromLatin1(v1ButtonNames[index]);
        button.raw = raw.mid(offset, 5);
        const bool allZero = a == 0 && b == 0 && c == 0 && d == 0;
        if (allZero && m_model.normalizesDefaultButtons) {
            button.actionId = v2DefaultButtonId(index);
            if (const V2Action *action = v2ActionForId(button.actionId)) {
                button.action = QString::fromLatin1(action->label);
            } else {
                button.action = QStringLiteral("Factory default");
            }
            button.readable = true;
        } else if (const V2Action *action = v2ActionForRecord(a, b, c, d)) {
            button.actionId = QString::fromLatin1(action->id);
            button.action = QString::fromLatin1(action->label);
            button.readable = true;
        } else {
            button.action = allZero ? QStringLiteral("Unreadable") : QStringLiteral("Custom / unsupported");
            button.readable = !allZero;
        }
        buttons.append(button);
    }
    m_state.buttons = buttons;
}

void AtkDevice::setPolling(int hertz)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(containsInt(m_state.pollingRates, hertz),
            QStringLiteral("%1 Hz is not available on this link").arg(hertz));
    if (m_info.protocol == QStringLiteral("v2")) {
        const int code = v2RateToCode(hertz);
        require(code >= 0, QStringLiteral("Unknown polling rate"));
        m_hid.setRateV2(code);
        const QByteArray config = m_hid.readConfigV2();
        const int actual = v2CodeToRate(u8(config, 2));
        require(actual == hertz, QStringLiteral("The mouse kept %1 Hz").arg(actual));
        m_state.pollingRate = actual;
        return;
    }
    const auto code = atk::pollingToCode(hertz);
    require(code.has_value(), QStringLiteral("Unknown polling rate"));
    m_hid.writeEeprom(atk::AddrSystem, atk::packPair(*code));
    QThread::msleep(120);
    const QByteArray system = m_hid.readEeprom(atk::AddrSystem, 10);
    const int actual = atk::pollingFromCode(u8(system, 0)).value_or(-1);
    require(actual == hertz, QStringLiteral("The mouse kept %1 Hz").arg(actual));
    m_state.pollingRate = actual;
}

void AtkDevice::setActiveStage(int index)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(index >= 0 && index < m_state.stageCount, QStringLiteral("DPI stage out of range"));
    if (m_info.protocol == QStringLiteral("v2")) {
        require(index < m_state.stages.size(), QStringLiteral("DPI stage out of range"));
        const auto stage = m_state.stages.at(index);
        const QString color = index < m_state.stageColors.size() ? m_state.stageColors.at(index)
                                                                 : QStringLiteral("#ffffff");
        m_hid.setDpiV2(index, stage.first, stage.second, color, true);
        reloadDpiV2();
        require(m_state.activeStage == index, QStringLiteral("The mouse did not switch DPI stage"));
        return;
    }
    m_hid.writeEeprom(atk::AddrSystem + 4, atk::packPair(index));
    QThread::msleep(50);
    const QByteArray system = m_hid.readEeprom(atk::AddrSystem, 10);
    const auto actual = atk::decodePair(u8(system, 4), u8(system, 5));
    require(actual && *actual == index, QStringLiteral("The mouse did not switch DPI stage"));
    m_state.activeStage = index;
    m_state.dpi = m_state.stages.at(index).first;
    m_state.dpiY = m_state.stages.at(index).second;
}

void AtkDevice::setStageDpi(int index, int x, int y)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(index >= 0 && index < m_state.stageCount, QStringLiteral("DPI stage out of range"));
    require(containsInt(m_state.dpiOptions, x) && containsInt(m_state.dpiOptions, y),
            QStringLiteral("%1 x %2 DPI is not supported by %3")
                .arg(x)
                .arg(y)
                .arg(m_caps.sensor.name));
    if (m_info.protocol == QStringLiteral("v2")) {
        const QString color = index < m_state.stageColors.size() ? m_state.stageColors.at(index)
                                                                 : QStringLiteral("#ffffff");
        m_hid.setDpiV2(index, x, y, color, index == m_state.activeStage);
        reloadDpiV2();
        require(m_state.stages.at(index) == qMakePair(x, y),
                QStringLiteral("The mouse did not retain the DPI settings"));
        return;
    }
    const QString family = m_caps.sensor.family;
    const QByteArray packed = atk::packDpiStage(x, y, family);
    const uint16_t address = static_cast<uint16_t>(atk::AddrDpiBase + index * 4);
    m_hid.writeEeprom(address, packed);
    QThread::msleep(30);
    const auto confirmed = atk::unpackDpiStage(m_hid.readEeprom(address, 4), family);
    require(confirmed.has_value(), QStringLiteral("The mouse returned an invalid DPI stage"));
    require(*confirmed == qMakePair(x, y),
            QStringLiteral("The mouse kept %1 x %2 DPI").arg(confirmed->first).arg(confirmed->second));
    m_state.stages[index] = *confirmed;
    if (index == m_state.activeStage) {
        m_state.dpi = confirmed->first;
        m_state.dpiY = confirmed->second;
    }
}

void AtkDevice::setStageColor(int index, const QString &color)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_state.supportsStageColors, QStringLiteral("This mouse does not expose stage colours"));
    require(index >= 0 && index < m_state.stageCount, QStringLiteral("DPI stage out of range"));
    const QString normalized = color.trimmed().toLower();
    require(validColor(normalized), QStringLiteral("Colour must be #rrggbb"));
    if (m_info.protocol == QStringLiteral("v2")) {
        require(index < m_state.stages.size(), QStringLiteral("DPI stage out of range"));
        const auto stage = m_state.stages.at(index);
        m_hid.setDpiV2(index, stage.first, stage.second, normalized, index == m_state.activeStage);
        reloadDpiV2();
        require(index < m_state.stageColors.size()
                    && m_state.stageColors.at(index).compare(normalized, Qt::CaseInsensitive) == 0,
                QStringLiteral("The mouse did not retain the stage colour"));
        return;
    }
    const QByteArray packed = atk::packRgb(normalized);
    require(!packed.isEmpty(), QStringLiteral("Colour must be #rrggbb"));
    const uint16_t groupAddress = static_cast<uint16_t>(atk::AddrStageColorBase + (index / 2) * 8);
    QByteArray group = m_hid.readEeprom(groupAddress, 8);
    require(group.size() == 8, QStringLiteral("Could not read the stage colour block"));
    const int offset = (index % 2) * 4;
    group.replace(offset, 4, packed);
    m_hid.writeEeprom(groupAddress, group);
    QThread::msleep(30);
    const QByteArray confirmedGroup = m_hid.readEeprom(groupAddress, 8);
    const QString confirmed = atk::unpackRgb(confirmedGroup.mid(offset, 4));
    require(confirmed.compare(normalized, Qt::CaseInsensitive) == 0,
            QStringLiteral("The mouse did not retain the stage colour"));
    while (m_state.stageColors.size() < m_state.stageCount) {
        m_state.stageColors.append(QStringLiteral("#ffffff"));
    }
    m_state.stageColors[index] = confirmed;
}

void AtkDevice::addStage()
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_info.protocol != QStringLiteral("v2"),
            QStringLiteral("This 0xFF05 firmware does not expose a DPI stage-count command"));
    require(m_state.supportsStageCount, QStringLiteral("This mouse does not expose a DPI stage count"));
    require(m_state.stageCount < 8, QStringLiteral("This mouse already has eight DPI stages"));
    require(!m_state.stages.isEmpty(), QStringLiteral("DPI stages are not readable"));
    const int newIndex = m_state.stageCount;
    const auto last = m_state.stages.last();
    const QString family = m_caps.sensor.family;
    const QByteArray packed = atk::packDpiStage(last.first, last.second, family);
    const uint16_t address = static_cast<uint16_t>(atk::AddrDpiBase + newIndex * 4);
    m_hid.writeEeprom(address, packed);
    QThread::msleep(30);
    const auto confirmed = atk::unpackDpiStage(m_hid.readEeprom(address, 4), family);
    require(confirmed && *confirmed == last, QStringLiteral("The mouse did not accept the new DPI stage"));

    if (m_state.supportsStageColors) {
        const QString color = m_state.stageColors.isEmpty() ? QStringLiteral("#ffffff")
                                                            : m_state.stageColors.last();
        const QByteArray packedColor = atk::packRgb(color);
        const uint16_t groupAddress = static_cast<uint16_t>(atk::AddrStageColorBase + (newIndex / 2) * 8);
        QByteArray group = m_hid.readEeprom(groupAddress, 8);
        if (!packedColor.isEmpty() && group.size() == 8) {
            const int offset = (newIndex % 2) * 4;
            group.replace(offset, 4, packedColor);
            m_hid.writeEeprom(groupAddress, group);
            QThread::msleep(30);
            const QByteArray confirmedGroup = m_hid.readEeprom(groupAddress, 8);
            require(confirmedGroup.mid(offset, 4) == packedColor,
                    QStringLiteral("The mouse did not retain the new stage colour"));
        }
    }

    QByteArray system = m_hid.readEeprom(atk::AddrSystem, 10);
    require(system.size() >= 6, QStringLiteral("Could not read the system record"));
    system.replace(2, 2, atk::packPair(newIndex + 1));
    m_hid.writeEeprom(atk::AddrSystem, system.left(6));
    QThread::msleep(50);
    const QByteArray after = m_hid.readEeprom(atk::AddrSystem, 10);
    require(atk::decodePair(u8(after, 2), u8(after, 3)) == std::optional<int>(newIndex + 1),
            QStringLiteral("The mouse did not retain the new DPI stage count"));
    m_state.stages.append(*confirmed);
    if (m_state.supportsStageColors) {
        m_state.stageColors.append(m_state.stageColors.isEmpty() ? QStringLiteral("#ffffff")
                                                                 : m_state.stageColors.last());
    }
    m_state.stageCount = newIndex + 1;
}

void AtkDevice::removeLastStage()
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_info.protocol != QStringLiteral("v2"),
            QStringLiteral("This 0xFF05 firmware does not expose a DPI stage-count command"));
    require(m_state.stageCount > 1, QStringLiteral("At least one DPI stage is required"));
    QByteArray system = m_hid.readEeprom(atk::AddrSystem, 10);
    require(system.size() >= 6, QStringLiteral("Could not read the system record"));
    const int newCount = m_state.stageCount - 1;
    const int active = std::min(m_state.activeStage, newCount - 1);
    system.replace(2, 2, atk::packPair(newCount));
    system.replace(4, 2, atk::packPair(active));
    m_hid.writeEeprom(atk::AddrSystem, system.left(6));
    QThread::msleep(50);
    const QByteArray after = m_hid.readEeprom(atk::AddrSystem, 10);
    require(atk::decodePair(u8(after, 2), u8(after, 3)) == std::optional<int>(newCount),
            QStringLiteral("The mouse did not retain the new DPI stage count"));
    m_state.stages = m_state.stages.mid(0, newCount);
    m_state.stageColors = m_state.stageColors.mid(0, newCount);
    m_state.stageCount = newCount;
    m_state.activeStage = active;
    m_state.dpi = m_state.stages.at(active).first;
    m_state.dpiY = m_state.stages.at(active).second;
}

void AtkDevice::setLiftOffCode(int code)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    const bool allowed = std::any_of(m_state.lodChoices.begin(), m_state.lodChoices.end(),
                                     [code](const QPair<int, double> &item) { return item.first == code; });
    require(allowed, QStringLiteral("That lift-off value is not available"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setLodV2(code);
        const QByteArray config = m_hid.readConfigV2();
        const int actual = u8(config, 5) + 1;
        require(actual == code, QStringLiteral("The mouse did not retain the lift-off setting"));
        m_state.lodCode = actual;
        m_state.lodMm = lodMm(actual);
        return;
    }
    m_hid.writeEeprom(atk::AddrLiftOff, atk::packPair(code));
    QThread::msleep(30);
    const QByteArray raw = m_hid.readEeprom(atk::AddrLiftOff, 2);
    require(atk::decodePair(u8(raw, 0), u8(raw, 1)) == std::optional<int>(code),
            QStringLiteral("The mouse did not retain the lift-off setting"));
    m_state.lodCode = code;
    m_state.lodMm = lodMm(code);
}

void AtkDevice::setDebounce(int milliseconds)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(containsInt(m_state.debounceOptions, milliseconds), QStringLiteral("Unsupported debounce value"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setDebounceV2(milliseconds);
        const QByteArray config = m_hid.readConfigV2();
        const int actual = u8(config, 11);
        require(actual == milliseconds, QStringLiteral("The mouse did not retain the debounce setting"));
        m_state.debounceMs = actual;
        return;
    }
    QByteArray block = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(block.size() == 10, QStringLiteral("Could not read the advanced settings"));
    block.replace(0, 2, atk::packPair(milliseconds));
    m_hid.writeEeprom(atk::AddrAdvanced, block);
    QThread::msleep(40);
    const QByteArray confirmed = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(atk::decodePair(u8(confirmed, 0), u8(confirmed, 1)) == std::optional<int>(milliseconds),
            QStringLiteral("The mouse did not retain the debounce setting"));
    m_state.debounceMs = milliseconds;
}

void AtkDevice::setMotionSync(bool enabled)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setMotionSyncV2(enabled);
        const QByteArray config = m_hid.readConfigV2();
        const bool actual = u8(config, 6) != 0;
        require(actual == enabled, QStringLiteral("The mouse did not retain Motion Sync"));
        m_state.motionSync = actual;
        return;
    }
    QByteArray block = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(block.size() == 10, QStringLiteral("Could not read the advanced settings"));
    block.replace(2, 2, atk::packPair(enabled ? 1 : 0));
    m_hid.writeEeprom(atk::AddrAdvanced, block);
    QThread::msleep(40);
    const QByteArray confirmed = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(atk::decodePair(u8(confirmed, 2), u8(confirmed, 3)) == std::optional<int>(enabled ? 1 : 0),
            QStringLiteral("The mouse did not retain Motion Sync"));
    m_state.motionSync = enabled;
}

void AtkDevice::setSleep(int seconds)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(containsInt(m_state.sleepOptions, seconds), QStringLiteral("Unsupported sleep timer"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setSleepV2(seconds);
        const QByteArray config = m_hid.readConfigV2();
        const int actual = u8(config, 9) | (u8(config, 10) << 8);
        require(actual == seconds, QStringLiteral("The mouse did not retain the sleep timer"));
        m_state.sleepSeconds = actual;
        return;
    }
    const int units = static_cast<int>(std::lround(seconds / 10.0));
    QByteArray block = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(block.size() == 10, QStringLiteral("Could not read the advanced settings"));
    block.replace(4, 2, atk::packPair(units));
    m_hid.writeEeprom(atk::AddrAdvanced, block);
    QThread::msleep(40);
    const QByteArray confirmed = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(atk::decodePair(u8(confirmed, 4), u8(confirmed, 5)) == std::optional<int>(units),
            QStringLiteral("The mouse did not retain the sleep timer"));
    m_state.sleepSeconds = seconds;
}

void AtkDevice::setStraightLine(bool enabled)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setAngleSnapV2(enabled);
        const QByteArray config = m_hid.readConfigV2();
        const bool actual = u8(config, 7) != 0;
        require(actual == enabled, QStringLiteral("The mouse did not retain angle snap"));
        m_state.straightLine = actual;
        m_state.angleEnabled = actual;
        return;
    }
    QByteArray block = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(block.size() == 10, QStringLiteral("Could not read the advanced settings"));
    block.replace(6, 2, atk::packPair(enabled ? 1 : 0));
    m_hid.writeEeprom(atk::AddrAdvanced, block);
    QThread::msleep(40);
    const QByteArray confirmed = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(atk::decodePair(u8(confirmed, 6), u8(confirmed, 7)) == std::optional<int>(enabled ? 1 : 0),
            QStringLiteral("The mouse did not retain straight-line correction"));
    m_state.straightLine = enabled;
}

void AtkDevice::setRipple(bool enabled)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setRippleV2(enabled);
        const QByteArray config = m_hid.readConfigV2();
        const bool actual = u8(config, 8) != 0;
        require(actual == enabled, QStringLiteral("The mouse did not retain ripple control"));
        m_state.ripple = actual;
        return;
    }
    QByteArray block = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(block.size() == 10, QStringLiteral("Could not read the advanced settings"));
    block.replace(8, 2, atk::packPair(enabled ? 1 : 0));
    m_hid.writeEeprom(atk::AddrAdvanced, block);
    QThread::msleep(40);
    const QByteArray confirmed = m_hid.readEeprom(atk::AddrAdvanced, 10);
    require(atk::decodePair(u8(confirmed, 8), u8(confirmed, 9)) == std::optional<int>(enabled ? 1 : 0),
            QStringLiteral("The mouse did not retain ripple control"));
    m_state.ripple = enabled;
}

void AtkDevice::setAngle(int degrees, bool enabled)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_info.protocol != QStringLiteral("v2"),
            QStringLiteral("Sensor rotation is not exposed by the 0xFF05 protocol"));
    require(m_state.supportsAngle, QStringLiteral("This mouse does not expose sensor rotation"));
    require(degrees >= -30 && degrees <= 30, QStringLiteral("Sensor angle must be between -30 and 30"));
    const QByteArray payload = atk::packPair(degrees & 0xFF) + atk::packPair(enabled ? 1 : 0);
    m_hid.writeEeprom(atk::AddrAngle, payload);
    QThread::msleep(30);
    const QByteArray raw = m_hid.readEeprom(atk::AddrAngle, 4);
    const auto angle = atk::decodePair(u8(raw, 0), u8(raw, 1));
    const auto switchValue = atk::decodePair(u8(raw, 2), u8(raw, 3));
    require(angle && switchValue, QStringLiteral("The mouse did not retain the sensor rotation"));
    const int signedAngle = *angle > 0x7F ? *angle - 0x100 : *angle;
    require(signedAngle == degrees && (*switchValue != 0) == enabled,
            QStringLiteral("The mouse did not retain the sensor rotation"));
    m_state.angleDegrees = degrees;
    m_state.angleEnabled = enabled;
}

void AtkDevice::setSensorMode(int mode)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_state.supportsPerformanceMode, QStringLiteral("Sensor sampling mode is not exposed"));
    require(mode >= 0 && mode <= 2, QStringLiteral("Sensor mode must be Basic, Competitive or Competitive MAX"));
    require(mode < 2 || m_state.supportsSensorMax,
            QStringLiteral("Competitive MAX has not been verified for this model"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setSensorModeV2(v2SensorModeToCode(mode));
        const int actual = v2CodeToSensorMode(m_hid.readSensorModeV2());
        require(actual == mode, QStringLiteral("The mouse did not retain the sensor sampling mode"));
        m_state.sensorMode = actual;
        m_state.performanceMode = actual != 0;
        return;
    }
    QByteArray row = m_hid.readEeprom(atk::AddrSensorMode, 6);
    require(row.size() == 6, QStringLiteral("Could not read the sensor mode record"));
    row.replace(4, 2, atk::packPair(mode));
    m_hid.writeEeprom(atk::AddrSensorMode, row);
    QThread::msleep(40);
    const QByteArray confirmed = m_hid.readEeprom(atk::AddrSensorMode, 6);
    require(u8(confirmed, 2) == 0x06 && u8(confirmed, 3) == 0x4F,
            QStringLiteral("The mouse did not retain the sensor sampling mode"));
    require(atk::decodePair(u8(confirmed, 4), u8(confirmed, 5)) == std::optional<int>(mode),
            QStringLiteral("The mouse did not retain the sensor sampling mode"));
    m_state.sensorMode = mode;
    m_state.performanceMode = mode != 0;
}

void AtkDevice::setButtonAction(int index, const QString &actionId)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_state.supportsButtonRemap, QStringLiteral("Button remapping has not been verified for this model"));
    require(index >= 0 && index < 6, QStringLiteral("Button index out of range"));
    if (index == 5) {
        require(m_caps.bottomButton, QStringLiteral("This mouse has no bottom button"));
    }
    require(index < m_state.buttons.size(), QStringLiteral("Button assignments are not readable"));
    require(m_state.buttons.at(index).readable,
            QStringLiteral("The %1 button record is unreadable").arg(m_state.buttons.at(index).name));

    if (m_info.protocol == QStringLiteral("v2")) {
        const V2Action *action = v2ActionForId(actionId);
        require(action != nullptr, QStringLiteral("Unsupported button action"));
        QVector<int> values = {action->a, action->b, action->c, action->d};
        m_hid.setButtonV2(index, values);
        readButtonsV2();
        require(index < m_state.buttons.size() && m_state.buttons.at(index).actionId == actionId,
                QStringLiteral("The mouse did not retain the button assignment"));
        return;
    }

    const ButtonAction *action = v1ActionForId(actionId);
    require(action != nullptr, QStringLiteral("Unsupported button action"));
    const QByteArray packed = packButtonRecord(action->keyClass, action->value1, action->value2);
    const int address = buttonAddress(index);
    m_hid.writeEeprom(static_cast<uint16_t>(address), packed);
    QThread::msleep(40);
    const QByteArray confirmed = m_hid.readEeprom(static_cast<uint16_t>(address), 4);
    require(confirmed == packed, QStringLiteral("The mouse did not retain the button assignment"));
    if (index < m_state.buttons.size()) {
        ButtonInfo &button = m_state.buttons[index];
        button.action = QString::fromLatin1(action->label);
        button.actionId = actionId;
        button.raw = packed;
        button.readable = true;
    }
}

void AtkDevice::resetButtons()
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_state.supportsButtonRemap, QStringLiteral("Button remapping has not been verified for this model"));
    require(!m_state.buttons.isEmpty(), QStringLiteral("Button assignments are not readable"));

    if (m_info.protocol == QStringLiteral("v2")) {
        QVector<QString> snapshot;
        for (const ButtonInfo &button : m_state.buttons) {
            snapshot.append(button.actionId);
        }
        int applied = 0;
        try {
            for (int index = 0; index < m_state.buttons.size() && index < 6; ++index) {
                const QString id = v2DefaultButtonId(index);
                const V2Action *action = v2ActionForId(id);
                if (!action) {
                    continue;
                }
                m_hid.setButtonV2(index, {action->a, action->b, action->c, action->d});
                ++applied;
            }
            readButtonsV2();
            for (int index = 0; index < m_state.buttons.size() && index < 6; ++index) {
                require(m_state.buttons.at(index).actionId == v2DefaultButtonId(index),
                        QStringLiteral("The mouse did not retain the default for button %1").arg(index + 1));
            }
        } catch (const AtkError &error) {
            for (int index = 0; index < snapshot.size() && index < 6; ++index) {
                const V2Action *action = v2ActionForId(snapshot.at(index));
                if (action) {
                    try {
                        m_hid.setButtonV2(index, {action->a, action->b, action->c, action->d});
                    } catch (const AtkError &) {
                        break;
                    }
                }
            }
            readButtonsV2();
            throw AtkError(QStringLiteral("Button reset stopped after %1 change(s); review the assignments. (%2)")
                               .arg(applied)
                               .arg(QString::fromUtf8(error.what())));
        }
        return;
    }

    QVector<QPair<uint16_t, QByteArray>> snapshot;
    for (int index = 0; index < m_state.buttons.size() && index < 6; ++index) {
        const uint16_t address = static_cast<uint16_t>(buttonAddress(index));
        snapshot.append({address, m_hid.readEeprom(address, 4)});
    }
    int applied = 0;
    try {
        for (int index = 0; index < m_state.buttons.size() && index < 6; ++index) {
            const ButtonAction *action = v1ActionForId(QString::fromLatin1(v1DefaultActions[index]));
            if (!action) {
                continue;
            }
            const QByteArray packed = packButtonRecord(action->keyClass, action->value1, action->value2);
            const uint16_t address = static_cast<uint16_t>(buttonAddress(index));
            m_hid.writeEeprom(address, packed);
            ++applied;
            QThread::msleep(30);
            require(m_hid.readEeprom(address, 4) == packed,
                    QStringLiteral("The mouse did not retain the default for button %1").arg(index + 1));
        }
    } catch (const AtkError &error) {
        for (const auto &item : snapshot) {
            try {
                m_hid.writeEeprom(item.first, item.second);
                QThread::msleep(20);
            } catch (const AtkError &) {
            }
        }
        readButtonsV1();
        throw AtkError(QStringLiteral("Button reset stopped after %1 change(s); review the assignments. (%2)")
                           .arg(applied)
                           .arg(QString::fromUtf8(error.what())));
    }
    readButtonsV1();
}

void AtkDevice::setLedMode(int mode, int brightness, int speed)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_state.supportsLighting, QStringLiteral("This mouse does not expose DPI lighting"));
    require(mode >= 0 && mode <= 2, QStringLiteral("LED mode must be off, steady or breathing"));
    require(brightness >= 0 && brightness <= 2, QStringLiteral("Invalid brightness"));
    require(speed >= 0 && speed <= 2, QStringLiteral("Invalid LED speed"));

    if (m_info.protocol == QStringLiteral("v2")) {
        const int rawMode = v2ModeToCode(mode);
        const int rawBrightness = mode == 0 ? 0 : (brightness == 0 ? 85 : (brightness == 1 ? 170 : 255));
        const int rawSpeed = mode == 0 ? 0 : (speed == 0 ? 10 : (speed == 1 ? 15 : 20));
        m_hid.setLightingV2(rawMode, rawBrightness, rawSpeed);
        const QByteArray raw = m_hid.readLightingV2();
        const int actualMode = v2CodeToMode(u8(raw, 0));
        require(actualMode == mode, QStringLiteral("The mouse did not retain the lighting mode"));
        m_state.ledMode = actualMode;
        if (actualMode == 0) {
            m_state.ledBrightness = 0;
            m_state.ledSpeed = 0;
        } else {
            static const QVector<int> brightnessValues = {85, 170, 255};
            static const QVector<int> speedValues = {10, 15, 20};
            m_state.ledBrightness = nearestIndex(brightnessValues, u8(raw, 1));
            m_state.ledSpeed = nearestIndex(speedValues, u8(raw, 2));
        }
        return;
    }

    QByteArray block = m_hid.readEeprom(atk::AddrDpiLight, 8);
    require(block.size() == 8, QStringLiteral("Could not read the lighting block"));
    const int effect = mode == 0 ? 0 : (mode == 2 ? 2 : 1);
    const int brightnessCode = mode == 0 ? 0 : (brightness == 0 ? 0x10 : (brightness == 1 ? 0x80 : 0xFF));
    const int speedCode = speed == 0 ? 1 : (speed == 1 ? 3 : 5);
    block.replace(0, 2, atk::packPair(effect));
    block.replace(2, 2, atk::packPair(brightnessCode));
    block.replace(4, 2, atk::packPair(speedCode));
    block.replace(6, 2, atk::packPair(mode == 0 ? 0 : 1));
    m_hid.writeEeprom(atk::AddrDpiLight, block);
    QThread::msleep(40);
    const auto confirmed = decodeLightingV1(m_hid.readEeprom(atk::AddrDpiLight, 8));
    require(confirmed.has_value(), QStringLiteral("The mouse did not retain the lighting settings"));
    require(confirmed->mode == mode && confirmed->speed == speed,
            QStringLiteral("The mouse did not retain the lighting settings"));
    m_state.ledMode = confirmed->mode;
    m_state.ledBrightness = confirmed->brightness;
    m_state.ledSpeed = confirmed->speed;
}

void AtkDevice::setProfile(int profile)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(profile >= 1 && profile <= 4, QStringLiteral("Profile must be between 1 and 4"));
    if (m_info.protocol == QStringLiteral("v2")) {
        m_hid.setProfileV2(profile);
        const QByteArray config = m_hid.readConfigV2();
        const int actual = u8(config, 0) + 1;
        require(actual == profile, QStringLiteral("The mouse kept profile %1").arg(actual));
        m_state.profile = actual;
        return;
    }
    m_hid.setProfile(profile - 1);
    QThread::msleep(80);
    const auto confirmed = m_hid.readProfile();
    require(confirmed && *confirmed + 1 == profile, QStringLiteral("The mouse did not switch profile"));
    m_state.profile = profile;
}

void AtkDevice::commitSettings()
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    if (m_info.protocol == QStringLiteral("v2")) {
        return;
    }
    const auto current = m_hid.readProfile();
    require(current.has_value(), QStringLiteral("The mouse did not report its active profile"));
    const int currentProfile = *current;
    const int other = currentProfile == 0 ? 1 : 0;
    bool switched = false;
    try {
        m_hid.setProfile(other);
        switched = true;
        QThread::msleep(180);
        m_hid.setProfile(currentProfile);
        switched = false;
        QThread::msleep(220);
        const auto confirmed = m_hid.readProfile();
        require(confirmed && *confirmed == currentProfile,
                QStringLiteral("The mouse did not return to its profile after saving"));
    } catch (...) {
        if (switched) {
            try {
                m_hid.setProfile(currentProfile);
            } catch (const AtkError &) {
            }
        }
        throw;
    }
    m_state.profile = currentProfile + 1;
}

QByteArray AtkDevice::rawRead(uint16_t address, int length)
{
    if (m_info.protocol == QStringLiteral("v2")) {
        return m_hid.readMemoryV2(address, length);
    }
    return m_hid.readEeprom(address, length);
}

void AtkDevice::rawWrite(uint16_t address, const QByteArray &data)
{
    require(!m_state.readOnly, QStringLiteral("This model is read-only"));
    require(m_info.protocol != QStringLiteral("v2"),
            QStringLiteral("Raw writes are not supported on 0xFF05 devices"));
    m_hid.writeEeprom(address, data);
}

void AtkDevice::applyDpi(const QVector<QPair<int, int>> &stages, int activeStage,
                         const QVector<QString> &colors)
{
    for (int index = 0; index < stages.size() && index < m_state.stageCount; ++index) {
        setStageDpi(index, stages.at(index).first, stages.at(index).second);
    }
    if (m_state.supportsStageColors) {
        for (int index = 0; index < colors.size() && index < m_state.stageCount; ++index) {
            if (validColor(colors.at(index))) {
                setStageColor(index, colors.at(index));
            }
        }
    }
    if (activeStage >= 0 && activeStage < m_state.stageCount
        && activeStage != m_state.activeStage) {
        setActiveStage(activeStage);
    }
}

void AtkDevice::applyPerformance(int polling, int lodCode, int debounce, bool motionSync,
                                 bool ripple, bool straightLine, int sensorMode, int sleepSeconds,
                                 int angleDegrees, bool angleEnabled)
{
    setPolling(polling);
    if (lodCode >= 0) {
        const bool allowed = std::any_of(m_state.lodChoices.begin(), m_state.lodChoices.end(),
                                         [lodCode](const QPair<int, double> &item) {
                                             return item.first == lodCode;
                                         });
        if (allowed) {
            setLiftOffCode(lodCode);
        }
    }
    if (containsInt(m_state.debounceOptions, debounce)) {
        setDebounce(debounce);
    }
    if (m_state.motionSync.has_value()) {
        setMotionSync(motionSync);
    }
    if (m_state.ripple.has_value()) {
        setRipple(ripple);
    }
    if (m_state.straightLine.has_value()) {
        setStraightLine(straightLine);
    }
    if (m_state.supportsPerformanceMode && sensorMode >= 0) {
        setSensorMode(sensorMode);
    }
    if (containsInt(m_state.sleepOptions, sleepSeconds)) {
        setSleep(sleepSeconds);
    }
    if (m_state.supportsAngle) {
        setAngle(angleDegrees, angleEnabled);
    }
}

void AtkDevice::applyButtons(const QVector<QString> &actions)
{
    for (int index = 0; index < actions.size() && index < m_state.buttons.size(); ++index) {
        if (!actions.at(index).isEmpty()) {
            setButtonAction(index, actions.at(index));
        }
    }
}
