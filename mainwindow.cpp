

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QWidget *infoTab = ui->Info;

    // Удаляем существующий layout, если есть
    delete infoTab->layout();

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

       // Размещаем кнопку в правом верхнем углу
       powerButtonWidget->move(900, 30);

       // Подключаем сигнал от кнопки
       QObject *buttonRoot = powerButtonWidget->rootObject();
       if (buttonRoot) {
           // Подключаем сигнал toggled к слоту
           connect(buttonRoot, SIGNAL(toggled(bool)),
                   this, SLOT(onPowerButtonToggled(bool)));

           // Устанавливаем начальное состояние (выключено)
           buttonRoot->setProperty("isOn", false);
       }




    // ========== Таймер для тестирования ==========
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [this]() {
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
    });
    timer->start(100);

    ui->tabWidget->tabBar()->setExpanding(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}


// Реализация слота для кнопки
void MainWindow::onPowerButtonToggled(bool state)
{
    qDebug() << "Кнопка переключена:" << (state ? "ВКЛ" : "ВЫКЛ");

    // Можно добавить дополнительную логику здесь
    if (state) {
        qDebug() << "Система активирована";
        // Здесь можно добавить действия при включении системы
        // Например: включить дополнительные датчики, начать запись данных и т.д.
    } else {
        qDebug() << "Система деактивирована";
        // Здесь можно добавить действия при выключении системы
        // Например: остановить запись данных, выключить дополнительные устройства
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

