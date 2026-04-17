


#include "mainwindow.h"
#include "ModbusMaster.h"
#include "device_manager.h"
#include "device_poller.h"
#include "nta8a01_device.h"
#include "relay_device.h"
#include "bldc_driver_device.h"
#include "vacuum_pressure_sensor.h"
#include "websocket_manager.h"
#include "logger.h"
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QFile>
#include <QRegularExpression>

ModbusMaster* g_master = nullptr;
DevicePoller* g_poller = nullptr;

// Функция для получения серийного номера CPU
QString getCpuSerialNumber() {
    QFile file("/proc/cpuinfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Не удалось открыть /proc/cpuinfo";
        return "UNKNOWN_SERIAL";
    }

    QString content = file.readAll();
    file.close();

    QRegularExpression reSerial("Serial\\s*:\\s*([0-9a-fA-F]+)");
    QRegularExpressionMatch match = reSerial.match(content);

    if (match.hasMatch()) {
        QString serial = match.captured(1);
        qDebug() << "CPU Serial Number:" << serial;
        return serial;
    }

    return "UNKNOWN_SERIAL";
}

void setupDefaultDevices() {
    auto& dm = DeviceManager::instance();

    dm.addDevice(new RelayDevice(1));
    dm.addDevice(new BldcDriverDevice(2));
    dm.addDevice(new NTA8A01Device(3));
    dm.addDevice(new NTA8A01Device(4));
    dm.addDevice(new NTA8A01Device(5));
    dm.addDevice(new NTA8A01Device(6));
    dm.addDevice(new VacuumPressureSensor(0x09));

    // Загружаем конфигурацию если есть
    dm.loadConfig("/etc/modbus_devices.conf");
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Скрываем курсор мыши для всего приложения
    a.setOverrideCursor(QCursor(Qt::BlankCursor));

    // Настройка устройств
    setupDefaultDevices();

    // Инициализация Modbus
    ModbusMaster master;
    g_master = &master;

    if (!master.init("/dev/ttyS5", 9600)) {
        Logger::error("Ошибка открытия UART");
        return -1;
    }

    // Инициализация поллера
    DevicePoller poller(&master);
    g_poller = &poller;

    // Получаем серийный номер
    QString serialNumber = getCpuSerialNumber();

    // Инициализация WebSocket менеджера
    auto& wsManager = WebSocketManager::instance();
    if (!wsManager.initialize(serialNumber)) {
        Logger::warning("Failed to initialize WebSocket manager");
    }

    // Запуск опроса устройств
    poller.start(30);
    Logger::debug("Device poller started with interval 80ms");

    // Запуск главного окна
    MainWindow w;
    w.showFullScreen();
    w.show();

    int result = a.exec();

    // Очистка перед выходом
    wsManager.cleanup();

    return result;
}


