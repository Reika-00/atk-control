#include "device.h"
#include "hidraw.h"
#include "mainwindow.h"
#include "protocol.h"

#include <QApplication>
#include <QColor>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QProcess>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTextStream>

#include <cstdio>
#include <optional>

namespace {

void applyDarkTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#15171c")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e6e8ee")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#1b1e24")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#20242c")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#1b1e24")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#e6e8ee")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#e6e8ee")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#23272f")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e6e8ee")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ff6b6b")));
    palette.setColor(QPalette::Link, QColor(QStringLiteral("#5b9dff")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#2b6cff")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#7a808c")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#7a808c")));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#7a808c")));
    app.setPalette(palette);

    app.setStyleSheet(QStringLiteral(R"(
QWidget { font-size: 13px; }
QMainWindow, #Root { background: #15171c; }
#Header { background: #101216; border-bottom: 1px solid #2a2f38; }
#Title { font-size: 20px; font-weight: 600; color: #f2f4f8; }
#Subtitle { color: #8b93a3; font-size: 12px; }
#PageTitle { font-size: 22px; font-weight: 600; color: #f2f4f8; }
#PageSubtitle { color: #8b93a3; }
#Card { background: #1b1e24; border: 1px solid #2a2f38; border-radius: 12px; }
#CardTitle { font-size: 14px; font-weight: 600; color: #dfe4ee; }
#Value { color: #e6e8ee; }
#Muted { color: #8b93a3; }
#BatteryBig { font-size: 30px; font-weight: 600; color: #f2f4f8; }
#BatteryPill { background: #20242c; border: 1px solid #2a2f38; border-radius: 12px; padding: 5px 10px; color: #dfe4ee; min-width: 96px; }
#DeviceCombo { font-weight: 500; }
#Sidebar { background: #101216; border: none; border-right: 1px solid #2a2f38; padding: 10px 8px; outline: 0; }
#Sidebar::item { padding: 10px 12px; border-radius: 7px; color: #b6bdca; }
#Sidebar::item:selected { background: #223152; color: #ffffff; }
#Sidebar::item:hover { background: #1c2230; }
#Pages { background: #15171c; }
QPushButton { background: #23272f; border: 1px solid #343a45; border-radius: 7px; padding: 7px 13px; color: #e6e8ee; }
QPushButton:hover { background: #2b303a; }
QPushButton:pressed { background: #1d2128; }
QPushButton:disabled { color: #6b7280; background: #1b1e24; border-color: #262b34; }
#PrimaryButton { background: #2b6cff; border-color: #2b6cff; color: white; font-weight: 600; }
#PrimaryButton:hover { background: #3b78ff; }
#PrimaryButton:disabled { background: #24304d; border-color: #24304d; color: #7c8799; }
QComboBox, QLineEdit, QSpinBox, QPlainTextEdit { background: #15181e; border: 1px solid #2e343f; border-radius: 6px; padding: 5px 8px; selection-background-color: #2b6cff; }
QTableWidget { background: #15181e; border: 1px solid #2e343f; border-radius: 6px; selection-background-color: #2b6cff; }
QComboBox { padding-right: 24px; }
QComboBox:hover, QLineEdit:hover, QSpinBox:hover { border-color: #3b4352; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; border: none; width: 22px; }
QComboBox QAbstractItemView { background: #1b1e24; border: 1px solid #2e343f; selection-background-color: #2b6cff; }
QComboBox QAbstractItemView::item { padding: 6px 8px; }
QHeaderView::section { background: #1b1e24; color: #9aa3b2; border: none; border-bottom: 1px solid #2a2f38; padding: 7px; }
QTableWidget { gridline-color: #262b34; }
QTableWidget::item:selected { background: #223152; }
QCheckBox { color: #d6dbe4; spacing: 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid #3a414d; background: #15181e; }
QCheckBox::indicator:checked { background: #2b6cff; border-color: #2b6cff; }
QProgressBar { background: #20242c; border: none; border-radius: 9px; }
QProgressBar::chunk { background: #2b6cff; border-radius: 9px; }
QStatusBar { background: #101216; color: #8b93a3; border-top: 1px solid #2a2f38; padding: 2px 6px; }
QMenu { background: #1b1e24; border: 1px solid #2e343f; padding: 6px; }
QMenu::item { padding: 6px 18px; border-radius: 5px; }
QMenu::item:selected { background: #2b6cff; color: white; }
QScrollBar:vertical { background: #15171c; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #343a45; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #424a58; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QToolTip { background: #1b1e24; color: #e6e8ee; border: 1px solid #2e343f; }
)"));
}

QString deviceLabel(const DeviceInfo &device)
{
    QString label = device.productName.isEmpty() ? QStringLiteral("ATK/VXE mouse")
                                                 : device.productName;
    label += QStringLiteral(" \u00b7 %1").arg(device.protocol.toUpper());
    if (device.receiver) {
        label += QStringLiteral(" \u00b7 receiver");
    }
    if (!device.accessible) {
        label += QStringLiteral(" \u00b7 no permission");
    }
    return label;
}

QJsonObject deviceJson(const DeviceInfo &device)
{
    QJsonObject object;
    object.insert(QStringLiteral("path"), device.path);
    object.insert(QStringLiteral("product"), device.productName);
    object.insert(QStringLiteral("vendor_id"), static_cast<int>(device.vendorId));
    object.insert(QStringLiteral("product_id"), static_cast<int>(device.productId));
    object.insert(QStringLiteral("protocol"), device.protocol);
    object.insert(QStringLiteral("accessible"), device.accessible);
    object.insert(QStringLiteral("receiver"), device.receiver);
    return object;
}

int runList(bool json)
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QVector<DeviceInfo> devices = discoverDevices();
    QJsonArray array;
    int count = 0;
    for (const DeviceInfo &device : devices) {
        if (device.protocol != QStringLiteral("v1") && device.protocol != QStringLiteral("v2")) {
            continue;
        }
        ++count;
        if (json) {
            array.append(deviceJson(device));
        } else {
            out << device.path << "  " << QStringLiteral("VID:PID %1:%2")
                                           .arg(device.vendorId, 4, 16, QLatin1Char('0'))
                                           .arg(device.productId, 4, 16, QLatin1Char('0'))
                                           .toUpper()
                << "  " << deviceLabel(device) << "\n";
        }
    }
    if (json) {
        out << QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)) << "\n";
    } else if (count == 0) {
        out << QStringLiteral("No ATK/VXE mouse found.\n");
    }
    out.flush();
    return 0;
}

int runBattery(bool json, const QString &path, bool debug)
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QVector<DeviceInfo> devices = discoverDevices();
    const DeviceInfo *target = nullptr;
    for (const DeviceInfo &device : devices) {
        if (device.protocol != QStringLiteral("v1") && device.protocol != QStringLiteral("v2")) {
            continue;
        }
        if (!path.isEmpty() && device.path != path) {
            continue;
        }
        target = &device;
        break;
    }

    if (!target) {
        if (json) {
            QJsonObject object;
            object.insert(QStringLiteral("connected"), false);
            object.insert(QStringLiteral("percent"), -1);
            object.insert(QStringLiteral("charging"), false);
            object.insert(QStringLiteral("voltage_mv"), 0);
            out << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)) << "\n";
        } else {
            err << QStringLiteral("No ATK/VXE mouse found.\n");
        }
        return 1;
    }

    try {
        AtkDevice device(*target, debug);
        device.open();
        const DeviceState state = device.readFull();
        if (json) {
            QJsonObject object;
            object.insert(QStringLiteral("connected"), true);
            object.insert(QStringLiteral("model"), state.modelName);
            object.insert(QStringLiteral("protocol"), state.protocol);
            object.insert(QStringLiteral("percent"), state.battery.value_or(-1));
            object.insert(QStringLiteral("charging"), state.charging.value_or(false));
            object.insert(QStringLiteral("voltage_mv"), state.voltageMv.value_or(0));
            object.insert(QStringLiteral("firmware"), state.firmware);
            object.insert(QStringLiteral("dongle_firmware"), state.dongleFirmware);
            object.insert(QStringLiteral("profile"), state.profile.value_or(0));
            object.insert(QStringLiteral("updated"), QDateTime::currentSecsSinceEpoch());
            out << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)) << "\n";
        } else {
            out << state.modelName << " (" << state.protocol << ")\n";
            if (state.battery) {
                out << QStringLiteral("Battery: %1%").arg(*state.battery)
                           + (state.charging.value_or(false) ? QStringLiteral(" (charging)")
                                                             : QString())
                    << "\n";
            } else {
                out << QStringLiteral("Battery: not reported\n");
            }
            if (state.voltageMv) {
                out << QStringLiteral("Voltage: %1 mV\n").arg(*state.voltageMv);
            }
            if (!state.firmware.isEmpty()) {
                out << QStringLiteral("Firmware: %1\n").arg(state.firmware);
            }
        }
        out.flush();
        return 0;
    } catch (const std::exception &error) {
        err << QStringLiteral("Error: %1\n").arg(QString::fromUtf8(error.what()));
        err.flush();
        return 1;
    }
}

int runSelftest()
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    int failures = 0;

    const auto check = [&](bool condition, const QString &name) {
        if (condition) {
            out << QStringLiteral("PASS  %1\n").arg(name);
        } else {
            ++failures;
            err << QStringLiteral("FAIL  %1\n").arg(name);
        }
    };

    const QByteArray frame = atk::buildRead(atk::AddrSystem, 10);
    check(frame.size() == atk::PayloadSize && atk::reportIsValid(frame),
          QStringLiteral("v1 frame size and checksum"));

    const QByteArray pair = atk::packPair(44);
    check(atk::decodePair(static_cast<uint8_t>(pair.at(0)),
                          static_cast<uint8_t>(pair.at(1))) == std::optional<int>(44),
          QStringLiteral("v1 pair encoding"));

    for (const QString &family : {QStringLiteral("step50"), QStringLiteral("ultra")}) {
        const QByteArray raw = atk::packDpiStage(800, 1600, family);
        const auto stage = atk::unpackDpiStage(raw, family);
        check(stage && stage->first == 800 && stage->second == 1600,
              QStringLiteral("DPI round trip (%1)").arg(family));
    }

    const QByteArray rgb = atk::packRgb(QStringLiteral("#12ab34"));
    check(atk::unpackRgb(rgb) == QStringLiteral("#12ab34"),
          QStringLiteral("RGB round trip"));

    const QByteArray v2 = atk::buildV2Frame(atk::V2CmdBattery, 0, 0);
    check(v2.size() == atk::V2FrameSize && static_cast<uint8_t>(v2.at(0)) == atk::V2ReportId
              && static_cast<uint8_t>(v2.at(2)) == atk::V2Marker,
          QStringLiteral("v2 frame size and marker"));

    const auto code = atk::pollingToCode(1000);
    check(code && atk::pollingFromCode(*code) == std::optional<int>(1000),
          QStringLiteral("polling rate round trip"));

    out.flush();
    err.flush();
    if (failures == 0) {
        out << QStringLiteral("Selftest passed.\n");
        return 0;
    }
    err << QStringLiteral("Selftest failed with %1 error(s).\n").arg(failures);
    return 1;
}

int runInstallUdev()
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    QFile rule(QStringLiteral(":/udev/71-atk-control.rules"));
    if (!rule.open(QIODevice::ReadOnly)) {
        err << QStringLiteral("The bundled udev rule could not be read.\n");
        return 1;
    }
    const QString temporary = QDir::tempPath() + QStringLiteral("/71-atk-control.rules");
    QFile output(temporary);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        err << QStringLiteral("Could not write a temporary rule file.\n");
        return 1;
    }
    output.write(rule.readAll());
    output.close();

    const QString manual = QStringLiteral(
        "sudo install -m 0644 %1 /etc/udev/rules.d/71-atk-control.rules\n"
        "sudo udevadm control --reload-rules\n"
        "sudo udevadm trigger --subsystem-match=hidraw")
                               .arg(temporary);

    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    const QString install = QStandardPaths::findExecutable(QStringLiteral("install"));
    const QString udevadm = QStandardPaths::findExecutable(QStringLiteral("udevadm"));
    if (pkexec.isEmpty() || install.isEmpty()) {
        err << QStringLiteral("pkexec or install was not found. Run:\n\n%1\n").arg(manual);
        return 1;
    }

    const int result = QProcess::execute(
        pkexec, {install, QStringLiteral("-m"), QStringLiteral("0644"), temporary,
                 QStringLiteral("/etc/udev/rules.d/71-atk-control.rules")});
    if (result != 0) {
        err << QStringLiteral("Installation failed. Run:\n\n%1\n").arg(manual);
        return 1;
    }
    if (!udevadm.isEmpty()) {
        QProcess::execute(pkexec, {udevadm, QStringLiteral("control"),
                                   QStringLiteral("--reload-rules")});
        QProcess::execute(pkexec, {udevadm, QStringLiteral("trigger"),
                                   QStringLiteral("--subsystem-match=hidraw")});
    }
    out << QStringLiteral("Installed 71-atk-control.rules. Reconnect the mouse.\n");
    out.flush();
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("atk-control"));
    QApplication::setApplicationDisplayName(QStringLiteral("ATK Control"));
    QApplication::setOrganizationName(QStringLiteral("ATKControl"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setDesktopFileName(QStringLiteral("atk-control"));
    QApplication::setQuitOnLastWindowClosed(false);
    applyDarkTheme(app);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Configure ATK and VXE gaming mice over raw HID."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption listOption({QStringLiteral("l"), QStringLiteral("list")},
                                        QStringLiteral("List detected ATK/VXE devices and exit."));
    const QCommandLineOption batteryOption(
        {QStringLiteral("b"), QStringLiteral("battery")},
        QStringLiteral("Print battery state and exit."));
    const QCommandLineOption jsonOption(
        {QStringLiteral("j"), QStringLiteral("json")},
        QStringLiteral("Use JSON output with --list or --battery."));
    const QCommandLineOption selftestOption(
        QStringLiteral("selftest"), QStringLiteral("Run protocol self-tests and exit."));
    const QCommandLineOption installOption(
        QStringLiteral("install-udev"),
        QStringLiteral("Install the bundled udev rule and exit."));
    const QCommandLineOption deviceOption(
        {QStringLiteral("d"), QStringLiteral("device")},
        QStringLiteral("Use a specific /dev/hidraw path."), QStringLiteral("path"));
    const QCommandLineOption debugOption(
        QStringLiteral("debug"), QStringLiteral("Enable verbose protocol logging."));

    parser.addOption(listOption);
    parser.addOption(batteryOption);
    parser.addOption(jsonOption);
    parser.addOption(selftestOption);
    parser.addOption(installOption);
    parser.addOption(deviceOption);
    parser.addOption(debugOption);
    parser.process(app);

    const bool json = parser.isSet(jsonOption);
    const bool debug = parser.isSet(debugOption);
    const QString devicePath = parser.value(deviceOption);

    if (parser.isSet(listOption)) {
        return runList(json);
    }
    if (parser.isSet(batteryOption)) {
        return runBattery(json, devicePath, debug);
    }
    if (parser.isSet(selftestOption)) {
        return runSelftest();
    }
    if (parser.isSet(installOption)) {
        return runInstallUdev();
    }

    MainWindow window;
    if (debug) {
        window.setDebug(true);
    }
    return app.exec();
}
