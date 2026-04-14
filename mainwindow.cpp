


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
#include "device_poller.h"
#include "ModbusMaster.h"

// Глобальные указатели (определены в main.cpp)
extern DevicePoller* g_poller;
extern ModbusMaster* g_master;

// Глобальная переменная для хранения текущей скорости
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
{
    ui->setupUi(this);
    // Устанавливаем вкладку "Информация" как активную по умолчанию
    ui->tabWidget->setCurrentIndex(0);

    QWidget *infoTab = ui->Info;
    QWidget *statisticsTab = ui->Statistics;

    // Удаляем существующий layout, если есть
    delete infoTab->layout();
    delete statisticsTab->layout();

    // ========== Находим устройство реле (адрес 0x0A = 10) ==========
    m_relayDevice = dynamic_cast<RelayDevice*>(
        DeviceManager::instance().getDevice(0x0A)
    );

    if (!m_relayDevice) {
        qDebug() << "⚠️ Relay device with address 0x0A not found!";
    } else {
        qDebug() << "✅ Relay device found, can control relay 1";
    }

    // ========== 1. Левый индикатор (Мощность) ==========
    powerWidget = new QQuickWidget(infoTab);
    powerWidget->setFixedSize(120, 400);
    powerWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    QQmlContext *powerContext = powerWidget->rootContext();
    powerContext->setContextProperty("gaugeLabel", "Мощность");
    powerContext->setContextProperty("gaugeUnit", "Вт/Час");
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
    }

    // ========== 5. График для вкладки Статистика ==========
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
    updateTimer->start(100);  // Быстрый таймер для индикаторов

    // ========== Таймер для графика (медленный, отдельный) ==========
    QTimer *chartTimer = new QTimer(this);
    connect(chartTimer, &QTimer::timeout, this, &MainWindow::updateChart);
    chartTimer->start(1000);  // Интервал обновления графика

    qDebug() << "✅ All timers started";

    ui->tabWidget->tabBar()->setExpanding(true);
}

MainWindow::~MainWindow()
{
    // Безопасное удаление
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

    // Настраиваем свойства графика
    chartRoot->setProperty("chartTitle", "Динамика скорости маховика");
    chartRoot->setProperty("xAxisLabel", "Время (сек)");
    chartRoot->setProperty("yAxisLabel", "Скорость (об/мин)");
    chartRoot->setProperty("maxYValue", 10000);
    chartRoot->setProperty("minYValue", 0);
    chartRoot->setProperty("maxDataPoints", 200);
    chartRoot->setProperty("cyclicMode", true);

    // Получаем список доступных методов
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
    qDebug() << "   Available methods:" << m_chartData->availableMethods;

    // Проверяем наличие метода addDataPoint
    if (m_chartData->availableMethods.contains("addDataPoint")) {
        qDebug() << "✅ addDataPoint method found";
    } else {
        qDebug() << "⚠️ addDataPoint method NOT found!";
    }
}



void MainWindow::updateIndicators()
{
    QMutexLocker locker(&dataMutex);

    if (!powerButtonWidget || !powerButtonWidget->rootObject()) {
        return;
    }

    bool isOn = powerButtonWidget->rootObject()->property("isOn").toBool();

    if (isOn) {
        static int counter = 0;
        counter++;

        // Основные параметры
        currentSpeed = 3000 + (counter % 70) * 100;
        currentPower = (currentSpeed * 2.5);
        currentTorque = 5 + (counter % 15);

        // Вычисляемые параметры
        currentEfficiency = (currentSpeed * currentTorque * 100) / (currentPower * 100);
        if (currentEfficiency > 95) currentEfficiency = 95;

        currentEnergy = (currentPower * counter) / 3600;
        currentCalculatedTemp = 25.0 + (currentPower / 500.0);
        currentCalculatedCapacity = (currentTorque * counter) / 3600.0;
        if (currentCalculatedCapacity > 100) currentCalculatedCapacity = 100;
        currentCalculatedVoltage = 220.0 - (currentTorque * 0.5);
        if (currentCalculatedVoltage < 200) currentCalculatedVoltage = 200;

        updateSpeed(currentSpeed);
        updatePower(currentPower);
        updateTorque(currentTorque);

    } else {
        currentSpeed = 0;
        currentPower = 0;
        currentTorque = 0;
        currentEfficiency = 0;
        currentEnergy = 0;
        currentCalculatedTemp = 0;
        currentCalculatedCapacity = 0;
        currentCalculatedVoltage = 0;

        updateSpeed(0);
        updatePower(0);
        updateTorque(0);
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

    if (!isOn) {
        return;
    }

    if (!m_chartData->chartRoot) {
        return;
    }

    QMutexLocker locker(&dataMutex);

    // Отправляем все параметры (и получаемые, и вычисляемые)
    QMetaObject::invokeMethod(m_chartData->chartRoot, "addDataPoint",
        Qt::AutoConnection,
        Q_ARG(QVariant, QVariant("Обороты")),
        Q_ARG(QVariant, currentSpeed));

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

    qDebug() << "📊 Chart updated - Speed:" << currentSpeed
             << "Current:" << currentTorque
             << "Power:" << currentPower
             << "Efficiency:" << currentEfficiency
             << "Energy:" << currentEnergy
             << "Temp:" << currentCalculatedTemp;
}






void MainWindow::onPowerButtonToggled(bool state)
{
    qDebug() << "🔘 Кнопка переключена:" << (state ? "ВКЛ" : "ВЫКЛ");

    QMutexLocker locker(&dataMutex);

    if (m_relayDevice) {
        qDebug() << "📡 Отправляем команду на реле 1:" << (state ? "ON" : "OFF");
        QByteArray command = m_relayDevice->generateSetRelayCommand(1, state);

        if (!command.isEmpty() && g_poller) {
            QString commandName = state ? "SetRelay1_ON" : "SetRelay1_OFF";
            g_poller->sendPriorityCommand(0x0A, command, commandName);
            qDebug() << "✅ Команда отправлена в очередь поллера";
        } else if (!command.isEmpty() && g_master) {
            qDebug() << "⚠️ Poller not available, sending directly via ModbusMaster";
            g_master->sendRawData(0x0A, command);
        } else {
            qDebug() << "❌ Failed to send command";
        }
    } else {
        qDebug() << "❌ Relay device not available!";
    }

    if (state) {
        qDebug() << "✅ Система активирована, реле 1 включено";
        // Сбрасываем счетчики при включении
        currentSpeed = 0;
        currentPower = 0;
        currentTorque = 0;
    } else {
        qDebug() << "✅ Система деактивирована, реле 1 выключено";
        // При выключении очищаем график
        if (m_chartData && m_chartData->chartRoot && m_chartData->availableMethods.contains("clearChart")) {
            QMetaObject::invokeMethod(m_chartData->chartRoot, "clearChart", Qt::AutoConnection);
            qDebug() << "🧹 Chart cleared";
        }
    }
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


