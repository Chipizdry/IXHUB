


#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <QObject>
#include <QJsonObject>
#include "WSClient.h"
#include "device_poller.h"
#include "ModbusMaster.h"

class WebSocketManager : public QObject
{
    Q_OBJECT

public:
    static WebSocketManager& instance();

    bool initialize(const QString& serialNumber);
    void cleanup();

    // Отправка данных
    void sendDeviceData(int slaveId, const QJsonObject& data);
    void sendDeviceError(int slaveId, const QString& error);
    void sendDeviceList();

    // Статус
    bool isConnected() const { return m_ws ? m_ws->isConnected() : false; }
    bool isAuthenticated() const { return m_authenticated; }

private:
    WebSocketManager();
    ~WebSocketManager();
    WebSocketManager(const WebSocketManager&) = delete;
    WebSocketManager& operator=(const WebSocketManager&) = delete;

    void setupConnections();
    void setupPollerConnections();
    void setupWSConnections();

    // Обработчики
    void handleSettingsCommand(const QString& commandType, const QJsonObject& settings);
    void logDeviceData(int slaveId, const QJsonObject& data);

    WSClient* m_ws;
    DevicePoller* m_poller;
    ModbusMaster* m_master;
    bool m_authenticated;
    QString m_serialNumber;
};

#endif // WEBSOCKET_MANAGER_H


