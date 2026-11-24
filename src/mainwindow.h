#pragma once

#include "types.h"

#include <QByteArray>
#include <QMainWindow>
#include <QPair>
#include <QSystemTrayIcon>
#include <QVector>

#include <functional>

class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;
class QThread;
class QTimer;
class QMenu;

class BatteryService;
class DeviceWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setDebug(bool debug);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshDeviceList();
    void openSelectedDevice();
    void onDeviceOpened(const DeviceState &state);
    void onStateChanged(const DeviceState &state);
    void onLiveStateChanged(const DeviceState &state);
    void onDeviceClosed();
    void onError(const QString &message);
    void onLog(const QString &message);
    void onBusyChanged(bool busy);
    void onRawDataRead(const QByteArray &data);
    void onBatteryTimer();
    void applyDpi();
    void applyPerformance();
    void applyButtons();
    void applyLighting();
    void activateProfile(int profile);
    void saveProfile();
    void readRaw();
    void writeRaw();
    void installUdev();
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
    void showWindow();
    void quitApplication();
    void updateDpiFamily();
    void onDpiItemChanged(QTableWidgetItem *item);
    void onPollIntervalChanged(int seconds);
    void onTrayToggled(bool enabled);

private:
    void buildUi();
    QWidget *buildOverviewPage();
    QWidget *buildDpiPage();
    QWidget *buildPerformancePage();
    QWidget *buildButtonsPage();
    QWidget *buildLightingPage();
    QWidget *buildProfilesPage();
    QWidget *buildAdvancedPage();
    QWidget *buildSettingsPage();
    void addPage(const QString &title, QWidget *page);
    void invokeWorker(const std::function<void(DeviceWorker *)> &operation);
    void applyState(const DeviceState &state);
    void updateBatteryOnly(const DeviceState &state);
    void updateOverview(const DeviceState &state);
    void updateDpiTable(const DeviceState &state);
    void updatePerformanceUi(const DeviceState &state);
    void updateButtonsTable(const DeviceState &state);
    void updateLightingUi(const DeviceState &state);
    void updateProfilesUi(const DeviceState &state);
    void updateAdvancedUi(const DeviceState &state);
    void updateEnabledState();
    void updateBatteryUi(const DeviceState &state);
    void updateTrayIcon(const DeviceState &state);
    void writeBatteryCache(const DeviceState &state);
    void clearBatteryCache();
    void appendLog(const QString &message);
    void saveSettings();
    void loadSettings();
    void setupTray();
    void cleanupWorker();
    QString batteryCacheDir() const;
    QIcon makeAppIcon() const;
    QIcon makeBatteryIcon(int percent, bool charging) const;

    QVector<DeviceInfo> m_devices;
    QString m_currentPath;
    DeviceState m_state;
    bool m_hasDevice = false;
    bool m_busy = false;
    bool m_quitting = false;
    bool m_trayMessageShown = false;
    bool m_trayEnabled = true;
    bool m_startHidden = false;
    int m_pollIntervalSec = 30;

    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_headerBattery = nullptr;
    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_statusLabel = nullptr;

    QLabel *m_modelLabel = nullptr;
    QLabel *m_brandLabel = nullptr;
    QLabel *m_sensorLabel = nullptr;
    QLabel *m_linkLabel = nullptr;
    QLabel *m_usbLabel = nullptr;
    QLabel *m_firmwareLabel = nullptr;
    QLabel *m_dongleLabel = nullptr;
    QLabel *m_profileLabel = nullptr;
    QLabel *m_protocolLabel = nullptr;
    QLabel *m_supportLabel = nullptr;
    QProgressBar *m_batteryBar = nullptr;
    QLabel *m_batteryText = nullptr;
    QLabel *m_voltageLabel = nullptr;

    QTableWidget *m_dpiTable = nullptr;
    QPushButton *m_applyDpiButton = nullptr;
    QPushButton *m_addStageButton = nullptr;
    QPushButton *m_removeStageButton = nullptr;
    QPushButton *m_activateStageButton = nullptr;
    QLabel *m_dpiNote = nullptr;

    QComboBox *m_pollingCombo = nullptr;
    QComboBox *m_lodCombo = nullptr;
    QComboBox *m_debounceCombo = nullptr;
    QComboBox *m_sleepCombo = nullptr;
    QComboBox *m_sensorModeCombo = nullptr;
    QCheckBox *m_motionSyncCheck = nullptr;
    QCheckBox *m_rippleCheck = nullptr;
    QCheckBox *m_straightLineCheck = nullptr;
    QSpinBox *m_angleSpin = nullptr;
    QCheckBox *m_angleEnableCheck = nullptr;
    QPushButton *m_applyPerformanceButton = nullptr;

    QTableWidget *m_buttonsTable = nullptr;
    QPushButton *m_applyButtonsButton = nullptr;
    QPushButton *m_resetButtonsButton = nullptr;

    QComboBox *m_ledModeCombo = nullptr;
    QComboBox *m_ledBrightnessCombo = nullptr;
    QComboBox *m_ledSpeedCombo = nullptr;
    QPushButton *m_applyLightingButton = nullptr;

    QButtonGroup *m_profileGroup = nullptr;
    QPushButton *m_saveProfileButton = nullptr;
    QLabel *m_profileNote = nullptr;

    QComboBox *m_dpiFamilyCombo = nullptr;
    QLineEdit *m_rawAddressEdit = nullptr;
    QSpinBox *m_rawLengthSpin = nullptr;
    QPushButton *m_rawReadButton = nullptr;
    QPushButton *m_rawWriteButton = nullptr;
    QPlainTextEdit *m_rawDataEdit = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QPushButton *m_installUdevButton = nullptr;

    QSpinBox *m_pollIntervalSpin = nullptr;
    QCheckBox *m_trayCheck = nullptr;
    QCheckBox *m_startHiddenCheck = nullptr;
    QLabel *m_cachePathLabel = nullptr;

    QThread *m_workerThread = nullptr;
    DeviceWorker *m_worker = nullptr;
    BatteryService *m_batteryService = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QTimer *m_batteryTimer = nullptr;
};
