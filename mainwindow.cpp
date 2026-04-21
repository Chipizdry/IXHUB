


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QQuickWidget>
#include <QQmlContext>
#include <QUrl>
#include <QTimer>
#include <QTabBar>
#include <QQuickItem>
#include <QDebug>
#include <QMetaMethod>
#include <QMutex>
#include "device_manager.h"
#include "relay_device.h"
#include "bldc_driver_device.h"
#include "device_poller.h"
#include "ModbusMaster.h"

// Глобальные указатели (определены в main.cpp)
extern DevicePoller* g_poller;
extern ModbusMaster* g_master;

// Глобальная переменная для хранения текущей скорости (из драйвера)
static int currentSpeed = 0;
static int currentPower = 0;
static int currentTorque = 0;
static int currentEfficiency = 0;
static int currentEnergy = 0;
static float currentCalculatedTemp = 0;
static float currentCalculatedCapacity = 0;
static float currentCalculatedVoltage = 0;

// Мьютекс для потокобезопасности
static QMutex dataMutex;

// Структура для хранения данных графиков
struct ChartData {
    QObject* chartRoot;
    QTimer* timer;
    bool isInitialized;
    QStringList availableMethods;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_chartData(nullptr)
    , m_startupTimer(nullptr)
    , m_statusCheckTimer(nullptr)
    , m_isStarting(false)
    , m_currentStep(0)
    , m_bldcDevice(nullptr)
    , pwmControlWidget(nullptr)
    , targetPwm(0)
    , currentPwm(0)

{
    ui->setupUi(this);
    // Устанавливаем вкладку "Информация" как активную по умолчанию
    ui->tabWidget->setCurrentIndex(0);

    QWidget *infoTab = ui->Info;
    QWidget *statisticsTab = ui->Statistics;

    // Удаляем существующий layout, если есть
    delete infoTab->layout();
    delete statisticsTab->layout();

    // ========== Находим устройство реле (адрес 1) ==========
    m_relayDevice = dynamic_cast<RelayDevice*>(
        DeviceManager::instance().getDevice(1)
    );

    if (!m_relayDevice) {
        qDebug() << "⚠️ Relay device with address 1 not found!";
    } else {
        qDebug() << "✅ Relay device found";
        connect(m_relayDevice, &RelayDevice::relayStateChangedByName,
                this, &MainWindow::onRelayStateChanged);
        connect(m_relayDevice, &RelayDevice::commandGenerated,
                this, &MainWindow::onRelayCommandGenerated);
    }

    // ========== Находим BLDC драйвер (адрес 2) ==========
    m_bldcDevice = dynamic_cast<BldcDriverDevice*>(
        DeviceManager::instance().getDevice(2)
    );

    if (!m_bldcDevice) {
        qDebug() << "⚠️ BLDC driver with address 2 not found!";
    } else {
        qDebug() << "✅ BLDC driver found";
        // Подключаемся к сигналу обновления данных от драйвера
        connect(m_bldcDevice, &BldcDriverDevice::dataUpdated,
                this, &MainWindow::onBldcDataUpdated);
        // Также подключаем сигнал команд
        connect(m_bldcDevice, &BldcDriverDevice::commandGenerated,
                this, &MainWindow::onBldcCommandGenerated);
    }

    // ========== Инициализация таймеров ==========
    m_startupTimer = new QTimer(this);
    m_startupTimer->setSingleShot(true);
    connect(m_startupTimer, &QTimer::timeout, this, &MainWindow::onStartupTimerTimeout);

    m_statusCheckTimer = new QTimer(this);
    m_statusCheckTimer->setSingleShot(true);
    connect(m_statusCheckTimer, &QTimer::timeout, this, &MainWindow::checkRelayStatus);

    // ========== 1. Левый индикатор (Мощность) ==========
    powerWidget = new QQuickWidget(infoTab);
    powerWidget->setFixedSize(120, 400);
    powerWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    QQmlContext *powerContext = powerWidget->rootContext();
    powerContext->setContextProperty("gaugeLabel", "Мощность");
    powerContext->setContextProperty("gaugeUnit", "Вт");
    powerContext->setContextProperty("gaugeMinValue", 0);
    powerContext->setContextProperty("gaugeMaxValue", 10000);

    powerWidget->setSource(QUrl("qrc:/qml/VerticalGauge.qml"));
    powerWidget->move(20, 20);

    // ========== 2. Центральный спидометр ==========
    speedWidget = new QQuickWidget(infoTab);
    speedWidget->setFixedSize(420, 420);
    speedWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    speedWidget->setSource(QUrl("qrc:/qml/Speedometer.qml"));
    speedWidget->move(190, 20);

    // ========== 3. Правый индикатор (Ток) ==========
    torqueWidget = new QQuickWidget(infoTab);
    torqueWidget->setFixedSize(120, 400);
    torqueWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    QQmlContext *torqueContext = torqueWidget->rootContext();
    torqueContext->setContextProperty("gaugeLabel", "Ток");
    torqueContext->setContextProperty("gaugeUnit", "А");
    torqueContext->setContextProperty("gaugeMinValue", 0);
    torqueContext->setContextProperty("gaugeMaxValue", 20);

    torqueWidget->setSource(QUrl("qrc:/qml/VerticalGauge.qml"));
    torqueWidget->move(650, 20);

    // ========== 4. Кнопка включения/выключения ==========
    powerButtonWidget = new QQuickWidget(infoTab);
    powerButtonWidget->setFixedSize(80, 80);
    powerButtonWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    powerButtonWidget->setSource(QUrl("qrc:/qml/PowerButton.qml"));
    powerButtonWidget->move(920, 400);

    QObject *buttonRoot = powerButtonWidget->rootObject();
    if (buttonRoot) {
        connect(buttonRoot, SIGNAL(toggled(bool)),
                this, SLOT(onPowerButtonToggled(bool)));
        buttonRoot->setProperty("isOn", false);
        buttonRoot->setProperty("isBlinking", false);
    }

    // ========== 5. Контроллер ШИМ ==========
    pwmControlWidget = new QQuickWidget(infoTab);
    pwmControlWidget->setFixedSize(250, 80);
    pwmControlWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    QQmlContext *pwmControlContext = pwmControlWidget->rootContext();
    pwmControlContext->setContextProperty("label", "ШИМ сигнал");
    pwmControlContext->setContextProperty("unit", "%");
    pwmControlContext->setContextProperty("minValue", 0);
    pwmControlContext->setContextProperty("maxValue", 100);
    pwmControlContext->setContextProperty("step", 5);
    pwmControlContext->setContextProperty("targetValue", 0);
    pwmControlContext->setContextProperty("currentValue", 0);

    pwmControlWidget->setSource(QUrl("qrc:/qml/ValueControl.qml"));
    pwmControlWidget->move(190, 450);
    pwmControlWidget->setVisible(true);  // Явно делаем видимым
   // pwmControlWidget->raise();           // Поднимаем наверх

    // Отладочная информация
    qDebug() << "=== PWM Widget Info ===";
    qDebug() << "  Size:" << pwmControlWidget->size();
    qDebug() << "  Position:" << pwmControlWidget->pos();
    qDebug() << "  Is visible:" << pwmControlWidget->isVisible();
    qDebug() << "  Parent size:" << infoTab->size();
    qDebug() << "  Parent geometry:" << infoTab->geometry();



    // ========== 6. График для вкладки Статистика ==========
    m_chartWidget = new QQuickWidget(statisticsTab);
    m_chartWidget->setFixedSize(1020, 510);
    m_chartWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_chartWidget->setSource(QUrl("qrc:/qml/ChartView.qml"));
    m_chartWidget->move(0, 0);

    // Инициализация структуры данных для графика
    m_chartData = new ChartData();
    m_chartData->chartRoot = nullptr;
    m_chartData->timer = nullptr;
    m_chartData->isInitialized = false;

    // Отложенная инициализация графика
    QTimer::singleShot(500, this, &MainWindow::initializeChart);

    // ========== Таймер для обновления индикаторов (быстрый) ==========
    QTimer *updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateIndicators);
    updateTimer->start(100);

    // ========== Таймер для графика (медленный, отдельный) ==========
    QTimer *chartTimer = new QTimer(this);
    connect(chartTimer, &QTimer::timeout, this, &MainWindow::updateChart);
    chartTimer->start(1000);

    qDebug() << "✅ All timers started";

    ui->tabWidget->tabBar()->setExpanding(true);
}

MainWindow::~MainWindow()
{
    if (m_startupTimer) m_startupTimer->stop();
    if (m_statusCheckTimer) m_statusCheckTimer->stop();

    if (m_chartData) {
        if (m_chartData->timer) {
            m_chartData->timer->stop();
            delete m_chartData->timer;
        }
        delete m_chartData;
    }
    delete ui;
}

void MainWindow::initializeChart()
{
    if (!m_chartWidget) {
        qDebug() << "❌ Chart widget is null";
        return;
    }

    QObject *chartRoot = m_chartWidget->rootObject();
    if (!chartRoot) {
        qDebug() << "❌ Chart root object is null, retrying...";
        QTimer::singleShot(500, this, &MainWindow::initializeChart);
        return;
    }

    m_chartData->chartRoot = chartRoot;

    chartRoot->setProperty("xAxisLabel", "Время (сек)");
    chartRoot->setProperty("yAxisLabel", "Скорость (об/мин)");
    chartRoot->setProperty("maxYValue", 10000);
    chartRoot->setProperty("minYValue", 0);
    chartRoot->setProperty("maxDataPoints", 200);
    chartRoot->setProperty("cyclicMode", true);

    const QMetaObject *metaObj = chartRoot->metaObject();
    for (int i = 0; i < metaObj->methodCount(); i++) {
        QMetaMethod method = metaObj->method(i);
        if (method.methodType() == QMetaMethod::Method) {
            m_chartData->availableMethods << method.name();
            qDebug() << "  📌 Method:" << method.name();
        }
    }

    m_chartData->isInitialized = true;
    qDebug() << "✅ Chart initialized successfully";

    if (m_chartData->availableMethods.contains("addDataPoint")) {
        qDebug() << "✅ addDataPoint method found";
    } else {
        qDebug() << "⚠️ addDataPoint method NOT found!";
    }
}

// ==================== УПРАВЛЕНИЕ КНОПКОЙ ====================

void MainWindow::setButtonBlinking(bool blinking, bool finalState)
{
    if (!powerButtonWidget || !powerButtonWidget->rootObject()) {
        return;
    }

    QObject *buttonRoot = powerButtonWidget->rootObject();

    if (blinking) {
        buttonRoot->setProperty("isBlinking", true);
        qDebug() << "🔴🟢 Button blinking started";
    } else {
        buttonRoot->setProperty("isBlinking", false);
        buttonRoot->setProperty("isOn", finalState);
        qDebug() << "Button blinking stopped, final state:" << (finalState ? "ON" : "OFF");
    }
}

// ==================== ОБРАБОТКА КОМАНД РЕЛЕ ====================

void MainWindow::onRelayCommandGenerated(const QByteArray& command)
{
    qDebug() << "📤 Relay command generated, sending to poller...";

    if (g_poller) {
        g_poller->sendPriorityCommand(1, command, "RelayCommand");
        qDebug() << "✅ Command sent to poller";
    } else if (g_master) {
        g_master->sendRawData(1, command);
        qDebug() << "✅ Command sent directly to Modbus";
    } else {
        qDebug() << "❌ No Modbus master available!";
    }
}

void MainWindow::sendRelayCommand(const QString& relayName, bool state)
{
    if (!m_relayDevice) {
        qDebug() << "❌ Relay device not available!";
        return;
    }

    qDebug() << "📡 Sending relay command:" << relayName << "->" << (state ? "ON" : "OFF");
    m_relayDevice->setRelayByName(relayName, state);
}

// ==================== ОБРАБОТКА КОМАНД BLDC ====================

void MainWindow::onBldcCommandGenerated(const QByteArray& command)
{
    qDebug() << "📤 BLDC command generated, sending to poller...";

    if (g_poller) {
        g_poller->sendPriorityCommand(2, command, "BLDCCommand");
        qDebug() << "✅ BLDC command sent to poller";
    } else if (g_master) {
        g_master->sendRawData(2, command);
        qDebug() << "✅ BLDC command sent directly to Modbus";
    } else {
        qDebug() << "❌ No Modbus master available!";
    }
}


void MainWindow::onBldcDataUpdated()
{
    if (!m_bldcDevice) return;

    QMutexLocker locker(&dataMutex);

    // Получаем данные из драйвера
    currentSpeed = m_bldcDevice->getRpm();

    // Получаем текущее значение ШИМ (0-1000) и конвертируем в проценты (0-100)
    int pwmRaw = m_bldcDevice->getPwmValue();
    int pwmPercent = (pwmRaw * 100) / 1000;  // Масштабируем 0-1000 -> 0-100

    // Обновляем контроллер ШИМ
    updatePwm(pwmPercent);

    // Рассчитываем мощность
    currentPower = currentSpeed * 2.5;
    if (currentPower > 10000) currentPower = 10000;

    // Рассчитываем ток (А) = мощность / напряжение
    currentTorque = currentPower / 220;
    if (currentTorque > 20) currentTorque = 20;

    // КПД (упрощенный расчет)
    if (currentSpeed > 0) {
        currentEfficiency = 75 + (currentSpeed / 100);
        if (currentEfficiency > 95) currentEfficiency = 95;
    } else {
        currentEfficiency = 0;
    }

    // Накопленная энергия (Вт*ч)
    static QElapsedTimer energyTimer;
    if (!energyTimer.isValid()) {
        energyTimer.start();
    } else {
        qint64 elapsedMs = energyTimer.restart();
        double hours = elapsedMs / 3600000.0;
        currentEnergy += currentPower * hours;
        if (currentEnergy > 100000) currentEnergy = 100000;
    }

    // Расчетная температура
    currentCalculatedTemp = 25.0 + (currentPower / 500.0);
    if (currentCalculatedTemp > 80) currentCalculatedTemp = 80;

    // Расчетная емкость
    currentCalculatedCapacity = (currentTorque * (currentSpeed / 1000)) / 10;
    if (currentCalculatedCapacity > 100) currentCalculatedCapacity = 100;

    // Расчетное напряжение
    currentCalculatedVoltage = 220.0 - (currentTorque * 0.5);
    if (currentCalculatedVoltage < 200) currentCalculatedVoltage = 200;

    qDebug() << "🔄 BLDC Data Updated - RPM:" << currentSpeed
             << "Power:" << currentPower
             << "Current:" << currentTorque
             << "PWM:" << pwmPercent << "% (raw:" << pwmRaw << ")";

    // Обновляем спидометр и индикаторы
    updateSpeed(currentSpeed);
    updatePower(currentPower);
    updateTorque(currentTorque);
}



// ==================== ПОСЛЕДОВАТЕЛЬНОСТЬ ВКЛЮЧЕНИЯ ====================

void MainWindow::startRelaySequence()
{
    qDebug() << "🚀 Starting relay sequence...";
    m_isStarting = true;
    m_currentStep = 0;

    setButtonBlinking(true, false);
    sendRelayCommand("Ballast_Resistor", true);
    m_startupTimer->start(3000);
}

void MainWindow::onStartupTimerTimeout()
{
    qDebug() << "⏰ Timer timeout, turning on Main Power...";
    sendRelayCommand("Main_Power", true);
    m_statusCheckTimer->start(500);
}

void MainWindow::checkRelayStatus()
{
    qDebug() << "🔍 Checking relay status...";

    if (!m_relayDevice) {
        qDebug() << "❌ Relay device not available!";
        setButtonBlinking(false, false);
        m_isStarting = false;
        return;
    }

    bool ballastOn = m_relayDevice->getRelayStateByName("Ballast_Resistor");
    bool mainPowerOn = m_relayDevice->getRelayStateByName("Main_Power");

    qDebug() << "  Ballast_Resistor:" << (ballastOn ? "ON" : "OFF");
    qDebug() << "  Main_Power:" << (mainPowerOn ? "ON" : "OFF");

    if (ballastOn && mainPowerOn) {
        qDebug() << "✅ Both relays are ON! Starting system...";
        setButtonBlinking(false, true);
        m_isStarting = false;

        // Включаем питание BLDC драйвера
        if (m_bldcDevice) {
            qDebug() << "🔌 Turning on BLDC driver power...";
            m_bldcDevice->setPowerOn(true);
            // Запрашиваем чтение данных с драйвера
            m_bldcDevice->readAllRegisters();
        }

        qDebug() << "✅ System activated successfully";
    } else {
        qDebug() << "⏳ Waiting for relays... Retrying in 1 second";
        m_statusCheckTimer->start(1000);
    }
}

void MainWindow::onRelayStateChanged(const QString& relayName, bool state)
{
    qDebug() << "📢 Relay state changed:" << relayName << "->" << (state ? "ON" : "OFF");

    if (m_isStarting && m_relayDevice) {
        bool ballastOn = m_relayDevice->getRelayStateByName("Ballast_Resistor");
        bool mainPowerOn = m_relayDevice->getRelayStateByName("Main_Power");

        if (ballastOn && mainPowerOn) {
            qDebug() << "✅ Both relays detected ON via signal!";
            setButtonBlinking(false, true);
            m_isStarting = false;
            if (m_statusCheckTimer) m_statusCheckTimer->stop();
            if (m_startupTimer) m_startupTimer->stop();

            // Включаем питание BLDC драйвера
            if (m_bldcDevice) {
                m_bldcDevice->setPowerOn(true);
                m_bldcDevice->readAllRegisters();
            }
        }
    }
}



void MainWindow::onPwmTargetChanged(qreal value)
{
    targetPwm = static_cast<int>(value);
    qDebug() << "🎯 Target PWM changed to:" << targetPwm << "%";

    // Проверяем, включена ли система
    if (!powerButtonWidget || !powerButtonWidget->rootObject()) {
        qDebug() << "⚠️ Power button widget not available";
        return;
    }

    bool isOn = powerButtonWidget->rootObject()->property("isOn").toBool();
    bool isBlinking = powerButtonWidget->rootObject()->property("isBlinking").toBool();

    if (isOn && !isBlinking) {
        // Отправляем команду на BLDC драйвер для изменения ШИМ
        if (m_bldcDevice) {
            // Конвертируем проценты (0-100) в значение драйвера (0-1000)
            int pwmValue = (targetPwm * 1000) / 100;
            m_bldcDevice->setPwmValue(pwmValue);
            qDebug() << "📤 PWM command sent to driver:" << pwmValue << "(raw) /" << targetPwm << "%";
        } else {
            qDebug() << "⚠️ BLDC driver not available";
        }
    } else {
        qDebug() << "⚠️ System is OFF or starting, PWM command not sent";
    }
}


void MainWindow::updatePwm(int value)
{
    currentPwm = value;

    if (pwmControlWidget && pwmControlWidget->rootObject()) {
        pwmControlWidget->rootObject()->setProperty("currentValue", value);

        // Если целевое значение совпадает с текущим, можно сбросить целевой режим
        // (это произойдет автоматически в QML)
    }
}




// ==================== ОБРАБОТЧИК КНОПКИ ====================

void MainWindow::onPowerButtonToggled(bool state)
{
    qDebug() << "🔘 Button toggled:" << (state ? "ON" : "OFF");

    if (state) {
        if (!m_isStarting) {
            startRelaySequence();
        } else {
            qDebug() << "⚠️ System is already starting, please wait...";
            if (powerButtonWidget && powerButtonWidget->rootObject()) {
                powerButtonWidget->rootObject()->setProperty("isOn", false);
            }
        }
    } else {
        qDebug() << "🛑 Shutting down system immediately...";

        if (m_startupTimer) m_startupTimer->stop();
        if (m_statusCheckTimer) m_statusCheckTimer->stop();

        // Отключаем BLDC драйвер
        if (m_bldcDevice) {
            m_bldcDevice->setPowerOn(false);
        }

        // Отключаем реле
        if (m_relayDevice) {
            sendRelayCommand("Ballast_Resistor", false);
            sendRelayCommand("Main_Power", false);
        }

        setButtonBlinking(false, false);
        m_isStarting = false;

        if (m_chartData && m_chartData->chartRoot && m_chartData->availableMethods.contains("clearChart")) {
            QMetaObject::invokeMethod(m_chartData->chartRoot, "clearChart", Qt::AutoConnection);
            qDebug() << "🧹 Chart cleared";
        }

        // Обнуляем показатели
        {
            QMutexLocker locker(&dataMutex);
            currentSpeed = 0;
            currentPower = 0;
            currentTorque = 0;
            currentEfficiency = 0;
            currentEnergy = 0;
            currentCalculatedTemp = 0;
            currentCalculatedCapacity = 0;
            currentCalculatedVoltage = 0;
        }

        updateSpeed(0);
        updatePower(0);
        updateTorque(0);

        qDebug() << "✅ System deactivated";
    }
}

// ==================== ОБНОВЛЕНИЕ ИНДИКАТОРОВ ====================

void MainWindow::updateIndicators()
{
    QMutexLocker locker(&dataMutex);

    if (!powerButtonWidget || !powerButtonWidget->rootObject()) {
        return;
    }

    bool isOn = powerButtonWidget->rootObject()->property("isOn").toBool();
    bool isBlinking = powerButtonWidget->rootObject()->property("isBlinking").toBool();

    // Индикаторы обновляются из данных драйвера в onBldcDataUpdated()
    // Здесь только обновляем дополнительные параметры
    if (isOn && !isBlinking) {
        // Дополнительные параметры уже обновлены в onBldcDataUpdated()
        // Обновляем только если нужно что-то дополнительное
    }
}

void MainWindow::updateChart()
{
    if (!m_chartData || !m_chartData->isInitialized) {
        return;
    }

    if (!powerButtonWidget || !powerButtonWidget->rootObject()) {
        return;
    }

    bool isOn = powerButtonWidget->rootObject()->property("isOn").toBool();
    bool isBlinking = powerButtonWidget->rootObject()->property("isBlinking").toBool();

    if (!isOn || isBlinking) {
        return;
    }

    if (!m_chartData->chartRoot) {
        return;
    }

    QMutexLocker locker(&dataMutex);

    // Отправляем реальные данные с драйвера
    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Обороты")),
        Q_ARG(QVariant, currentSpeed));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("ШИМ")),  // Добавляем ШИМ на график
        Q_ARG(QVariant, currentPwm));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Ток")),
        Q_ARG(QVariant, currentTorque));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Мощность")),
        Q_ARG(QVariant, currentPower));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("КПД")),
        Q_ARG(QVariant, currentEfficiency));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Энергия")),
        Q_ARG(QVariant, currentEnergy));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Температура")),
        Q_ARG(QVariant, currentCalculatedTemp));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Ёмкость")),
        Q_ARG(QVariant, currentCalculatedCapacity));

    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Напряжение")),
        Q_ARG(QVariant, currentCalculatedVoltage));
}

void MainWindow::updateSpeed(int value)
{
    if (speedWidget && speedWidget->rootObject()) {
        speedWidget->rootObject()->setProperty("speedValue", value);
    }
}

void MainWindow::updatePower(int value)
{
    if (powerWidget && powerWidget->rootObject()) {
        powerWidget->rootObject()->setProperty("value", value);
    }
}

void MainWindow::updateTorque(int value)
{
    if (torqueWidget && torqueWidget->rootObject()) {
        torqueWidget->rootObject()->setProperty("value", value);
    }
}


