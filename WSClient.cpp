

#include "WSClient.h"
#include <QDebug>
#include <QSslConfiguration>
#include <QSslError>
#include <QJsonDocument>
#include <QTimer>
#include <QDateTime>
#include <QLoggingCategory>
#include <QRegularExpression>

// Включаем детальное логирование
static const QString TAG = "WSClient";

WSClient::WSClient(const QUrl &url, QObject *parent)
    : QObject(parent)
    , m_url(url)
    , m_connected(false)
    , m_authenticated(false)
    , m_reconnectEnabled(true)
    , m_authSent(false)
    , m_debugEnabled(true)
    , m_cloudStatus("Idle")
{
    setupConnections();

    // Таймер для реконнекта
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &WSClient::onReconnectTimeout);

    // Таймер для heartbeat
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &WSClient::onHeartbeatTimeout);
    m_heartbeatTimer.setInterval(HEARTBEAT_INTERVAL);

    // Таймер таймаута подключения
    m_connectionTimeoutTimer.setSingleShot(true);
    connect(&m_connectionTimeoutTimer, &QTimer::timeout, this, &WSClient::onConnectionTimeout);
}

WSClient::~WSClient()
{
    disconnect();
}

void WSClient::setupConnections()
{
    connect(&m_ws, &QWebSocket::connected, this, &WSClient::onConnected);
    connect(&m_ws, &QWebSocket::disconnected, this, &WSClient::onDisconnected);
    connect(&m_ws, &QWebSocket::textMessageReceived, this, &WSClient::onTextMessageReceived);
    connect(&m_ws, &QWebSocket::sslErrors, this, &WSClient::onSslErrors);
}

void WSClient::setCredentials(const QString &email, const QString &password, const QString &nodeName)
{
    m_email = email;
    m_password = password;
    m_nodeName = nodeName;
    logMessage("INFO", QString("Credentials set - Email: %1, Node: %2").arg(m_email).arg(m_nodeName));
}

void WSClient::setSessionId(const QString &sessionId)
{
    m_sessionId = sessionId;
    logMessage("INFO", QString("Session ID set: %1").arg(m_sessionId));
}

void WSClient::connectToServer()
{
    if (m_ws.state() == QAbstractSocket::ConnectingState ||
        m_ws.state() == QAbstractSocket::ConnectedState) {
        logMessage("WARN", "Already connecting/connected");
        return;
    }

    logMessage("INFO", QString("🌐 Connecting to: %1").arg(m_url.toString()));

    // Игнорируем SSL ошибки для embedded (как в ESP32)
    m_ws.ignoreSslErrors();

    // Настройка SSL
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // skip_cert_common_name_check
    m_ws.setSslConfiguration(sslConfig);

    m_ws.open(m_url);
    m_authSent = false;
    m_authenticated = false;
    m_rxBuffer.clear();

    // Таймаут подключения
    m_connectionTimeoutTimer.start(CONNECTION_TIMEOUT);
}

void WSClient::onConnected()
{
    m_connectionTimeoutTimer.stop();
    logMessage("INFO", "✅ WebSocket connected");
    m_connected = true;
    m_reconnectTimer.stop();
    m_heartbeatTimer.start();

    // Отправляем авторизацию
    sendAuthMessage();

    emit connected();
}

void WSClient::sendAuthMessage()
{
    if (m_authSent) {
        logMessage("WARN", "Auth message already sent");
        return;
    }

    // Приоритет: если есть email/password, используем их
    if (!m_email.isEmpty() && !m_password.isEmpty()) {
        QJsonObject authMsg;
        authMsg["email"] = m_email;
        authMsg["password"] = m_password;
        if (!m_nodeName.isEmpty()) {
            authMsg["node_name"] = m_nodeName;
        }

        QJsonDocument doc(authMsg);
        QString authStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

        sendMessage(authStr);
        logMessage("SENT", QString("📤 Auth message: %1").arg(authStr));
        m_authSent = true;
    }
    // Если есть session_id, он уже в URL
    else if (!m_sessionId.isEmpty()) {
        logMessage("INFO", QString("🔑 Using session_id from URL: %1").arg(m_sessionId));
        m_authSent = true;
        // Отправляем сообщение о готовности
        QJsonObject readyMsg;
        readyMsg["command_type"] = "device_online";
        readyMsg["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        sendJson(readyMsg);
    }
    else {
        logMessage("WARN", "⚠️ No credentials provided for authentication");
    }
}

void WSClient::onDisconnected()
{
    logMessage("WARN", "❌ WebSocket disconnected");
    m_connected = false;
    m_authenticated = false;
    m_authSent = false;
    m_heartbeatTimer.stop();
    m_cloudStatus = "disconnected";

    emit disconnected();
    emit cloudStatusChanged(m_cloudStatus);

    if (m_reconnectEnabled) {
        logMessage("INFO", QString("🔄 Scheduling reconnect in %1 ms").arg(RECONNECT_INTERVAL));
        m_reconnectTimer.start(RECONNECT_INTERVAL);
    }
}

void WSClient::onTextMessageReceived(const QString &message)
{
    // Сброс таймера активности (как last_ws_event_tick в ESP32)
    m_heartbeatTimer.start(); // перезапускаем таймер heartbeat

    logMessage("RECV", QString("📩 %1").arg(message));

    // Обработка ping/pong
    if (message == "ping") {
        logMessage("INFO", "💓 Ping received → sending pong");
        sendMessage("pong");
        return;
    }

    emit messageReceived(message);

    // Парсим JSON и обрабатываем
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject json = doc.object();
        emit jsonReceived(json);
        processMessage(message);
    }
}

void WSClient::processMessage(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull()) return;

    QJsonObject json = doc.object();

    // Обработка cloud_status
    if (json.contains("cloud_status")) {
        handleCloudStatus(json);
    }

    // Обработка hex_data
    if (json.contains("hex_data") || message.contains("\"hex_data\"")) {
        handleHexData(json);
    }

    // Обработка pi30
    if (json.contains("pi30") || message.contains("\"pi30\"")) {
        handlePi30Data(json);
    }

    // Обработка command_type (settings)
    if (json.contains("command_type")) {
        handleSettingsCommand(json);
    }

    // Обработка update_firmware (OTA)
    if (json.contains("update_firmware") || message.contains("update_firmware")) {
        handleOtaUpdate(json);
    }
}

void WSClient::handleCloudStatus(const QJsonObject &json)
{
    QString status = json["cloud_status"].toString();
    logMessage("INFO", QString("💾 Cloud status: %1").arg(status));

    m_cloudStatus = status;
    emit cloudStatusChanged(status);

    if (status == "authenticated" || status == "connected") {
        if (!m_authenticated) {
            m_authenticated = true;
            logMessage("INFO", "✅ Authenticated successfully");
            emit authenticated();

            // Отправляем device_online как в ESP32
            QJsonObject onlineMsg;
            onlineMsg["command_type"] = "device_online";
            onlineMsg["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            sendJson(onlineMsg);
            logMessage("SENT", "📤 device_online message sent");
        }
    } else if (status == "disconnected" || status == "error") {
        m_authenticated = false;
    }
}

void WSClient::handleHexData(const QJsonObject &json)
{
    QString hexData;
    QString commandType = json["command_type"].toString();
    QString commandName = json["command_name"].toString();

    // Извлекаем hex_data
    if (json.contains("hex_data")) {
        hexData = json["hex_data"].toString();
    } else {
        // Пробуем найти в сыром JSON
        QString msg = QJsonDocument(json).toJson(QJsonDocument::Compact);
        QRegularExpression re("\"hex_data\"\\s*:\\s*\"([^\"]+)\"");
        QRegularExpressionMatch match = re.match(msg);
        if (match.hasMatch()) {
            hexData = match.captured(1);
        }
    }

    if (!hexData.isEmpty()) {
        logMessage("INFO", QString("✅ HEX data extracted: %1, type: %2, name: %3")
                   .arg(hexData).arg(commandType).arg(commandName));
        emit hexDataReceived(hexData, commandType, commandName);
    }
}

void WSClient::handlePi30Data(const QJsonObject &json)
{
    QString pi30Data;

    if (json.contains("pi30")) {
        pi30Data = json["pi30"].toString();
    } else {
        QString msg = QJsonDocument(json).toJson(QJsonDocument::Compact);
        QRegularExpression re("\"pi30\"\\s*:\\s*\"([^\"]+)\"");
        QRegularExpressionMatch match = re.match(msg);
        if (match.hasMatch()) {
            pi30Data = match.captured(1);
        }
    }

    if (!pi30Data.isEmpty()) {
        logMessage("INFO", QString("🔶 PI30 data: %1").arg(pi30Data));
        emit pi30DataReceived(pi30Data);
    }
}

void WSClient::handleSettingsCommand(const QJsonObject &json)
{
    QString commandType = json["command_type"].toString();

    logMessage("INFO", QString("⚙️ Settings command: %1").arg(commandType));

    if (commandType == "get_settings") {
        QString category = json["settings_requested"].toString();
        if (category.isEmpty()) category = "all";
        logMessage("INFO", QString("📥 Get settings requested: %1").arg(category));
    }
    else if (commandType == "set_settings") {
        QJsonObject settings = json["settings"].toObject();
        logMessage("INFO", QString("✍️ Set settings received, category: %1")
                   .arg(settings.keys().join(", ")));
        emit settingsCommandReceived(commandType, settings);
    }

    emit settingsCommandReceived(commandType, json);
}

void WSClient::handleOtaUpdate(const QJsonObject &json)
{
    QString firmwareUrl = json["firmware_url"].toString();

    if (!firmwareUrl.isEmpty()) {
        logMessage("WARN", QString("🚀 OTA update from: %1").arg(firmwareUrl));
        emit otaUpdateReceived(firmwareUrl);
    }
}

void WSClient::onSslErrors(const QList<QSslError> &errors)
{
    logMessage("WARN", "⚠️ SSL errors occurred:");
    for (const QSslError &err : errors) {
        logMessage("WARN", QString("  - %1").arg(err.errorString()));
    }
    m_ws.ignoreSslErrors();
}

void WSClient::onReconnectTimeout()
{
    if (m_reconnectEnabled && !m_connected) {
        logMessage("INFO", "🔄 Attempting to reconnect...");
        connectToServer();
    }
}

void WSClient::onHeartbeatTimeout()
{
    if (m_connected) {
        logMessage("DEBUG", "💓 Sending heartbeat ping");
        sendMessage("ping");
    }
}

void WSClient::onConnectionTimeout()
{
    if (!m_connected && m_ws.state() != QAbstractSocket::ConnectedState) {
        logMessage("WARN", "⏰ Connection timeout");
        m_ws.close();
        if (m_reconnectEnabled) {
            m_reconnectTimer.start(RECONNECT_INTERVAL);
        }
        emit error("Connection timeout");
    }
}

void WSClient::sendMessage(const QString &message)
{
    if (m_connected && m_ws.state() == QAbstractSocket::ConnectedState) {
        m_ws.sendTextMessage(message);
        logMessage("SENT", QString("📤 %1").arg(message));
    } else {
        logMessage("WARN", "⚠️ Cannot send message - WebSocket not connected");
        emit error("WebSocket not connected");
    }
}

void WSClient::sendJson(const QJsonObject &json)
{
    QJsonDocument doc(json);
    sendMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void WSClient::disconnect()
{
    m_reconnectEnabled = false;
    m_reconnectTimer.stop();
    m_heartbeatTimer.stop();
    m_connectionTimeoutTimer.stop();

    if (m_ws.state() == QAbstractSocket::ConnectedState ||
        m_ws.state() == QAbstractSocket::ConnectingState) {
        m_ws.close();
    }
}

void WSClient::setReconnectEnabled(bool enabled)
{
    m_reconnectEnabled = enabled;
    if (!enabled) {
        m_reconnectTimer.stop();
    }
}

void WSClient::resetReconnectTimer()
{
    m_reconnectTimer.stop();
}

void WSClient::logMessage(const QString &level, const QString &message)
{
    if (!m_debugEnabled && level == "DEBUG") return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    qDebug() << qPrintable(QString("[%1] [%2] %3: %4")
                          .arg(timestamp)
                          .arg(TAG)
                          .arg(level)
                          .arg(message));
}


