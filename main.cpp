

#include "mainwindow.h"
#include "ModbusMaster.h"
#include "WSClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QScreen>
#include <QSslError>
#include <QFile>
#include <QRegularExpression>
#include <QDateTime>
#include <QObject>

#include <QQuickWidget>
#include <QQmlContext>
#include <QVBoxLayout>

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

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ModbusMaster master;

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

    QList<int> slaves = {1,2,3,4,5};
    int currentIndex = 0;

    auto pollNext = [&]() {
        int slave = slaves[currentIndex];
        qDebug() << "➡️ Опрос slave:" << slave;
        master.readHoldingRegisters(slave, 0x0000, 4);
    };

    QObject::connect(&master, &ModbusMaster::dataReady,
        [&](QByteArray data){
            qDebug() << "RX:" << data.toHex();

            // Отправляем данные через WebSocket (как в ESP32)
            QJsonObject json;
            json["slave"] = slaves[currentIndex];
            json["data"] = QString(data.toHex());
            json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            ws.sendJson(json);

            currentIndex = (currentIndex + 1) % slaves.size();
            QTimer::singleShot(100, pollNext);
        });

    QObject::connect(&master, &ModbusMaster::error,
        [&](QString err){
            qDebug() << "Ошибка Modbus:" << err;
            currentIndex = (currentIndex + 1) % slaves.size();
            QTimer::singleShot(100, pollNext);
        });

    // Обработчики событий WebSocket
    QObject::connect(&ws, &WSClient::connected, []() {
        qDebug() << "✅ WebSocket connected";
    });

    QObject::connect(&ws, &WSClient::authenticated, []() {
        qDebug() << "✅ WebSocket authenticated";
    });

    QObject::connect(&ws, &WSClient::disconnected, []() {
        qDebug() << "⚠️ WebSocket disconnected";
    });

    QObject::connect(&ws, &WSClient::cloudStatusChanged, [](const QString &status) {
        qDebug() << "☁️ Cloud status:" << status;
    });

    // Обработка hex_data команд (как в ESP32)
    QObject::connect(&ws, &WSClient::hexDataReceived,
        [&](const QString &hexData, const QString &commandType, const QString &commandName) {
        qDebug() << "📦 Hex data received:" << hexData;
        qDebug() << "   Command type:" << commandType;
        qDebug() << "   Command name:" << commandName;

        // Здесь отправляем данные в Modbus/RS485
        // master.sendHexData(hexData);
    });

    // Обработка PI30 данных
    QObject::connect(&ws, &WSClient::pi30DataReceived, [](const QString &pi30Data) {
        qDebug() << "🔶 PI30 data received:" << pi30Data;
        // Обработка PI30 команды
    });

    // Обработка команд настроек
    QObject::connect(&ws, &WSClient::settingsCommandReceived,
        [&](const QString &commandType, const QJsonObject &settings) {
        qDebug() << "⚙️ Settings command:" << commandType;

        if (commandType == "get_settings") {
            // Отправляем текущие настройки
            QJsonObject response;
            response["command_type"] = "settings_response";
            response["status"] = "ok";
            ws.sendJson(response);
        } else if (commandType == "set_settings") {
            // Применяем настройки
            qDebug() << "Applying settings:" << settings;
        }
    });

    // Обработка OTA обновлений
    QObject::connect(&ws, &WSClient::otaUpdateReceived, [](const QString &firmwareUrl) {
        qDebug() << "🚀 OTA update available:" << firmwareUrl;
        // Здесь запускаем процесс обновления
    });

    QTimer::singleShot(1000, pollNext);

    MainWindow w;
    w.showFullScreen();
    w.show();

    return a.exec();
}

