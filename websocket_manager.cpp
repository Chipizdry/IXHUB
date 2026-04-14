



#include "websocket_manager.h"
#include "device_manager.h"
#include "nta8a01_device.h"
#include "relay_device.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>

WebSocketManager::WebSocketManager()
    : QObject(nullptr)
    , m_ws(nullptr)
    , m_poller(nullptr)
    , m_master(nullptr)
    , m_authenticated(false)
{
}

WebSocketManager::~WebSocketManager()
{
    cleanup();
}

WebSocketManager& WebSocketManager::instance()
{
    static WebSocketManager manager;
    return manager;
}

bool WebSocketManager::initialize(const QString& serialNumber)
{
    m_serialNumber = serialNumber;

    // Получаем указатели на глобальные объекты
    extern ModbusMaster* g_master;
    extern DevicePoller* g_poller;

    m_master = g_master;
    m_poller = g_poller;

    if (!m_master || !m_poller) {
        qDebug() << "❌ WebSocketManager: Master or Poller not available";
        return false;
    }

    // Формируем URL
    QString urlString = QString("wss://dev.monitoring.cor-int.com/dev-modbus/devices?session_id=%1")
                        .arg(m_serialNumber);
    QUrl url(urlString);

    qDebug() << "🌐 Connecting to:" << url.toString();

    // Создаем WebSocket клиент
    m_ws = new WSClient(url);
    m_ws->setSessionId(m_serialNumber);
    m_ws->setCredentials("chipizdry@gmail.com", "12345678", "IXHUB_Flywheel");

    // Настраиваем соединения
    setupWSConnections();
    setupPollerConnections();

    // Подключаемся к серверу
    m_ws->connectToServer();

    return true;
}

void WebSocketManager::cleanup()
{
    if (m_ws) {
        m_ws->disconnect();
        delete m_ws;
        m_ws = nullptr;
    }
}

void WebSocketManager::sendDeviceData(int slaveId, const QJsonObject& data)
{
    if (!m_ws || !m_authenticated) {
        qDebug() << "⚠️ Cannot send device data - not authenticated";
        return;
    }

    QJsonObject message;
    message["type"] = "device_data";
    message["slave_id"] = slaveId;
    message["data"] = data;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_ws->sendJson(message);
    qDebug() << "📤 Sent device data for slave" << slaveId << "to cloud";
}

void WebSocketManager::sendDeviceError(int slaveId, const QString& error)
{
    if (!m_ws || !m_authenticated) {
        return;
    }

    QJsonObject message;
    message["type"] = "device_error";
    message["slave_id"] = slaveId;
    message["error"] = error;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_ws->sendJson(message);
    qDebug() << "⚠️ Device error sent for slave" << slaveId;
}

void WebSocketManager::sendDeviceList()
{
    if (!m_ws || !m_authenticated) {
        return;
    }

    QJsonObject devicesMsg;
    devicesMsg["type"] = "device_list";
    devicesMsg["devices"] = DeviceManager::instance().getAllDevicesInfo();
    m_ws->sendJson(devicesMsg);
    qDebug() << "📋 Device list sent to cloud";
}

void WebSocketManager::setupWSConnections()
{
    if (!m_ws) return;

    // Обработчики событий WebSocket
    QObject::connect(m_ws, &WSClient::connected, [this]() {
        qDebug() << "✅ WebSocket connected";
    });

    QObject::connect(m_ws, &WSClient::authenticated, [this]() {
        qDebug() << "✅ WebSocket authenticated";
        m_authenticated = true;
        sendDeviceList();
    });

    QObject::connect(m_ws, &WSClient::disconnected, [this]() {
        qDebug() << "⚠️ WebSocket disconnected";
        m_authenticated = false;
    });

    QObject::connect(m_ws, &WSClient::cloudStatusChanged, [](const QString &status) {
        qDebug() << "☁️ Cloud status:" << status;
    });

    // Обработка hex_data команд
    QObject::connect(m_ws, &WSClient::hexDataReceived,
        [this](const QString &hexData, const QString &commandType, const QString &commandName) {
            qDebug() << "📦 Hex data received:" << hexData;
            qDebug() << "   Command type:" << commandType;
            qDebug() << "   Command name:" << commandName;

            QByteArray data = QByteArray::fromHex(hexData.toUtf8());
            if (!data.isEmpty() && m_master) {
                quint8 slaveId = static_cast<quint8>(data[0]);
                m_master->sendRawData(slaveId, data);
            }
        });

    // Обработка PI30 данных
    QObject::connect(m_ws, &WSClient::pi30DataReceived, [](const QString &pi30Data) {
        qDebug() << "🔶 PI30 data received:" << pi30Data;
    });

    // Обработка команд настроек
    QObject::connect(m_ws, &WSClient::settingsCommandReceived,
        [this](const QString &commandType, const QJsonObject &settings) {
            qDebug() << "⚙️ Settings command:" << commandType;
            handleSettingsCommand(commandType, settings);
        });

    // Обработка OTA обновлений
    QObject::connect(m_ws, &WSClient::otaUpdateReceived, [](const QString &firmwareUrl) {
        qDebug() << "🚀 OTA update available:" << firmwareUrl;
    });
}

void WebSocketManager::setupPollerConnections()
{
    if (!m_poller) return;

    // Подключаем сигналы поллера
    QObject::connect(m_poller, &DevicePoller::deviceDataReady,
        [this](int slaveId, const QJsonObject& data) {
            sendDeviceData(slaveId, data);
            logDeviceData(slaveId, data);
        });

    QObject::connect(m_poller, &DevicePoller::deviceError,
        [this](int slaveId, const QString& error) {
            sendDeviceError(slaveId, error);
        });
}

void WebSocketManager::handleSettingsCommand(const QString& commandType, const QJsonObject& settings)
{
    if (!m_ws) return;

    if (commandType == "get_settings") {
        QJsonObject response;
        response["command_type"] = "settings_response";
        response["status"] = "ok";
        response["poll_interval_ms"] = m_poller ? m_poller->getPollInterval() : 100;
        response["baud_rate"] = 9600;
        response["devices"] = DeviceManager::instance().getAllDevicesInfo();
        m_ws->sendJson(response);

    } else if (commandType == "set_poll_interval") {
        int interval = settings["interval_ms"].toInt(100);
        if (m_poller) {
            m_poller->setPollInterval(interval);
        }

        QJsonObject response;
        response["command_type"] = "settings_response";
        response["status"] = "ok";
        response["poll_interval_ms"] = interval;
        m_ws->sendJson(response);

    } else if (commandType == "add_device") {
        int slaveId = settings["slave_id"].toInt();
        QString deviceType = settings["device_type"].toString();

        if (deviceType == "NTA8A01") {
            DeviceManager::instance().addDevice(new NTA8A01Device(slaveId));
            if (m_poller) {
                m_poller->refreshDeviceList();
            }
            DeviceManager::instance().saveConfig("/etc/modbus_devices.conf");

            QJsonObject response;
            response["command_type"] = "settings_response";
            response["status"] = "ok";
            response["message"] = "Device added";
            m_ws->sendJson(response);
        }

    } else if (commandType == "remove_device") {
        int slaveId = settings["slave_id"].toInt();
        DeviceManager::instance().removeDevice(slaveId);
        if (m_poller) {
            m_poller->refreshDeviceList();
        }
        DeviceManager::instance().saveConfig("/etc/modbus_devices.conf");

        QJsonObject response;
        response["command_type"] = "settings_response";
        response["status"] = "ok";
        response["message"] = "Device removed";
        m_ws->sendJson(response);
    }
}

void WebSocketManager::logDeviceData(int slaveId, const QJsonObject& data)
{
    // Логирование данных устройства
    if (data.contains("temperature_celsius")) {
        double temperature = data["temperature_celsius"].toDouble();

        qDebug() << "=========================================";
        qDebug() << "📊 Device Data Report";
        qDebug() << "   Slave ID:    " << slaveId;
        qDebug() << "   Temperature: " << temperature << "°C";
        qDebug() << "   Sensor Status:" << data["sensor_status"].toString();

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
}



