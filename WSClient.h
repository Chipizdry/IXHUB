

#ifndef WSCLIENT_H
#define WSCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QUrl>
#include <QTimer>
#include <QJsonObject>
#include <QQueue>

class WSClient : public QObject
{
    Q_OBJECT
public:
    explicit WSClient(const QUrl &url, QObject *parent = nullptr);
    ~WSClient();

    void connectToServer();
    void sendMessage(const QString &message);
    void sendJson(const QJsonObject &json);
    void disconnect();
    bool isConnected() const { return m_connected; }
    bool isAuthenticated() const { return m_authenticated; }

    // Управление реконнектом
    void setReconnectEnabled(bool enabled);
    void setCredentials(const QString &email, const QString &password, const QString &nodeName);
    void setSessionId(const QString &sessionId);

    // Статус облака
    QString getCloudStatus() const { return m_cloudStatus; }

signals:
    void connected();
    void disconnected();
    void authenticated();
    void messageReceived(const QString &message);
    void jsonReceived(const QJsonObject &json);
    void error(const QString &error);
    void cloudStatusChanged(const QString &status);

    // Сигналы для команд
    void hexDataReceived(const QString &hexData, const QString &commandType, const QString &commandName);
    void pi30DataReceived(const QString &pi30Data);
    void settingsCommandReceived(const QString &commandType, const QJsonObject &settings);
    void otaUpdateReceived(const QString &firmwareUrl);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onSslErrors(const QList<QSslError> &errors);
    void onReconnectTimeout();
    void onHeartbeatTimeout();
    void onConnectionTimeout();

private:
    void setupConnections();
    void sendAuthMessage();
    void resetReconnectTimer();
    void processMessage(const QString &message);
    void handleCloudStatus(const QJsonObject &json);
    void handleHexData(const QJsonObject &json);
    void handlePi30Data(const QJsonObject &json);
    void handleSettingsCommand(const QJsonObject &json);
    void handleOtaUpdate(const QJsonObject &json);
    void logMessage(const QString &direction, const QString &message);

    QWebSocket m_ws;
    QUrl m_url;
    QTimer m_reconnectTimer;
    QTimer m_heartbeatTimer;
    QTimer m_connectionTimeoutTimer;

    bool m_connected;
    bool m_authenticated;
    bool m_reconnectEnabled;
    bool m_authSent;
    bool m_debugEnabled;

    // Данные для авторизации
    QString m_email;
    QString m_password;
    QString m_nodeName;
    QString m_sessionId;
    QString m_cloudStatus;

    // Буфер для фрагментированных сообщений
    QString m_rxBuffer;

    static const int RECONNECT_INTERVAL = 3000;      // 3 секунды
    static const int HEARTBEAT_INTERVAL = 30000;     // 30 секунд
    static const int CONNECTION_TIMEOUT = 20000;     // 20 секунд
    static const int WS_ACTIVITY_TIMEOUT = 20000;    // 20 секунд без активности
};

#endif // WSCLIENT_H
