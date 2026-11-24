#include "mainwindow.h"

#include "batteryservice.h"
#include "hidraw.h"
#include "worker.h"

#include <QAbstractButton>
#include <QApplication>
#include <QBrush>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString dash()
{
    return QStringLiteral("\u2014");
}

QString display(const QString &value)
{
    return value.isEmpty() ? dash() : value;
}

QLabel *valueLabel(const QString &text = QString())
{
    auto *label = new QLabel(text.isEmpty() ? dash() : text);
    label->setObjectName(QStringLiteral("Value"));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QWidget *card(const QString &title, QVBoxLayout **body)
{
    auto *widget = new QWidget;
    widget->setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(12);
    auto *label = new QLabel(title);
    label->setObjectName(QStringLiteral("CardTitle"));
    layout->addWidget(label);
    *body = layout;
    return widget;
}

QWidget *pageHeader(const QString &title, const QString &subtitle)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("PageTitle"));
    auto *subtitleLabel = new QLabel(subtitle);
    subtitleLabel->setObjectName(QStringLiteral("PageSubtitle"));
    subtitleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    return widget;
}

QString sleepLabel(int seconds)
{
    if (seconds <= 0) {
        return QStringLiteral("Never");
    }
    if (seconds < 60) {
        return QStringLiteral("%1 s").arg(seconds);
    }
    if (seconds % 60 == 0) {
        return QStringLiteral("%1 min").arg(seconds / 60);
    }
    return QStringLiteral("%1 min %2 s").arg(seconds / 60).arg(seconds % 60);
}

QString lodLabel(const QPair<int, double> &choice)
{
    if (choice.second > 0.0) {
        return QStringLiteral("%1 mm").arg(choice.second, 0, 'f', 1);
    }
    return QStringLiteral("Level %1").arg(choice.first);
}

struct ActionChoice
{
    const char *id;
    const char *label;
};

const QVector<ActionChoice> &actionChoices()
{
    static const QVector<ActionChoice> choices = {
        {"left", "Left click"},
        {"right", "Right click"},
        {"middle", "Middle click"},
        {"back", "Back"},
        {"forward", "Forward"},
        {"dpi_cycle", "DPI cycle"},
        {"dpi_up", "DPI up"},
        {"dpi_down", "DPI down"},
        {"disabled", "Disabled"},
    };
    return choices;
}

QString hexDump(const QByteArray &data)
{
    QStringList parts;
    parts.reserve(data.size());
    for (unsigned char byte : data) {
        parts.append(QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper());
    }
    return parts.join(QLatin1Char(' '));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    loadSettings();
    m_batteryTimer->start(m_pollIntervalSec * 1000);
    m_tray->setVisible(m_trayEnabled);
    refreshDeviceList();
    if (m_startHidden) {
        hide();
    } else {
        show();
    }
}

MainWindow::~MainWindow()
{
    cleanupWorker();
}

void MainWindow::setDebug(bool debug)
{
    if (m_worker) {
        invokeWorker([debug](DeviceWorker *worker) { worker->setDebug(debug); });
    }
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("ATK Control"));
    setWindowIcon(makeAppIcon());
    resize(1100, 760);
    setMinimumSize(900, 600);

    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("Root"));
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *header = new QWidget;
    header->setObjectName(QStringLiteral("Header"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 14, 20, 14);
    headerLayout->setSpacing(12);

    auto *titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(1);
    auto *title = new QLabel(QStringLiteral("ATK Control"));
    title->setObjectName(QStringLiteral("Title"));
    auto *subtitle = new QLabel(QStringLiteral("ATK and VXE mouse configuration"));
    subtitle->setObjectName(QStringLiteral("Subtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);
    headerLayout->addLayout(titles);
    headerLayout->addStretch(1);

    m_deviceCombo = new QComboBox;
    m_deviceCombo->setObjectName(QStringLiteral("DeviceCombo"));
    m_deviceCombo->setMinimumWidth(350);
    m_deviceCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_deviceCombo->setMinimumContentsLength(36);
    headerLayout->addWidget(m_deviceCombo);

    m_refreshButton = new QPushButton(QStringLiteral("Refresh"));
    m_refreshButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_refreshButton->setMinimumWidth(88);
    headerLayout->addWidget(m_refreshButton);

    m_headerBattery = new QLabel(QStringLiteral("No device"));
    m_headerBattery->setObjectName(QStringLiteral("BatteryPill"));
    m_headerBattery->setAlignment(Qt::AlignCenter);
    m_headerBattery->setMinimumWidth(132);
    headerLayout->addWidget(m_headerBattery);

    rootLayout->addWidget(header);

    auto *body = new QWidget;
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_sidebar = new QListWidget;
    m_sidebar->setObjectName(QStringLiteral("Sidebar"));
    m_sidebar->setFixedWidth(196);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    m_sidebar->setSpacing(2);
    m_sidebar->setUniformItemSizes(true);
    bodyLayout->addWidget(m_sidebar);

    m_stack = new QStackedWidget;
    m_stack->setObjectName(QStringLiteral("Pages"));
    bodyLayout->addWidget(m_stack, 1);

    addPage(QStringLiteral("Overview"), buildOverviewPage());
    addPage(QStringLiteral("DPI"), buildDpiPage());
    addPage(QStringLiteral("Performance"), buildPerformancePage());
    addPage(QStringLiteral("Buttons"), buildButtonsPage());
    addPage(QStringLiteral("Lighting"), buildLightingPage());
    addPage(QStringLiteral("Profiles"), buildProfilesPage());
    addPage(QStringLiteral("Advanced"), buildAdvancedPage());
    addPage(QStringLiteral("Settings"), buildSettingsPage());

    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_sidebar->setCurrentRow(0);

    rootLayout->addWidget(body, 1);
    setCentralWidget(root);

    m_statusLabel = new QLabel(QStringLiteral("Ready"));
    statusBar()->addPermanentWidget(m_statusLabel);
    statusBar()->showMessage(QStringLiteral("Connect an ATK or VXE mouse, then press Refresh."));

    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshDeviceList);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::activated), this,
            &MainWindow::openSelectedDevice);

    setupTray();

    m_workerThread = new QThread(this);
    m_worker = new DeviceWorker;
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &DeviceWorker::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_worker, &DeviceWorker::liveStateChanged, this, &MainWindow::onLiveStateChanged);
    connect(m_worker, &DeviceWorker::deviceOpened, this, &MainWindow::onDeviceOpened);
    connect(m_worker, &DeviceWorker::deviceClosed, this, &MainWindow::onDeviceClosed);
    connect(m_worker, &DeviceWorker::errorOccurred, this, &MainWindow::onError);
    connect(m_worker, &DeviceWorker::logMessage, this, &MainWindow::onLog);
    connect(m_worker, &DeviceWorker::busyChanged, this, &MainWindow::onBusyChanged);
    connect(m_worker, &DeviceWorker::rawDataRead, this, &MainWindow::onRawDataRead);
    m_workerThread->start();

    m_batteryService = new BatteryService(this);
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.atk.Control"))) {
        appendLog(QStringLiteral("D-Bus service org.atk.Control is already registered; "
                                 "the KDE widget will use the battery cache file."));
    }
    if (!bus.registerObject(QStringLiteral("/Battery"), m_batteryService,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        appendLog(QStringLiteral("Could not export the battery object on D-Bus."));
    }

    m_batteryTimer = new QTimer(this);
    connect(m_batteryTimer, &QTimer::timeout, this, &MainWindow::onBatteryTimer);

    updateEnabledState();
}

QWidget *MainWindow::buildOverviewPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("Overview"),
                                 QStringLiteral("Device information and battery state.")));

    auto *top = new QHBoxLayout;
    top->setSpacing(16);

    QVBoxLayout *deviceBody = nullptr;
    auto *deviceCard = card(QStringLiteral("Device"), &deviceBody);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(22);
    form->setVerticalSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_modelLabel = valueLabel();
    m_brandLabel = valueLabel();
    m_sensorLabel = valueLabel();
    m_linkLabel = valueLabel();
    m_usbLabel = valueLabel();
    m_firmwareLabel = valueLabel();
    m_dongleLabel = valueLabel();
    m_profileLabel = valueLabel();
    m_protocolLabel = valueLabel();
    m_supportLabel = valueLabel();

    form->addRow(QStringLiteral("Model"), m_modelLabel);
    form->addRow(QStringLiteral("Brand"), m_brandLabel);
    form->addRow(QStringLiteral("Sensor"), m_sensorLabel);
    form->addRow(QStringLiteral("Connection"), m_linkLabel);
    form->addRow(QStringLiteral("USB ID"), m_usbLabel);
    form->addRow(QStringLiteral("Firmware"), m_firmwareLabel);
    form->addRow(QStringLiteral("Dongle"), m_dongleLabel);
    form->addRow(QStringLiteral("Profile"), m_profileLabel);
    form->addRow(QStringLiteral("Protocol"), m_protocolLabel);
    form->addRow(QStringLiteral("Support"), m_supportLabel);
    deviceBody->addLayout(form);
    top->addWidget(deviceCard, 3);

    QVBoxLayout *batteryBody = nullptr;
    auto *batteryCard = card(QStringLiteral("Battery"), &batteryBody);
    m_batteryText = new QLabel(QStringLiteral("No battery data"));
    m_batteryText->setObjectName(QStringLiteral("BatteryBig"));
    m_batteryBar = new QProgressBar;
    m_batteryBar->setObjectName(QStringLiteral("BatteryBar"));
    m_batteryBar->setRange(0, 100);
    m_batteryBar->setValue(0);
    m_batteryBar->setTextVisible(false);
    m_batteryBar->setFixedHeight(18);
    m_voltageLabel = new QLabel;
    m_voltageLabel->setObjectName(QStringLiteral("Muted"));
    batteryBody->addWidget(m_batteryText);
    batteryBody->addWidget(m_batteryBar);
    batteryBody->addWidget(m_voltageLabel);
    batteryBody->addStretch(1);
    top->addWidget(batteryCard, 2);

    layout->addLayout(top);

    QVBoxLayout *noteBody = nullptr;
    auto *noteCard = card(QStringLiteral("Notes"), &noteBody);
    auto *note = new QLabel(QStringLiteral(
        "ATK and VXE mice answer configuration requests only while awake. Move the mouse if "
        "battery or settings look stale. Every change is read back from the mouse and verified."));
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("Muted"));
    noteBody->addWidget(note);
    layout->addWidget(noteCard);
    layout->addStretch(1);

    return page;
}

QWidget *MainWindow::buildDpiPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("DPI"),
                                 QStringLiteral("Edit DPI stages, the active stage and per-stage colors.")));

    QVBoxLayout *body = nullptr;
    auto *cardWidget = card(QStringLiteral("DPI stages"), &body);

    auto *top = new QHBoxLayout;
    m_dpiNote = new QLabel;
    m_dpiNote->setObjectName(QStringLiteral("Muted"));
    m_dpiNote->setWordWrap(true);
    top->addWidget(m_dpiNote, 1);

    m_activateStageButton = new QPushButton(QStringLiteral("Set active"));
    m_addStageButton = new QPushButton(QStringLiteral("Add stage"));
    m_removeStageButton = new QPushButton(QStringLiteral("Remove last"));
    top->addWidget(m_activateStageButton);
    top->addWidget(m_addStageButton);
    top->addWidget(m_removeStageButton);
    body->addLayout(top);

    m_dpiTable = new QTableWidget;
    m_dpiTable->setColumnCount(5);
    m_dpiTable->setHorizontalHeaderLabels(
        {QStringLiteral("Active"), QStringLiteral("Stage"), QStringLiteral("X DPI"),
         QStringLiteral("Y DPI"), QStringLiteral("Color")});
    m_dpiTable->verticalHeader()->setVisible(false);
    m_dpiTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_dpiTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_dpiTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_dpiTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_dpiTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_dpiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dpiTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dpiTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                | QAbstractItemView::SelectedClicked
                                | QAbstractItemView::EditKeyPressed);
    body->addWidget(m_dpiTable, 1);

    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    m_applyDpiButton = new QPushButton(QStringLiteral("Apply DPI"));
    m_applyDpiButton->setObjectName(QStringLiteral("PrimaryButton"));
    bottom->addWidget(m_applyDpiButton);
    body->addLayout(bottom);

    layout->addWidget(cardWidget, 1);

    connect(m_dpiTable, &QTableWidget::itemChanged, this, &MainWindow::onDpiItemChanged);
    connect(m_applyDpiButton, &QPushButton::clicked, this, &MainWindow::applyDpi);
    connect(m_activateStageButton, &QPushButton::clicked, this, &MainWindow::applyDpi);
    connect(m_addStageButton, &QPushButton::clicked, this, [this] {
        invokeWorker([](DeviceWorker *worker) { worker->addStage(); });
    });
    connect(m_removeStageButton, &QPushButton::clicked, this, [this] {
        invokeWorker([](DeviceWorker *worker) { worker->removeLastStage(); });
    });

    return page;
}

QWidget *MainWindow::buildPerformancePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("Performance"),
                                 QStringLiteral("Polling rate, lift-off distance, sensor behavior and sleep.")));

    QVBoxLayout *body = nullptr;
    auto *cardWidget = card(QStringLiteral("Sensor and link"), &body);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_pollingCombo = new QComboBox;
    m_lodCombo = new QComboBox;
    m_debounceCombo = new QComboBox;
    m_sleepCombo = new QComboBox;
    m_sensorModeCombo = new QComboBox;

    form->addRow(QStringLiteral("Polling rate"), m_pollingCombo);
    form->addRow(QStringLiteral("Lift-off distance"), m_lodCombo);
    form->addRow(QStringLiteral("Debounce"), m_debounceCombo);
    form->addRow(QStringLiteral("Sleep timeout"), m_sleepCombo);
    form->addRow(QStringLiteral("Sensor mode"), m_sensorModeCombo);
    body->addLayout(form);

    auto *checks = new QHBoxLayout;
    m_motionSyncCheck = new QCheckBox(QStringLiteral("Motion Sync"));
    m_rippleCheck = new QCheckBox(QStringLiteral("Ripple control"));
    m_straightLineCheck = new QCheckBox(QStringLiteral("Straight-line correction"));
    checks->addWidget(m_motionSyncCheck);
    checks->addWidget(m_rippleCheck);
    checks->addWidget(m_straightLineCheck);
    checks->addStretch(1);
    body->addLayout(checks);

    auto *angleRow = new QHBoxLayout;
    m_angleEnableCheck = new QCheckBox(QStringLiteral("Angle snapping"));
    m_angleSpin = new QSpinBox;
    m_angleSpin->setRange(0, 180);
    m_angleSpin->setSuffix(QStringLiteral(" deg"));
    angleRow->addWidget(m_angleEnableCheck);
    angleRow->addWidget(m_angleSpin);
    angleRow->addStretch(1);
    body->addLayout(angleRow);

    auto *note = new QLabel(QStringLiteral(
        "Only options reported by the connected model are written. Unsupported controls stay disabled."));
    note->setObjectName(QStringLiteral("Muted"));
    note->setWordWrap(true);
    body->addWidget(note);

    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    m_applyPerformanceButton = new QPushButton(QStringLiteral("Apply performance settings"));
    m_applyPerformanceButton->setObjectName(QStringLiteral("PrimaryButton"));
    bottom->addWidget(m_applyPerformanceButton);
    body->addLayout(bottom);

    layout->addWidget(cardWidget);
    layout->addStretch(1);

    connect(m_applyPerformanceButton, &QPushButton::clicked, this, &MainWindow::applyPerformance);

    return page;
}

QWidget *MainWindow::buildButtonsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("Buttons"),
                                 QStringLiteral("Remap the physical buttons of the mouse.")));

    QVBoxLayout *body = nullptr;
    auto *cardWidget = card(QStringLiteral("Button assignments"), &body);

    m_buttonsTable = new QTableWidget;
    m_buttonsTable->setColumnCount(2);
    m_buttonsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Button"), QStringLiteral("Action")});
    m_buttonsTable->verticalHeader()->setVisible(false);
    m_buttonsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_buttonsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_buttonsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_buttonsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    body->addWidget(m_buttonsTable, 1);

    auto *note = new QLabel(QStringLiteral(
        "Assignments are written to the mouse and verified by reading them back. Some models expose "
        "only a subset of buttons."));
    note->setObjectName(QStringLiteral("Muted"));
    note->setWordWrap(true);
    body->addWidget(note);

    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    m_resetButtonsButton = new QPushButton(QStringLiteral("Reset defaults"));
    m_applyButtonsButton = new QPushButton(QStringLiteral("Apply buttons"));
    m_applyButtonsButton->setObjectName(QStringLiteral("PrimaryButton"));
    bottom->addWidget(m_resetButtonsButton);
    bottom->addWidget(m_applyButtonsButton);
    body->addLayout(bottom);

    layout->addWidget(cardWidget, 1);

    connect(m_applyButtonsButton, &QPushButton::clicked, this, &MainWindow::applyButtons);
    connect(m_resetButtonsButton, &QPushButton::clicked, this, [this] {
        invokeWorker([](DeviceWorker *worker) { worker->resetButtons(); });
    });

    return page;
}

QWidget *MainWindow::buildLightingPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("Lighting"),
                                 QStringLiteral("Logo and DPI indicator lighting.")));

    QVBoxLayout *body = nullptr;
    auto *cardWidget = card(QStringLiteral("Lighting effect"), &body);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_ledModeCombo = new QComboBox;
    m_ledModeCombo->addItem(QStringLiteral("Off"), 0);
    m_ledModeCombo->addItem(QStringLiteral("Steady"), 1);
    m_ledModeCombo->addItem(QStringLiteral("Breathing"), 2);

    m_ledBrightnessCombo = new QComboBox;
    m_ledBrightnessCombo->addItem(QStringLiteral("Low"), 0);
    m_ledBrightnessCombo->addItem(QStringLiteral("Medium"), 1);
    m_ledBrightnessCombo->addItem(QStringLiteral("High"), 2);

    m_ledSpeedCombo = new QComboBox;
    m_ledSpeedCombo->addItem(QStringLiteral("Slow"), 0);
    m_ledSpeedCombo->addItem(QStringLiteral("Medium"), 1);
    m_ledSpeedCombo->addItem(QStringLiteral("Fast"), 2);

    form->addRow(QStringLiteral("Effect"), m_ledModeCombo);
    form->addRow(QStringLiteral("Brightness"), m_ledBrightnessCombo);
    form->addRow(QStringLiteral("Speed"), m_ledSpeedCombo);
    body->addLayout(form);

    auto *note = new QLabel(QStringLiteral(
        "Lighting support varies by model. Per-stage DPI colors are configured on the DPI page."));
    note->setObjectName(QStringLiteral("Muted"));
    note->setWordWrap(true);
    body->addWidget(note);

    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    m_applyLightingButton = new QPushButton(QStringLiteral("Apply lighting"));
    m_applyLightingButton->setObjectName(QStringLiteral("PrimaryButton"));
    bottom->addWidget(m_applyLightingButton);
    body->addLayout(bottom);

    layout->addWidget(cardWidget);
    layout->addStretch(1);

    connect(m_applyLightingButton, &QPushButton::clicked, this, &MainWindow::applyLighting);

    return page;
}

QWidget *MainWindow::buildProfilesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("Profiles"),
                                 QStringLiteral("Switch between on-board profiles.")));

    QVBoxLayout *body = nullptr;
    auto *cardWidget = card(QStringLiteral("On-board profiles"), &body);

    auto *buttons = new QHBoxLayout;
    m_profileGroup = new QButtonGroup(this);
    m_profileGroup->setExclusive(true);
    for (int profile = 1; profile <= 4; ++profile) {
        auto *button = new QPushButton(QStringLiteral("Profile %1").arg(profile));
        button->setCheckable(true);
        m_profileGroup->addButton(button, profile);
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    body->addLayout(buttons);

    m_profileNote = new QLabel;
    m_profileNote->setObjectName(QStringLiteral("Muted"));
    m_profileNote->setWordWrap(true);
    body->addWidget(m_profileNote);

    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    m_saveProfileButton = new QPushButton(QStringLiteral("Save current settings"));
    m_saveProfileButton->setObjectName(QStringLiteral("PrimaryButton"));
    bottom->addWidget(m_saveProfileButton);
    body->addLayout(bottom);

    layout->addWidget(cardWidget);
    layout->addStretch(1);

    connect(m_profileGroup, &QButtonGroup::idClicked, this, &MainWindow::activateProfile);
    connect(m_saveProfileButton, &QPushButton::clicked, this, &MainWindow::saveProfile);

    return page;
}

QWidget *MainWindow::buildAdvancedPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("Advanced"),
                                 QStringLiteral("DPI encoding, raw EEPROM access, permissions and logs.")));

    QVBoxLayout *encodingBody = nullptr;
    auto *encodingCard = card(QStringLiteral("DPI encoding"), &encodingBody);
    auto *encodingRow = new QHBoxLayout;
    encodingRow->addWidget(new QLabel(QStringLiteral("Encoding")));
    m_dpiFamilyCombo = new QComboBox;
    m_dpiFamilyCombo->addItem(QStringLiteral("Auto (model default)"), QString());
    m_dpiFamilyCombo->addItem(QStringLiteral("Step 50"), QStringLiteral("step50"));
    m_dpiFamilyCombo->addItem(QStringLiteral("Ultra"), QStringLiteral("ultra"));
    encodingRow->addWidget(m_dpiFamilyCombo, 1);
    encodingBody->addLayout(encodingRow);
    auto *encodingNote = new QLabel(QStringLiteral(
        "Change this only if DPI values are wrong for your model. Auto uses the model table."));
    encodingNote->setObjectName(QStringLiteral("Muted"));
    encodingNote->setWordWrap(true);
    encodingBody->addWidget(encodingNote);
    layout->addWidget(encodingCard);

    QVBoxLayout *rawBody = nullptr;
    auto *rawCard = card(QStringLiteral("Raw EEPROM access"), &rawBody);
    auto *rawRow = new QHBoxLayout;
    m_rawAddressEdit = new QLineEdit(QStringLiteral("0x0000"));
    m_rawAddressEdit->setMaximumWidth(120);
    m_rawLengthSpin = new QSpinBox;
    m_rawLengthSpin->setRange(1, 10);
    m_rawLengthSpin->setValue(10);
    m_rawReadButton = new QPushButton(QStringLiteral("Read"));
    m_rawWriteButton = new QPushButton(QStringLiteral("Write"));
    rawRow->addWidget(new QLabel(QStringLiteral("Address")));
    rawRow->addWidget(m_rawAddressEdit);
    rawRow->addWidget(new QLabel(QStringLiteral("Length")));
    rawRow->addWidget(m_rawLengthSpin);
    rawRow->addWidget(m_rawReadButton);
    rawRow->addWidget(m_rawWriteButton);
    rawRow->addStretch(1);
    rawBody->addLayout(rawRow);

    m_rawDataEdit = new QPlainTextEdit;
    m_rawDataEdit->setPlaceholderText(QStringLiteral("Hex bytes, for example: 01 02 03 04"));
    m_rawDataEdit->setMaximumHeight(90);
    rawBody->addWidget(m_rawDataEdit);

    auto *rawNote = new QLabel(QStringLiteral(
        "Protocol v1 only. Raw writes change undocumented settings; use them at your own risk."));
    rawNote->setObjectName(QStringLiteral("Muted"));
    rawNote->setWordWrap(true);
    rawBody->addWidget(rawNote);
    layout->addWidget(rawCard);

    QVBoxLayout *permissionsBody = nullptr;
    auto *permissionsCard = card(QStringLiteral("Permissions"), &permissionsBody);
    auto *permissionsNote = new QLabel(QStringLiteral(
        "Direct HID access needs a udev rule. Install the bundled rule to allow the current user "
        "to open ATK and VXE hidraw devices without root."));
    permissionsNote->setObjectName(QStringLiteral("Muted"));
    permissionsNote->setWordWrap(true);
    permissionsBody->addWidget(permissionsNote);
    auto *permissionsRow = new QHBoxLayout;
    m_installUdevButton = new QPushButton(QStringLiteral("Install udev rule"));
    permissionsRow->addWidget(m_installUdevButton);
    permissionsRow->addStretch(1);
    permissionsBody->addLayout(permissionsRow);
    layout->addWidget(permissionsCard);

    QVBoxLayout *logBody = nullptr;
    auto *logCard = card(QStringLiteral("Log"), &logBody);
    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(1000);
    m_logEdit->setMaximumHeight(160);
    logBody->addWidget(m_logEdit);
    auto *logRow = new QHBoxLayout;
    auto *clearLog = new QPushButton(QStringLiteral("Clear log"));
    logRow->addWidget(clearLog);
    logRow->addStretch(1);
    logBody->addLayout(logRow);
    layout->addWidget(logCard);
    layout->addStretch(1);

    connect(m_dpiFamilyCombo, QOverload<int>::of(&QComboBox::activated), this,
            &MainWindow::updateDpiFamily);
    connect(m_rawReadButton, &QPushButton::clicked, this, &MainWindow::readRaw);
    connect(m_rawWriteButton, &QPushButton::clicked, this, &MainWindow::writeRaw);
    connect(m_installUdevButton, &QPushButton::clicked, this, &MainWindow::installUdev);
    connect(clearLog, &QPushButton::clicked, m_logEdit, &QPlainTextEdit::clear);

    return page;
}

QWidget *MainWindow::buildSettingsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    layout->addWidget(pageHeader(QStringLiteral("Settings"),
                                 QStringLiteral("Background refresh, tray behavior and data location.")));

    QVBoxLayout *generalBody = nullptr;
    auto *generalCard = card(QStringLiteral("General"), &generalBody);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(10);
    m_pollIntervalSpin = new QSpinBox;
    m_pollIntervalSpin->setRange(5, 300);
    m_pollIntervalSpin->setSuffix(QStringLiteral(" s"));
    form->addRow(QStringLiteral("Battery refresh"), m_pollIntervalSpin);

    m_trayCheck = new QCheckBox(QStringLiteral("Keep running in the system tray"));
    m_startHiddenCheck = new QCheckBox(QStringLiteral("Start hidden"));
    generalBody->addLayout(form);
    generalBody->addWidget(m_trayCheck);
    generalBody->addWidget(m_startHiddenCheck);

    m_cachePathLabel = new QLabel(batteryCacheDir() + QStringLiteral("/battery.json"));
    m_cachePathLabel->setObjectName(QStringLiteral("Muted"));
    m_cachePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *cacheRow = new QHBoxLayout;
    cacheRow->addWidget(new QLabel(QStringLiteral("Battery cache")));
    cacheRow->addWidget(m_cachePathLabel, 1);
    auto *openCache = new QPushButton(QStringLiteral("Open folder"));
    cacheRow->addWidget(openCache);
    generalBody->addLayout(cacheRow);

    layout->addWidget(generalCard);

    QVBoxLayout *aboutBody = nullptr;
    auto *aboutCard = card(QStringLiteral("About"), &aboutBody);
    auto *about = new QLabel(QStringLiteral(
        "ATK Control 1.0.0\n"
        "Open-source Qt frontend for ATK and VXE gaming mice.\n"
        "License: GPL-3.0-only. Protocol research based on the MIT-licensed "
        "atk-control project."));
    about->setWordWrap(true);
    about->setObjectName(QStringLiteral("Muted"));
    aboutBody->addWidget(about);
    layout->addWidget(aboutCard);
    layout->addStretch(1);

    connect(m_pollIntervalSpin, &QSpinBox::valueChanged, this, &MainWindow::onPollIntervalChanged);
    connect(m_trayCheck, &QCheckBox::toggled, this, &MainWindow::onTrayToggled);
    connect(m_startHiddenCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_startHidden = enabled;
        QSettings settings;
        settings.setValue(QStringLiteral("startHidden"), enabled);
    });
    connect(openCache, &QPushButton::clicked, this, [this] {
        const QString dir = batteryCacheDir();
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });

    return page;
}

void MainWindow::addPage(const QString &title, QWidget *page)
{
    m_sidebar->addItem(title);
    m_stack->addWidget(page);
}

void MainWindow::setupTray()
{
    m_tray = new QSystemTrayIcon(makeAppIcon(), this);
    m_tray->setToolTip(QStringLiteral("ATK Control"));
    m_trayMenu = new QMenu(this);
    auto *showAction = m_trayMenu->addAction(QStringLiteral("Show window"));
    auto *refreshAction = m_trayMenu->addAction(QStringLiteral("Refresh device"));
    m_trayMenu->addSeparator();
    auto *quitAction = m_trayMenu->addAction(QStringLiteral("Quit"));
    m_tray->setContextMenu(m_trayMenu);
    connect(showAction, &QAction::triggered, this, &MainWindow::showWindow);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshDeviceList);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitApplication);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::trayActivated);
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &MainWindow::showWindow);
    m_tray->show();
}

void MainWindow::invokeWorker(const std::function<void(DeviceWorker *)> &operation)
{
    if (!m_worker) {
        return;
    }
    DeviceWorker *worker = m_worker;
    QMetaObject::invokeMethod(
        worker, [worker, operation] { operation(worker); }, Qt::QueuedConnection);
}

void MainWindow::refreshDeviceList()
{
    const QString previousPath = m_currentPath;
    m_devices = discoverDevices();

    QSignalBlocker blocker(m_deviceCombo);
    m_deviceCombo->clear();

    int selectedRow = -1;
    for (int index = 0; index < m_devices.size(); ++index) {
        const DeviceInfo &device = m_devices.at(index);
        if (device.protocol != QStringLiteral("v1") && device.protocol != QStringLiteral("v2")) {
            continue;
        }
        QString label = device.productName.isEmpty() ? QStringLiteral("ATK/VXE mouse")
                                                     : device.productName;
        label += QStringLiteral(" \u00b7 %1").arg(device.protocol.toUpper());
        if (device.receiver) {
            label += QStringLiteral(" \u00b7 receiver");
        }
        if (!device.accessible) {
            label += QStringLiteral(" \u00b7 no permission");
        }
        m_deviceCombo->addItem(label, index);
        m_deviceCombo->setItemData(m_deviceCombo->count() - 1, label, Qt::ToolTipRole);
        if (!previousPath.isEmpty() && device.path == previousPath) {
            selectedRow = m_deviceCombo->count() - 1;
        }
    }

    if (m_deviceCombo->count() == 0) {
        m_deviceCombo->addItem(QStringLiteral("No ATK/VXE mouse found"), -1);
        m_deviceCombo->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("Ready"));
        statusBar()->showMessage(
            QStringLiteral("No ATK/VXE mouse found. Connect a mouse and press Refresh."), 8000);
        if (m_hasDevice) {
            invokeWorker([](DeviceWorker *worker) { worker->closeDevice(); });
        }
        return;
    }

    m_deviceCombo->setEnabled(true);
    if (selectedRow < 0) {
        selectedRow = 0;
    }
    m_deviceCombo->setCurrentIndex(selectedRow);

    const int deviceIndex = m_deviceCombo->currentData().toInt();
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return;
    }
    const DeviceInfo &device = m_devices.at(deviceIndex);
    if (!m_hasDevice || m_currentPath != device.path) {
        openSelectedDevice();
    }
}

void MainWindow::openSelectedDevice()
{
    const int index = m_deviceCombo->currentData().toInt();
    if (index < 0 || index >= m_devices.size()) {
        return;
    }
    const DeviceInfo info = m_devices.at(index);
    if (info.path.isEmpty()) {
        return;
    }
    m_currentPath = info.path;
    m_statusLabel->setText(QStringLiteral("Connecting\u2026"));
    if (!info.accessible) {
        appendLog(QStringLiteral("No direct permission for %1. Install the udev rule from Advanced.")
                      .arg(info.path));
    }
    invokeWorker([info](DeviceWorker *worker) { worker->openDevice(info); });
}

void MainWindow::onDeviceOpened(const DeviceState &state)
{
    applyState(state);
    statusBar()->showMessage(QStringLiteral("Connected to %1").arg(state.modelName), 5000);
}

void MainWindow::onStateChanged(const DeviceState &state)
{
    applyState(state);
}

void MainWindow::onLiveStateChanged(const DeviceState &state)
{
    updateBatteryOnly(state);
}

void MainWindow::onDeviceClosed()
{
    m_hasDevice = false;
    m_currentPath.clear();
    m_state = DeviceState {};
    updateOverview(m_state);
    updateDpiTable(m_state);
    updatePerformanceUi(m_state);
    updateButtonsTable(m_state);
    updateLightingUi(m_state);
    updateProfilesUi(m_state);
    updateAdvancedUi(m_state);
    updateBatteryUi(m_state);
    updateEnabledState();
    m_statusLabel->setText(QStringLiteral("Ready"));
}

void MainWindow::onError(const QString &message)
{
    statusBar()->showMessage(message, 10000);
    appendLog(QStringLiteral("ERROR: %1").arg(message));
    qWarning().noquote() << "ATK Control:" << message;
}

void MainWindow::onLog(const QString &message)
{
    appendLog(message);
}

void MainWindow::onBusyChanged(bool busy)
{
    m_busy = busy;
    if (busy) {
        m_statusLabel->setText(QStringLiteral("Working\u2026"));
    } else if (m_hasDevice) {
        m_statusLabel->setText(QStringLiteral("Connected"));
    } else {
        m_statusLabel->setText(QStringLiteral("Ready"));
    }
    updateEnabledState();
}

void MainWindow::onRawDataRead(const QByteArray &data)
{
    const QString text = hexDump(data);
    m_rawDataEdit->setPlainText(text);
    appendLog(QStringLiteral("Raw read: %1").arg(text));
}

void MainWindow::onBatteryTimer()
{
    if (!m_hasDevice || m_busy) {
        return;
    }
    invokeWorker([](DeviceWorker *worker) { worker->refreshLive(); });
}

void MainWindow::applyState(const DeviceState &state)
{
    if (state.modelName.isEmpty() && state.protocol.isEmpty()) {
        onDeviceClosed();
        return;
    }
    m_state = state;
    m_hasDevice = true;
    m_statusLabel->setText(QStringLiteral("Connected"));
    updateOverview(state);
    updateDpiTable(state);
    updatePerformanceUi(state);
    updateButtonsTable(state);
    updateLightingUi(state);
    updateProfilesUi(state);
    updateAdvancedUi(state);
    updateBatteryUi(state);
    updateEnabledState();
}

void MainWindow::updateBatteryOnly(const DeviceState &state)
{
    if (state.modelName.isEmpty() && state.protocol.isEmpty()) {
        return;
    }
    m_state.battery = state.battery;
    m_state.voltageMv = state.voltageMv;
    m_state.charging = state.charging;
    m_state.receiverOnline = state.receiverOnline;
    updateOverview(m_state);
    updateBatteryUi(m_state);
}

void MainWindow::updateOverview(const DeviceState &state)
{
    if (state.modelName.isEmpty()) {
        m_modelLabel->setText(dash());
        m_brandLabel->setText(dash());
        m_sensorLabel->setText(dash());
        m_linkLabel->setText(dash());
        m_usbLabel->setText(dash());
        m_firmwareLabel->setText(dash());
        m_dongleLabel->setText(dash());
        m_profileLabel->setText(dash());
        m_protocolLabel->setText(dash());
        m_supportLabel->setText(dash());
        return;
    }

    m_modelLabel->setText(display(state.modelName));
    m_brandLabel->setText(display(state.brand));
    m_sensorLabel->setText(display(state.sensor));

    QString link = state.linkLabel();
    if (state.viaReceiver) {
        link += state.receiverOnline ? QStringLiteral(" \u00b7 online")
                                     : QStringLiteral(" \u00b7 offline");
    }
    m_linkLabel->setText(link);

    m_usbLabel->setText(QStringLiteral("VID %1 \u00b7 PID %2 \u00b7 %3")
                            .arg(state.vendorId, 4, 16, QLatin1Char('0'))
                            .arg(state.productId, 4, 16, QLatin1Char('0'))
                            .arg(display(state.usbBcd))
                            .toUpper());
    m_firmwareLabel->setText(display(state.firmware));
    m_dongleLabel->setText(display(state.dongleFirmware));
    m_profileLabel->setText(state.profile ? QStringLiteral("Profile %1").arg(*state.profile)
                                          : dash());
    m_protocolLabel->setText(state.protocol == QStringLiteral("v2")
                                 ? QStringLiteral("v2 (64-byte reports)")
                                 : QStringLiteral("v1 (17-byte reports)"));
    m_supportLabel->setText(display(state.support));
}

void MainWindow::updateDpiTable(const DeviceState &state)
{
    QSignalBlocker blocker(m_dpiTable);
    m_dpiTable->setRowCount(state.stages.size());

    for (int row = 0; row < state.stages.size(); ++row) {
        auto *active = new QTableWidgetItem;
        active->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        active->setCheckState(row == state.activeStage ? Qt::Checked : Qt::Unchecked);
        m_dpiTable->setItem(row, 0, active);

        auto *stage = new QTableWidgetItem(QString::number(row + 1));
        stage->setFlags(Qt::ItemIsEnabled);
        m_dpiTable->setItem(row, 1, stage);

        auto *x = new QTableWidgetItem(QString::number(state.stages.at(row).first));
        m_dpiTable->setItem(row, 2, x);

        auto *y = new QTableWidgetItem(QString::number(state.stages.at(row).second));
        m_dpiTable->setItem(row, 3, y);

        const QString color = row < state.stageColors.size() ? state.stageColors.at(row)
                                                             : QStringLiteral("#ffffff");
        auto *colorItem = new QTableWidgetItem(color);
        if (!state.supportsStageColors) {
            colorItem->setFlags(Qt::ItemIsEnabled);
        }
        m_dpiTable->setItem(row, 4, colorItem);
    }

    if (state.modelName.isEmpty()) {
        m_dpiNote->setText(QStringLiteral("Connect a mouse to edit DPI stages."));
    } else if (state.protocol == QStringLiteral("v2")) {
        m_dpiNote->setText(QStringLiteral(
            "Protocol v2 stores eight stages. Check a row to make it the active stage."));
    } else {
        m_dpiNote->setText(QStringLiteral(
            "Protocol v1 stores one to eight stages. Use the buttons to add or remove stages."));
    }
    if (!state.supportsStageColors && !state.modelName.isEmpty()) {
        m_dpiNote->setText(m_dpiNote->text() + QStringLiteral(" This model does not report per-stage colors."));
    }
}

void MainWindow::updatePerformanceUi(const DeviceState &state)
{
    QSignalBlocker blockPolling(m_pollingCombo);
    QSignalBlocker blockLod(m_lodCombo);
    QSignalBlocker blockDebounce(m_debounceCombo);
    QSignalBlocker blockSleep(m_sleepCombo);
    QSignalBlocker blockSensor(m_sensorModeCombo);
    QSignalBlocker blockMotion(m_motionSyncCheck);
    QSignalBlocker blockRipple(m_rippleCheck);
    QSignalBlocker blockStraight(m_straightLineCheck);
    QSignalBlocker blockAngle(m_angleSpin);
    QSignalBlocker blockAngleEnable(m_angleEnableCheck);

    m_pollingCombo->clear();
    if (state.pollingRates.isEmpty()) {
        m_pollingCombo->addItem(QStringLiteral("Not reported"), -1);
    } else {
        for (int rate : state.pollingRates) {
            m_pollingCombo->addItem(QStringLiteral("%1 Hz").arg(rate), rate);
        }
    }
    m_pollingCombo->setCurrentIndex(
        qMax(0, m_pollingCombo->findData(state.pollingRate)));

    m_lodCombo->clear();
    if (state.lodChoices.isEmpty()) {
        m_lodCombo->addItem(QStringLiteral("Not reported"), -1);
    } else {
        for (const auto &choice : state.lodChoices) {
            m_lodCombo->addItem(lodLabel(choice), choice.first);
        }
    }
    m_lodCombo->setCurrentIndex(
        qMax(0, m_lodCombo->findData(state.lodCode.value_or(-1))));

    m_debounceCombo->clear();
    if (state.debounceOptions.isEmpty()) {
        m_debounceCombo->addItem(QStringLiteral("Not reported"), -1);
    } else {
        for (int value : state.debounceOptions) {
            m_debounceCombo->addItem(QStringLiteral("%1 ms").arg(value), value);
        }
    }
    m_debounceCombo->setCurrentIndex(
        qMax(0, m_debounceCombo->findData(state.debounceMs.value_or(-1))));

    m_sleepCombo->clear();
    if (state.sleepOptions.isEmpty()) {
        m_sleepCombo->addItem(QStringLiteral("Not reported"), -1);
    } else {
        for (int value : state.sleepOptions) {
            m_sleepCombo->addItem(sleepLabel(value), value);
        }
    }
    m_sleepCombo->setCurrentIndex(
        qMax(0, m_sleepCombo->findData(state.sleepSeconds.value_or(-1))));

    m_sensorModeCombo->clear();
    if (state.supportsPerformanceMode) {
        m_sensorModeCombo->addItem(QStringLiteral("Basic"), 0);
        m_sensorModeCombo->addItem(QStringLiteral("Competitive"), 1);
        if (state.supportsSensorMax) {
            m_sensorModeCombo->addItem(QStringLiteral("Competitive MAX"), 2);
        }
    } else {
        m_sensorModeCombo->addItem(QStringLiteral("Not supported"), -1);
    }
    m_sensorModeCombo->setCurrentIndex(
        qMax(0, m_sensorModeCombo->findData(state.sensorMode.value_or(-1))));

    m_motionSyncCheck->setChecked(state.motionSync.value_or(false));
    m_rippleCheck->setChecked(state.ripple.value_or(false));
    m_straightLineCheck->setChecked(state.straightLine.value_or(false));
    m_angleEnableCheck->setChecked(state.angleEnabled.value_or(false));
    m_angleSpin->setValue(state.angleDegrees.value_or(0));
}

void MainWindow::updateButtonsTable(const DeviceState &state)
{
    QSignalBlocker blocker(m_buttonsTable);
    m_buttonsTable->setRowCount(0);
    m_buttonsTable->setRowCount(state.buttons.size());

    for (int row = 0; row < state.buttons.size(); ++row) {
        const ButtonInfo &button = state.buttons.at(row);
        auto *name = new QTableWidgetItem(display(button.name));
        name->setFlags(Qt::ItemIsEnabled);
        m_buttonsTable->setItem(row, 0, name);

        auto *combo = new QComboBox;
        for (const ActionChoice &choice : actionChoices()) {
            combo->addItem(QString::fromLatin1(choice.label), QString::fromLatin1(choice.id));
        }
        const int index = combo->findData(button.actionId);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
        combo->setProperty("readable", button.readable);
        if (!button.readable || !state.supportsButtonRemap) {
            combo->setEnabled(false);
        }
        m_buttonsTable->setCellWidget(row, 1, combo);
    }
}

void MainWindow::updateLightingUi(const DeviceState &state)
{
    QSignalBlocker blockMode(m_ledModeCombo);
    QSignalBlocker blockBrightness(m_ledBrightnessCombo);
    QSignalBlocker blockSpeed(m_ledSpeedCombo);

    const int mode = state.ledMode.value_or(0);
    const int brightness = state.ledBrightness.value_or(1);
    const int speed = state.ledSpeed.value_or(1);
    m_ledModeCombo->setCurrentIndex(qMax(0, m_ledModeCombo->findData(mode)));
    m_ledBrightnessCombo->setCurrentIndex(qMax(0, m_ledBrightnessCombo->findData(brightness)));
    m_ledSpeedCombo->setCurrentIndex(qMax(0, m_ledSpeedCombo->findData(speed)));
}

void MainWindow::updateProfilesUi(const DeviceState &state)
{
    const int profile = state.profile.value_or(0);
    for (QAbstractButton *button : m_profileGroup->buttons()) {
        const int id = m_profileGroup->id(button);
        button->setChecked(id == profile);
        button->setEnabled(!m_busy && m_hasDevice && state.supportsProfiles);
    }
    if (!state.modelName.isEmpty() && !state.supportsProfiles) {
        m_profileNote->setText(QStringLiteral("This model does not expose on-board profiles."));
    } else if (state.protocol == QStringLiteral("v2")) {
        m_profileNote->setText(QStringLiteral(
            "Protocol v2 saves profile changes immediately. Use Save only if you want to force a commit."));
    } else {
        m_profileNote->setText(QStringLiteral(
            "Protocol v1 needs an explicit save. Click Save current settings to write the active profile."));
    }
}

void MainWindow::updateAdvancedUi(const DeviceState &state)
{
    QSignalBlocker blocker(m_dpiFamilyCombo);
    const int index = m_dpiFamilyCombo->findData(state.dpiFamily);
    m_dpiFamilyCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void MainWindow::updateEnabledState()
{
    const bool connected = m_hasDevice && !m_busy;

    m_dpiTable->setEnabled(connected);
    m_applyDpiButton->setEnabled(connected);
    m_activateStageButton->setEnabled(connected);
    m_addStageButton->setEnabled(connected && m_state.protocol == QStringLiteral("v1")
                                 && m_state.supportsStageCount && m_state.stageCount < 8);
    m_removeStageButton->setEnabled(connected && m_state.protocol == QStringLiteral("v1")
                                    && m_state.supportsStageCount && m_state.stageCount > 1);

    m_applyPerformanceButton->setEnabled(connected);
    m_pollingCombo->setEnabled(connected && !m_state.pollingRates.isEmpty());
    m_lodCombo->setEnabled(connected && !m_state.lodChoices.isEmpty());
    m_debounceCombo->setEnabled(connected && !m_state.debounceOptions.isEmpty());
    m_sleepCombo->setEnabled(connected && !m_state.sleepOptions.isEmpty());
    m_sensorModeCombo->setEnabled(connected && m_state.supportsPerformanceMode);
    m_motionSyncCheck->setEnabled(connected && m_state.motionSync.has_value());
    m_rippleCheck->setEnabled(connected && m_state.ripple.has_value());
    m_straightLineCheck->setEnabled(connected && m_state.straightLine.has_value());
    m_angleSpin->setEnabled(connected && m_state.supportsAngle);
    m_angleEnableCheck->setEnabled(connected && m_state.supportsAngle);

    m_buttonsTable->setEnabled(connected && m_state.supportsButtonRemap);
    m_applyButtonsButton->setEnabled(connected && m_state.supportsButtonRemap);
    m_resetButtonsButton->setEnabled(connected && m_state.supportsButtonRemap);

    m_ledModeCombo->setEnabled(connected && m_state.supportsLighting);
    m_ledBrightnessCombo->setEnabled(connected && m_state.supportsLighting);
    m_ledSpeedCombo->setEnabled(connected && m_state.supportsLighting);
    m_applyLightingButton->setEnabled(connected && m_state.supportsLighting);

    m_saveProfileButton->setEnabled(connected && m_state.supportsProfiles
                                    && m_state.protocol == QStringLiteral("v1"));
    for (QAbstractButton *button : m_profileGroup->buttons()) {
        button->setEnabled(connected && m_state.supportsProfiles);
    }

    m_dpiFamilyCombo->setEnabled(connected);
    m_rawAddressEdit->setEnabled(connected);
    m_rawLengthSpin->setEnabled(connected);
    m_rawReadButton->setEnabled(connected);
    m_rawWriteButton->setEnabled(connected && m_state.protocol == QStringLiteral("v1"));
}

void MainWindow::updateBatteryUi(const DeviceState &state)
{
    if (state.modelName.isEmpty()) {
        m_headerBattery->setText(QStringLiteral("No device"));
        m_batteryBar->setValue(0);
        m_batteryText->setText(QStringLiteral("No battery data"));
        m_voltageLabel->setText(QStringLiteral("Connect a mouse to read battery state."));
        updateTrayIcon(state);
        writeBatteryCache(state);
        m_batteryService->clear();
        return;
    }

    if (!state.battery) {
        m_headerBattery->setText(QStringLiteral("Battery \u2014"));
        m_batteryBar->setValue(0);
        m_batteryText->setText(QStringLiteral("Battery not reported"));
        m_voltageLabel->setText(QStringLiteral("Move the mouse to wake it, then press Refresh."));
    } else {
        const int percent = *state.battery;
        const bool charging = state.charging.value_or(false);
        const QString suffix = charging ? QStringLiteral(" \u00b7 charging") : QString();
        const QString batteryText = QStringLiteral("%1%").arg(percent) + suffix;
        m_headerBattery->setText(batteryText);
        m_batteryBar->setValue(percent);
        m_batteryText->setText(batteryText);
        m_voltageLabel->setText(state.voltageMv
                                    ? QStringLiteral("%1 mV").arg(*state.voltageMv)
                                    : QStringLiteral("Voltage not reported"));
    }

    updateTrayIcon(state);
    writeBatteryCache(state);
    m_batteryService->update(state);
}

void MainWindow::updateTrayIcon(const DeviceState &state)
{
    if (!m_tray) {
        return;
    }
    if (state.modelName.isEmpty()) {
        m_tray->setIcon(makeAppIcon());
        m_tray->setToolTip(QStringLiteral("ATK Control \u00b7 no device"));
        return;
    }
    if (!state.battery) {
        m_tray->setIcon(makeAppIcon());
        m_tray->setToolTip(QStringLiteral("%1 \u00b7 battery not reported").arg(state.modelName));
        return;
    }
    const int percent = *state.battery;
    const bool charging = state.charging.value_or(false);
    m_tray->setIcon(makeBatteryIcon(percent, charging));
    m_tray->setToolTip(QStringLiteral("%1 \u00b7 %2%")
                           .arg(state.modelName)
                           .arg(percent)
                       + (charging ? QStringLiteral(" (charging)") : QString()));
}

void MainWindow::applyDpi()
{
    if (!m_hasDevice || m_dpiTable->rowCount() == 0) {
        return;
    }
    QVector<QPair<int, int>> stages;
    QVector<QString> colors;
    int active = -1;

    for (int row = 0; row < m_dpiTable->rowCount(); ++row) {
        auto *xItem = m_dpiTable->item(row, 2);
        auto *yItem = m_dpiTable->item(row, 3);
        if (!xItem || !yItem) {
            continue;
        }
        bool okX = false;
        bool okY = false;
        const int x = xItem->text().toInt(&okX);
        const int y = yItem->text().toInt(&okY);
        if (!okX || !okY) {
            QMessageBox::warning(this, QStringLiteral("Invalid DPI"),
                                 QStringLiteral("DPI values must be whole numbers."));
            return;
        }
        stages.append({x, y});
        auto *colorItem = m_dpiTable->item(row, 4);
        colors.append(colorItem ? colorItem->text() : QStringLiteral("#ffffff"));
        auto *activeItem = m_dpiTable->item(row, 0);
        if (activeItem && activeItem->checkState() == Qt::Checked) {
            active = row;
        }
    }

    if (stages.isEmpty()) {
        return;
    }
    if (active < 0) {
        active = m_state.activeStage;
    }

    invokeWorker([stages, active, colors](DeviceWorker *worker) {
        worker->applyDpi(stages, active, colors);
    });
}

void MainWindow::applyPerformance()
{
    if (!m_hasDevice) {
        return;
    }
    int polling = m_pollingCombo->currentData().toInt();
    if (polling < 0) {
        polling = m_state.pollingRate;
    }
    const int lod = m_lodCombo->currentData().toInt();
    const int debounce = m_debounceCombo->currentData().toInt();
    const int sleep = m_sleepCombo->currentData().toInt();
    const int sensorMode = m_sensorModeCombo->currentData().toInt();
    const bool motionSync = m_motionSyncCheck->isChecked();
    const bool ripple = m_rippleCheck->isChecked();
    const bool straightLine = m_straightLineCheck->isChecked();
    const int angle = m_angleSpin->value();
    const bool angleEnabled = m_angleEnableCheck->isChecked();

    invokeWorker([=](DeviceWorker *worker) {
        worker->applyPerformance(polling, lod, debounce, motionSync, ripple, straightLine,
                                 sensorMode, sleep, angle, angleEnabled);
    });
}

void MainWindow::applyButtons()
{
    if (!m_hasDevice) {
        return;
    }
    QVector<QString> actions;
    for (int row = 0; row < m_buttonsTable->rowCount(); ++row) {
        auto *combo = qobject_cast<QComboBox *>(m_buttonsTable->cellWidget(row, 1));
        const bool readable = combo && combo->property("readable").toBool();
        actions.append(readable ? combo->currentData().toString() : QString());
    }
    invokeWorker([actions](DeviceWorker *worker) { worker->applyButtons(actions); });
}

void MainWindow::applyLighting()
{
    if (!m_hasDevice) {
        return;
    }
    const int mode = m_ledModeCombo->currentData().toInt();
    const int brightness = m_ledBrightnessCombo->currentData().toInt();
    const int speed = m_ledSpeedCombo->currentData().toInt();
    invokeWorker([=](DeviceWorker *worker) { worker->setLedMode(mode, brightness, speed); });
}

void MainWindow::activateProfile(int profile)
{
    if (!m_hasDevice) {
        return;
    }
    invokeWorker([profile](DeviceWorker *worker) { worker->setProfile(profile); });
}

void MainWindow::saveProfile()
{
    invokeWorker([](DeviceWorker *worker) { worker->commitSettings(); });
}

void MainWindow::readRaw()
{
    if (!m_hasDevice) {
        return;
    }
    QString text = m_rawAddressEdit->text().trimmed();
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        text = text.mid(2);
    }
    bool ok = false;
    const int address = text.toInt(&ok, 16);
    if (!ok || address < 0 || address > 0xFFFF) {
        QMessageBox::warning(this, QStringLiteral("Invalid address"),
                             QStringLiteral("Enter a hexadecimal address between 0x0000 and 0xFFFF."));
        return;
    }
    const int length = m_rawLengthSpin->value();
    invokeWorker([address, length](DeviceWorker *worker) {
        worker->rawRead(static_cast<uint16_t>(address), length);
    });
}

void MainWindow::writeRaw()
{
    if (!m_hasDevice || m_state.protocol != QStringLiteral("v1")) {
        return;
    }
    QString text = m_rawDataEdit->toPlainText();
    text.remove(QRegularExpression(QStringLiteral("0x"), QRegularExpression::CaseInsensitiveOption));
    text.remove(QRegularExpression(QStringLiteral("[\\s,;:]")));
    const QByteArray data = QByteArray::fromHex(text.toLatin1());
    if (data.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid data"),
                             QStringLiteral("Enter one or more hexadecimal bytes."));
        return;
    }
    if (data.size() > 10) {
        QMessageBox::warning(this, QStringLiteral("Too much data"),
                             QStringLiteral("Protocol v1 writes at most ten bytes at a time."));
        return;
    }

    QString addressText = m_rawAddressEdit->text().trimmed();
    if (addressText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        addressText = addressText.mid(2);
    }
    bool ok = false;
    const int address = addressText.toInt(&ok, 16);
    if (!ok || address < 0 || address > 0xFFFF) {
        QMessageBox::warning(this, QStringLiteral("Invalid address"),
                             QStringLiteral("Enter a hexadecimal address between 0x0000 and 0xFFFF."));
        return;
    }

    const auto answer = QMessageBox::question(
        this, QStringLiteral("Raw write"),
        QStringLiteral("Write %1 at 0x%2? This changes undocumented settings.")
            .arg(hexDump(data))
            .arg(address, 4, 16, QLatin1Char('0')),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    invokeWorker([address, data](DeviceWorker *worker) {
        worker->rawWrite(static_cast<uint16_t>(address), data);
    });
}

void MainWindow::installUdev()
{
    QFile rule(QStringLiteral(":/udev/71-atk-control.rules"));
    if (!rule.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("udev rule"),
                             QStringLiteral("The bundled udev rule could not be read."));
        return;
    }
    const QString temporary = QDir::tempPath() + QStringLiteral("/71-atk-control.rules");
    QFile output(temporary);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("udev rule"),
                             QStringLiteral("Could not write a temporary rule file."));
        return;
    }
    output.write(rule.readAll());
    output.close();

    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    const QString install = QStandardPaths::findExecutable(QStringLiteral("install"));
    const QString udevadm = QStandardPaths::findExecutable(QStringLiteral("udevadm"));
    const QString manual = QStringLiteral(
        "sudo install -m 0644 %1 /etc/udev/rules.d/71-atk-control.rules\n"
        "sudo udevadm control --reload-rules\n"
        "sudo udevadm trigger --subsystem-match=hidraw")
                               .arg(temporary);

    if (pkexec.isEmpty() || install.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("udev rule"),
                                 QStringLiteral("pkexec or install was not found. Run:\n\n%1")
                                     .arg(manual));
        return;
    }

    const auto answer = QMessageBox::question(
        this, QStringLiteral("Install udev rule"),
        QStringLiteral("Install the ATK/VXE hidraw rule with administrator privileges?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const int result = QProcess::execute(
        pkexec, {install, QStringLiteral("-m"), QStringLiteral("0644"), temporary,
                 QStringLiteral("/etc/udev/rules.d/71-atk-control.rules")});
    if (result != 0) {
        QMessageBox::warning(this, QStringLiteral("udev rule"),
                             QStringLiteral("Installation failed. Run:\n\n%1").arg(manual));
        return;
    }

    if (!udevadm.isEmpty()) {
        QProcess::execute(pkexec, {udevadm, QStringLiteral("control"), QStringLiteral("--reload-rules")});
        QProcess::execute(pkexec, {udevadm, QStringLiteral("trigger"),
                                   QStringLiteral("--subsystem-match=hidraw")});
    }
    appendLog(QStringLiteral("Installed udev rule."));
    QMessageBox::information(this, QStringLiteral("udev rule"),
                             QStringLiteral("Installed. Reconnect the mouse, then press Refresh."));
}

void MainWindow::updateDpiFamily()
{
    if (!m_hasDevice) {
        return;
    }
    const QString family = m_dpiFamilyCombo->currentData().toString();
    invokeWorker([family](DeviceWorker *worker) { worker->setDpiFamily(family); });
}

void MainWindow::onDpiItemChanged(QTableWidgetItem *item)
{
    if (!item || item->column() != 0 || item->checkState() != Qt::Checked) {
        return;
    }
    QSignalBlocker blocker(m_dpiTable);
    for (int row = 0; row < m_dpiTable->rowCount(); ++row) {
        if (row == item->row()) {
            continue;
        }
        if (auto *other = m_dpiTable->item(row, 0)) {
            other->setCheckState(Qt::Unchecked);
        }
    }
}

void MainWindow::onPollIntervalChanged(int seconds)
{
    m_pollIntervalSec = seconds;
    if (m_batteryTimer) {
        m_batteryTimer->setInterval(seconds * 1000);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("pollInterval"), seconds);
}

void MainWindow::onTrayToggled(bool enabled)
{
    m_trayEnabled = enabled;
    if (m_tray) {
        m_tray->setVisible(enabled);
    }
    if (!enabled && !isVisible()) {
        showWindow();
    }
    QSettings settings;
    settings.setValue(QStringLiteral("trayEnabled"), enabled);
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            showWindow();
        }
    } else if (reason == QSystemTrayIcon::MiddleClick) {
        refreshDeviceList();
    }
}

void MainWindow::showWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::quitApplication()
{
    m_quitting = true;
    saveSettings();
    cleanupWorker();
    QCoreApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_quitting && m_trayEnabled && m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        if (!m_trayMessageShown) {
            m_tray->showMessage(QStringLiteral("ATK Control"),
                                QStringLiteral("Still running in the system tray."),
                                QSystemTrayIcon::Information, 3000);
            m_trayMessageShown = true;
        }
        return;
    }
    saveSettings();
    cleanupWorker();
    event->accept();
    QCoreApplication::quit();
}

void MainWindow::appendLog(const QString &message)
{
    if (!m_logEdit) {
        return;
    }
    m_logEdit->appendPlainText(
        QStringLiteral("%1  %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                     message));
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("pollInterval"), m_pollIntervalSec);
    settings.setValue(QStringLiteral("trayEnabled"), m_trayEnabled);
    settings.setValue(QStringLiteral("startHidden"), m_startHidden);
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("windowState"), saveState());
}

void MainWindow::loadSettings()
{
    QSettings settings;
    m_pollIntervalSec = settings.value(QStringLiteral("pollInterval"), 30).toInt();
    m_trayEnabled = settings.value(QStringLiteral("trayEnabled"), true).toBool();
    m_startHidden = settings.value(QStringLiteral("startHidden"), false).toBool();
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("windowState")).toByteArray());

    QSignalBlocker blockInterval(m_pollIntervalSpin);
    QSignalBlocker blockTray(m_trayCheck);
    QSignalBlocker blockHidden(m_startHiddenCheck);
    m_pollIntervalSpin->setValue(m_pollIntervalSec);
    m_trayCheck->setChecked(m_trayEnabled);
    m_startHiddenCheck->setChecked(m_startHidden);
}

void MainWindow::cleanupWorker()
{
    if (!m_workerThread) {
        return;
    }
    if (m_worker) {
        DeviceWorker *worker = m_worker;
        QMetaObject::invokeMethod(
            worker, [worker] { worker->closeDevice(); }, Qt::BlockingQueuedConnection);
    }
    m_workerThread->quit();
    m_workerThread->wait(3000);
    m_workerThread = nullptr;
    m_worker = nullptr;
}

QString MainWindow::batteryCacheDir() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.cache");
    }
    return base + QStringLiteral("/atk-control");
}

void MainWindow::writeBatteryCache(const DeviceState &state)
{
    const QString dir = batteryCacheDir();
    QDir().mkpath(dir);

    QJsonObject object;
    object.insert(QStringLiteral("connected"), !state.modelName.isEmpty());
    object.insert(QStringLiteral("percent"), state.battery.value_or(-1));
    object.insert(QStringLiteral("charging"), state.charging.value_or(false));
    object.insert(QStringLiteral("voltage_mv"), state.voltageMv.value_or(0));
    object.insert(QStringLiteral("model"), state.modelName);
    object.insert(QStringLiteral("updated"), QDateTime::currentSecsSinceEpoch());

    QSaveFile file(dir + QStringLiteral("/battery.json"));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.commit();
}

void MainWindow::clearBatteryCache()
{
    writeBatteryCache(DeviceState {});
}

QIcon MainWindow::makeAppIcon() const
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#16181d")));
    painter.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);

    painter.setBrush(QColor(QStringLiteral("#2b6cff")));
    painter.drawRoundedRect(QRectF(20, 14, 24, 38), 12, 12);
    painter.setBrush(QColor(QStringLiteral("#16181d")));
    painter.drawRoundedRect(QRectF(29, 20, 6, 14), 3, 3);
    painter.setPen(QPen(QColor(QStringLiteral("#8fb4ff")), 2));
    painter.drawLine(QPointF(32, 40), QPointF(32, 47));
    return QIcon(pixmap);
}

QIcon MainWindow::makeBatteryIcon(int percent, bool charging) const
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor outline(QStringLiteral("#d9dde6"));
    const QColor fill = charging ? QColor(QStringLiteral("#4f9cf9"))
                                 : (percent >= 60 ? QColor(QStringLiteral("#4caf50"))
                                                  : (percent >= 25 ? QColor(QStringLiteral("#ffb300"))
                                                                   : QColor(QStringLiteral("#e05a5a"))));

    painter.setPen(QPen(outline, 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(7, 19, 46, 26), 6, 6);
    painter.setPen(Qt::NoPen);
    painter.setBrush(outline);
    painter.drawRoundedRect(QRectF(54, 26, 5, 12), 2, 2);

    const int clamped = qBound(0, percent, 100);
    if (clamped > 0) {
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(11, 23, 38.0 * clamped / 100.0, 18), 3, 3);
    }

    if (charging) {
        QPolygonF bolt;
        bolt << QPointF(34, 16) << QPointF(24, 34) << QPointF(31, 34)
             << QPointF(28, 48) << QPointF(40, 28) << QPointF(33, 28);
        painter.setBrush(QColor(QStringLiteral("#ffffff")));
        painter.setPen(QPen(QColor(QStringLiteral("#1b1d22")), 1));
        painter.drawPolygon(bolt);
    }

    return QIcon(pixmap);
}
