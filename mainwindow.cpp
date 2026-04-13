


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
#include "device_poller.h"      // ДОБАВИТЬ
#include "ModbusMaster.h"       // ДОБАВИТЬ

// Глобальные указатели (определены в main.cpp)
extern DevicePoller* g_poller;
extern ModbusMaster* g_master;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QWidget *infoTab = ui->Info;

    // Удаляем существующий layout, если есть
    delete infoTab->layout();

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

    // Устанавливаем контекстные свойства ДО загрузки QML
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

    // Устанавливаем контекстные свойства ДО загрузки QML
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

    // Размещаем кнопку
    powerButtonWidget->move(920, 400);

    // Подключаем сигнал от кнопки
    QObject *buttonRoot = powerButtonWidget->rootObject();
    if (buttonRoot) {
        // Подключаем сигнал toggled к слоту
        connect(buttonRoot, SIGNAL(toggled(bool)),
                this, SLOT(onPowerButtonToggled(bool)));

        // Устанавливаем начальное состояние (выключено)
        buttonRoot->setProperty("isOn", false);
    }

    // ========== Таймер для тестирования с управлением от кнопки ==========
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
            } else {
                // Если выключено - сбрасываем все значения на ноль
                updateSpeed(0);
                updatePower(0);
                updateTorque(0);
            }
        }
    });
    updateTimer->start(100);

    ui->tabWidget->tabBar()->setExpanding(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Реализация слота для кнопки
void MainWindow::onPowerButtonToggled(bool state)
{
    qDebug() << "🔘 Кнопка переключена:" << (state ? "ВКЛ" : "ВЫКЛ");

    // Управляем реле 1
    if (m_relayDevice) {
        qDebug() << "📡 Отправляем команду на реле 1:" << (state ? "ON" : "OFF");

        // Генерируем команду для реле 1 (теперь метод публичный)
        QByteArray command = m_relayDevice->generateSetRelayCommand(1, state);

        if (!command.isEmpty() && g_poller) {
            // Отправляем через поллер как приоритетную команду
            QString commandName = state ? "SetRelay1_ON" : "SetRelay1_OFF";
            g_poller->sendPriorityCommand(0x0A, command, commandName);
            qDebug() << "✅ Команда отправлена в очередь поллера";
        } else if (!command.isEmpty() && g_master) {
            // Fallback: прямая отправка (если поллер недоступен)
            qDebug() << "⚠️ Poller not available, sending directly via ModbusMaster";
            g_master->sendRawData(0x0A, command);
        } else {
            qDebug() << "❌ Failed to send command - no command generated or no transport available";
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


