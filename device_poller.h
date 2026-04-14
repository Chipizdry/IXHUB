


#ifndef DEVICE_POLLER_H
#define DEVICE_POLLER_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QQueue>
#include "ModbusMaster.h"

// Forward declaration
class Device;

// Структура для хранения задания на опрос
struct PollTask {
    int slaveId;
    int subTaskId;      // Для устройств с несколькими задачами
    QString taskName;   // Имя задачи для отладки
    QByteArray command; // Готовый Modbus команда для отправки
    bool isPriority;    // Флаг приоритетной команды

    PollTask() : slaveId(-1), subTaskId(0), isPriority(false) {}
    PollTask(int id, const QByteArray& cmd, const QString& name = "", int subId = 0, bool priority = false)
        : slaveId(id), subTaskId(subId), taskName(name), command(cmd), isPriority(priority) {}
};

class DevicePoller : public QObject
{
    Q_OBJECT
public:
    explicit DevicePoller(ModbusMaster* master, QObject *parent = nullptr);
    ~DevicePoller();

    void start(int intervalMs = 100);
    void stop();
    void setPollInterval(int ms);
    void refreshDeviceList();

    // Принудительный опрос конкретного устройства
    void pollDeviceNow(int slaveId, const QString& taskName = "");

    // НОВЫЙ МЕТОД: Отправка приоритетной команды (например, управление реле)
    void sendPriorityCommand(int slaveId, const QByteArray& command, const QString& commandName);
    int getPollInterval() const { return m_pollInterval; }

signals:
    void deviceDataReady(int slaveId, const QJsonObject& data);
    void deviceError(int slaveId, const QString& error);
    void pollingFinished();

private slots:
    void processNextTask();
    void onDataReady(const QByteArray& data);
    void onError(const QString& error);
    void onTimeout();

private:
    // Генерация задач для устройства
    QList<PollTask> generateTasksForDevice(Device* device);

    // Отправка команды
    void sendCommand(const PollTask& task);

    ModbusMaster* m_master;
    QTimer* m_pollTimer;
    QTimer* m_timeoutTimer;

    QQueue<PollTask> m_taskQueue;
    PollTask m_currentTask;

    int m_pollInterval;
    bool m_isWaiting;
    bool m_isRunning;

    // Статистика
    int m_totalTasksProcessed;
    int m_successCount;
    int m_timeoutCount;
    int m_errorCount;
};

#endif // DEVICE_POLLER_H



