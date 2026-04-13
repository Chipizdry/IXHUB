#include "mainwindow.h"
#include "ModbusMaster.h"
#include "WSClient.h"
#include "device_manager.h"
#include "device_poller.h"
#include "nta8a01_device.h"
#include "relay_device.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QScreen>
#include <QSslError>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>
#include <QObject>
#include <QQuickWidget>
#include <QQmlContext>
#include <QVBoxLayout>


class Logger {
public:
    static void log(const QString& message, const QString& level = "INFO") {
        QFile file("/var/log/modbus_devices.log");
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
                << " [" << level << "] " << message << "\n";
            file.close();
        }
    }
};


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

    // Создаем датчики NTA8A01 для адресов 1-5
    //dm.addDevice(new NTA8A01Device(1));
    //dm.addDevice(new NTA8A01Device(2));
    dm.addDevice(new NTA8A01Device(3));
    dm.addDevice(new NTA8A01Device(4));
    dm.addDevice(new NTA8A01Device(5));
    dm.addDevice(new NTA8A01Device(6));
    dm.addDevice(new RelayDevice(0xFF));


    // Загружаем конфигурацию если есть
    dm.loadConfig("/etc/modbus_devices.conf");
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Настройка устройств
    setupDefaultDevices();

    ModbusMaster master;
    DevicePoller poller(&master);

    // Получаем серийный номер устройства
    QString serialNumber = getCpuSerialNumber();

    // Формируем URL с session_id (как в ESP32)
    QString urlString = QString("wss://dev.monitoring.cor-int.com/dev-modbus/devices?session_id=%1")
                        .arg(serialNumber);
    QUrl url(urlString);

    qDebug() << "Connecting to:" << url.toString();

    WSClient ws(url);
    ws.setSessionId(serialNumber);

    // Если нужна дополнительная авторизация по email/password
    ws.setCredentials("chipizdry@gmail.com", "12345678", "IXHUB_Flywheel");

    ws.connectToServer();

    if (!master.init("/dev/ttyS5", 9600)) {
        qDebug() << "Ошибка открытия UART";
        return -1;
    }



    QObject::connect(&poller, &DevicePoller::deviceDataReady,
        [&ws](int slaveId, const QJsonObject& data) {
            QJsonObject message;
            message["type"] = "device_data";
            message["slave_id"] = slaveId;
            message["data"] = data;
            message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

            ws.sendJson(message);
            qDebug() << "📤 Sent device data to cloud";

            // ============ ЛОГГИРОВАНИЕ ДАННЫХ ============
            // Извлекаем температуру из данных
            if (data.contains("temperature_celsius")) {
                double temperature = data["temperature_celsius"].toDouble();

                // Лог в файл
                Logger::log(QString("Slave %1: Temperature=%2°C, Sensor=%3")
                           .arg(slaveId)
                           .arg(temperature)
                           .arg(data["sensor_status"].toString()),
                           "DATA");

                // Детальный лог в консоль
                qDebug() << "=========================================";
                qDebug() << "📊 Device Data Report";
                qDebug() << "   Slave ID:    " << slaveId;
                qDebug() << "   Temperature: " << temperature << "°C";
                qDebug() << "   Sensor Status:" << data["sensor_status"].toString();

                // Дополнительные поля для NTA8A01
                if (data.contains("rs485_address")) {
                    qDebug() << "   RS485 Addr:  " << data["rs485_address"].toInt();
                }
                if (data.contains("baud_rate")) {
                    qDebug() << "   Baud Rate:   " << data["baud_rate"].toInt();
                }
                if (data.contains("temperature_correction")) {
                    qDebug() << "   Correction:  " << data["temperature_correction"].toDouble();
                }
                qDebug() << "=========================================";
            }

            // Лог всего JSON (опционально)
            Logger::log(QString("Full data: %1").arg(QString(QJsonDocument(data).toJson(QJsonDocument::Compact))), "DEBUG");
        });

    QObject::connect(&poller, &DevicePoller::deviceError,
        [&ws](int slaveId, const QString& error) {
            QJsonObject message;
            message["type"] = "device_error";
            message["slave_id"] = slaveId;
            message["error"] = error;
            message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

            ws.sendJson(message);
            qDebug() << "⚠️ Device error sent to cloud";
        });

    // Обработчики событий WebSocket
    QObject::connect(&ws, &WSClient::connected, []() {
        qDebug() << "✅ WebSocket connected";
    });

    QObject::connect(&ws, &WSClient::authenticated, [&ws]() {
        qDebug() << "✅ WebSocket authenticated";

        // Отправляем список устройств
        QJsonObject devicesMsg;
        devicesMsg["type"] = "device_list";
        devicesMsg["devices"] = DeviceManager::instance().getAllDevicesInfo();
        ws.sendJson(devicesMsg);
    });

    QObject::connect(&ws, &WSClient::disconnected, []() {
        qDebug() << "⚠️ WebSocket disconnected";
    });

    QObject::connect(&ws, &WSClient::cloudStatusChanged, [](const QString &status) {
        qDebug() << "☁️ Cloud status:" << status;
    });

    // Обработка hex_data команд (как в ESP32)
    QObject::connect(&ws, &WSClient::hexDataReceived,
        [&master](const QString &hexData, const QString &commandType, const QString &commandName) {
            qDebug() << "📦 Hex data received:" << hexData;
            qDebug() << "   Command type:" << commandType;
            qDebug() << "   Command name:" << commandName;

            QByteArray data = QByteArray::fromHex(hexData.toUtf8());
            if (!data.isEmpty()) {
                quint8 slaveId = static_cast<quint8>(data[0]);
                master.sendRawData(slaveId, data);
            }
        });

    // Обработка PI30 данных
    QObject::connect(&ws, &WSClient::pi30DataReceived, [](const QString &pi30Data) {
        qDebug() << "🔶 PI30 data received:" << pi30Data;
    });

    // Обработка команд настроек - ТОЛЬКО ОДИН ОБРАБОТЧИК!
    QObject::connect(&ws, &WSClient::settingsCommandReceived,
        [&ws, &poller, &master](const QString &commandType, const QJsonObject &settings) {
            qDebug() << "⚙️ Settings command:" << commandType;

            if (commandType == "get_settings") {
                QJsonObject response;
                response["command_type"] = "settings_response";
                response["status"] = "ok";
                response["poll_interval_ms"] = 100;
                response["baud_rate"] = 9600;
                response["devices"] = DeviceManager::instance().getAllDevicesInfo();
                ws.sendJson(response);  // ИСПРАВЛЕНО: используем ws, а не WSClient::instance()

            } else if (commandType == "set_poll_interval") {
                int interval = settings["interval_ms"].toInt(100);
                poller.setPollInterval(interval);

                QJsonObject response;
                response["command_type"] = "settings_response";
                response["status"] = "ok";
                response["poll_interval_ms"] = interval;
                ws.sendJson(response);

            } else if (commandType == "add_device") {
                int slaveId = settings["slave_id"].toInt();
                QString deviceType = settings["device_type"].toString();

                if (deviceType == "NTA8A01") {
                    DeviceManager::instance().addDevice(new NTA8A01Device(slaveId));
                    poller.refreshDeviceList();
                    DeviceManager::instance().saveConfig("/etc/modbus_devices.conf");

                    QJsonObject response;
                    response["command_type"] = "settings_response";
                    response["status"] = "ok";
                    response["message"] = "Device added";
                    ws.sendJson(response);
                }

            } else if (commandType == "remove_device") {
                int slaveId = settings["slave_id"].toInt();
                DeviceManager::instance().removeDevice(slaveId);
                poller.refreshDeviceList();
                DeviceManager::instance().saveConfig("/etc/modbus_devices.conf");

                QJsonObject response;
                response["command_type"] = "settings_response";
                response["status"] = "ok";
                response["message"] = "Device removed";
                ws.sendJson(response);
            }
        });

    // Обработка OTA обновлений
    QObject::connect(&ws, &WSClient::otaUpdateReceived, [](const QString &firmwareUrl) {
        qDebug() << "🚀 OTA update available:" << firmwareUrl;
    });

    // Запуск опроса устройств
    poller.start(80);

    MainWindow w;
    w.showFullScreen();
    w.show();

    return a.exec();
}


