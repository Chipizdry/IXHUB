


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
#include "device_manager.h"
#include "relay_device.h"
#include "device_poller.h"
#include "ModbusMaster.h"

// Глобальные указатели (определены в main.cpp)
extern DevicePoller* g_poller;
extern ModbusMaster* g_master;

// Глобальная переменная для хранения текущей скорости
static int currentSpeed = 0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
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
    QQuickWidget *chartWidget = new QQuickWidget(statisticsTab);
    chartWidget->setFixedSize(1020, 510);
    chartWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    chartWidget->setSource(QUrl("qrc:/qml/ChartView.qml"));

    // Размещаем график по координатам (x, y)
    chartWidget->move(0, 0);

    // Даем время на загрузку QML
    QTimer::singleShot(500, [this, chartWidget]() {
        // Настраиваем свойства графика
        QObject *chartRoot = chartWidget->rootObject();
        if (chartRoot) {
            chartRoot->setProperty("chartTitle", "Динамика скорости маховика");
            chartRoot->setProperty("xAxisLabel", "Время (сек)");
            chartRoot->setProperty("yAxisLabel", "Скорость (об/мин)");
            chartRoot->setProperty("maxYValue", 10000);
            chartRoot->setProperty("minYValue", 0);
            chartRoot->setProperty("maxDataPoints", 200);
            chartRoot->setProperty("cyclicMode", true);

            qDebug() << "✅ Chart configured successfully";
        } else {
            qDebug() << "❌ Failed to get chart root object";
        }
    });

    // ========== Таймер для обновления индикаторов (быстрый) ==========
    QTimer *updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, [this]() {
        // Проверяем состояние кнопки
        if (powerButtonWidget && powerButtonWidget->rootObject()) {
            bool isOn = powerButtonWidget->rootObject()->property("isOn").toBool();

            if (isOn) {
                // Только если включено - обновляем данные
                static int speed = 0;
                static int power = 0;
                static int torque = 0;

                speed += 50;
                power += 50;
                torque += 1;

                if (speed > 10000) speed = 0;
                if (power > 10000) power = 0;
                if (torque > 20) torque = 0;

                updateSpeed(speed);
                updatePower(power);
                updateTorque(torque);

                // Сохраняем текущую скорость для графика
                currentSpeed = speed;
            } else {
                // Если выключено - сбрасываем все значения на ноль
                updateSpeed(0);
                updatePower(0);
                updateTorque(0);
                currentSpeed = 0;
            }
        }
    });
    updateTimer->start(100);  // Быстрый таймер для индикаторов

    // ========== Таймер для графика (медленный, отдельный) ==========
    QTimer *chartTimer = new QTimer(this);
    connect(chartTimer, &QTimer::timeout, [this, chartWidget]() {
        // Проверяем состояние кнопки
        if (powerButtonWidget && powerButtonWidget->rootObject()) {
            bool isOn = powerButtonWidget->rootObject()->property("isOn").toBool();

            if (isOn && chartWidget && chartWidget->rootObject()) {
                qDebug() << "⏰ Chart timer triggered, currentSpeed =" << currentSpeed;

                // Пробуем вызвать метод addDataPoint
                QObject *chartRoot = chartWidget->rootObject();

                // Проверяем существование метода через metaObject
                bool methodExists = false;
                const QMetaObject *metaObj = chartRoot->metaObject();
                for (int i = 0; i < metaObj->methodCount(); i++) {
                    if (QString(metaObj->method(i).name()) == "addDataPoint") {
                        methodExists = true;
                        break;
                    }
                }

                if (methodExists) {
                    QMetaObject::invokeMethod(chartRoot, "addDataPoint",
                        Q_ARG(QVariant, currentSpeed));
                    qDebug() << "📊 Chart updated with speed:" << currentSpeed;
                } else {
                    qDebug() << "❌ Method addDataPoint not found in QML object";
                    qDebug() << "Available methods:";
                    for (int i = 0; i < metaObj->methodCount(); i++) {
                        qDebug() << "  -" << metaObj->method(i).name();
                    }
                }
            }
        }
    });
    // Устанавливаем интервал обновления графика (например, 1 секунда = 1000 мс)
    chartTimer->start(1000);  // Измените на нужное значение (1000 = 1 секунда)

    qDebug() << "✅ Таймер графика запущен с интервалом 1000 мс";

    ui->tabWidget->tabBar()->setExpanding(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onPowerButtonToggled(bool state)
{
    qDebug() << "🔘 Кнопка переключена:" << (state ? "ВКЛ" : "ВЫКЛ");

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
    } else {
        qDebug() << "✅ Система деактивирована, реле 1 выключено";
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


