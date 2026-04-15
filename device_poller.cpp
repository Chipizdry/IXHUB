


#include "device_poller.h"
#include "device_manager.h"
#include "device.h"
#include "relay_device.h"
#include "nta8a01_device.h"
#include "bldc_driver_device.h"
#include "vacuum_pressure_sensor.h"
#include <QDebug>
#include <QDateTime>


DevicePoller::DevicePoller(ModbusMaster* master, QObject *parent)
    : QObject(parent)
    , m_master(master)
    , m_pollInterval(100)
    , m_isWaiting(false)
    , m_isRunning(false)
    , m_totalTasksProcessed(0)
    , m_successCount(0)
    , m_timeoutCount(0)
    , m_errorCount(0)
{
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &DevicePoller::processNextTask);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &DevicePoller::onTimeout);

    connect(m_master, &ModbusMaster::dataReady, this, &DevicePoller::onDataReady);
    connect(m_master, &ModbusMaster::error, this, &DevicePoller::onError);
}

DevicePoller::~DevicePoller()
{
    stop();
}

// НОВЫЙ МЕТОД: Отправка приоритетной команды
void DevicePoller::sendPriorityCommand(int slaveId, const QByteArray& command, const QString& commandName)
{
    if (!m_isRunning) {
        qDebug() << "⚠️ DevicePoller not running, cannot send priority command";
        return;
    }

    // Создаем приоритетную задачу
    PollTask priorityTask(slaveId, command, commandName, 0, true);

    // Вставляем в начало очереди
    QQueue<PollTask> newQueue;
    newQueue.enqueue(priorityTask);

    // Добавляем остальные задачи
    for (const PollTask& task : m_taskQueue) {
        newQueue.enqueue(task);
    }

    m_taskQueue = newQueue;

    qDebug() << "⭐ PRIORITY command added:" << commandName
             << "(slave:" << slaveId << ") Queue size:" << m_taskQueue.size();

    // Если не ждем ответ, сразу начинаем обработку
    if (!m_isWaiting) {
        processNextTask();
    }
}

void DevicePoller::start(int intervalMs)
{
    if (m_isRunning) {
        qDebug() << "DevicePoller already running";
        return;
    }

    m_pollInterval = intervalMs;
    m_isRunning = true;
    m_totalTasksProcessed = 0;
    m_successCount = 0;
    m_timeoutCount = 0;
    m_errorCount = 0;

    refreshDeviceList();
    m_isWaiting = false;
    m_pollTimer->start(m_pollInterval);

    qDebug() << "🚀 DevicePoller started with interval:" << m_pollInterval << "ms";
}

void DevicePoller::stop()
{
    if (!m_isRunning) return;

    m_pollTimer->stop();
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    m_isWaiting = false;
    m_isRunning = false;
    m_taskQueue.clear();

    qDebug() << "🛑 DevicePoller stopped. Stats - Total:" << m_totalTasksProcessed
             << "Success:" << m_successCount
             << "Timeouts:" << m_timeoutCount
             << "Errors:" << m_errorCount;
}

void DevicePoller::setPollInterval(int ms)
{
    m_pollInterval = ms;
    if (m_isRunning && !m_isWaiting) {
        m_pollTimer->start(m_pollInterval);
    }
    qDebug() << "Poll interval changed to:" << m_pollInterval << "ms";
}

void DevicePoller::refreshDeviceList()
{
    m_taskQueue.clear();

    QList<int> activeSlaves = DeviceManager::instance().getActiveSlaves();

    for (int slaveId : activeSlaves) {
        Device* device = DeviceManager::instance().getDevice(slaveId);
        if (device) {
            QList<PollTask> tasks = generateTasksForDevice(device);
            for (const PollTask& task : tasks) {
                m_taskQueue.enqueue(task);
                qDebug() << "📋 Added task for slave" << slaveId
                         << ":" << task.taskName
                         << "(subtask:" << task.subTaskId << ")";
            }
        }
    }

    qDebug() << "🔄 Device list refreshed. Total tasks:" << m_taskQueue.size()
             << "for" << activeSlaves.size() << "devices";
}

void DevicePoller::pollDeviceNow(int slaveId, const QString& taskName)
{
    if (!m_isRunning) {
        qDebug() << "Poller not running, cannot poll device now";
        return;
    }

    Device* device = DeviceManager::instance().getDevice(slaveId);
    if (!device) {
        qDebug() << "Device not found for slave:" << slaveId;
        return;
    }

    // Создаем приоритетную задачу (вставляем в начало очереди)
    QList<PollTask> tasks = generateTasksForDevice(device);
    for (const PollTask& task : tasks) {
        if (taskName.isEmpty() || task.taskName == taskName) {
            m_taskQueue.prepend(task);
            qDebug() << "⏩ Priority task added for slave" << slaveId << ":" << task.taskName;
            break;
        }
    }

    // Если не ждем ответ, сразу начинаем обработку
    if (!m_isWaiting) {
        processNextTask();
    }
}

QList<PollTask> DevicePoller::generateTasksForDevice(Device* device)
{
    QList<PollTask> tasks;

    if (!device) return tasks;

    int slaveId = device->getSlaveId();
    Device::DeviceType type = device->getType();

    switch(type) {
        case Device::TYPE_NTA8A01:
               {
                   // Для датчика температуры - одна задача на чтение всех регистров
                   QList<quint16> registers = device->getRegisterAddresses();
                   if (!registers.isEmpty()) {
                       quint16 startReg = registers.first();
                       quint16 numRegs = registers.size();

                       // Формируем Modbus команду
                       QByteArray command;
                       command.append(static_cast<char>(slaveId));
                       command.append(static_cast<char>(0x03));  // Read Holding Registers
                       command.append(static_cast<char>(startReg >> 8));
                       command.append(static_cast<char>(startReg & 0xFF));
                       command.append(static_cast<char>(numRegs >> 8));
                       command.append(static_cast<char>(numRegs & 0xFF));

                       // Добавляем CRC используя метод из Device
                       command = Device::appendCRC(command);

                       qDebug() << "Modbus command with CRC:" << command.toHex();

                       tasks.append(PollTask(slaveId, command, "ReadRegisters", 0));
                   }
                   break;
               }

        case Device::TYPE_RELAY:
        {
            RelayDevice* relay = static_cast<RelayDevice*>(device);

            // Задача 0: Чтение состояния реле
            QByteArray relayCmd = relay->generateReadRelayStatusCommand();
            if (!relayCmd.isEmpty()) {
                tasks.append(PollTask(slaveId, relayCmd, "ReadRelayStatus", 0));
            }

            // Задача 1: Чтение состояния оптопар
            QByteArray optoCmd = relay->generateReadOptocouplerCommand();
            if (!optoCmd.isEmpty()) {
                tasks.append(PollTask(slaveId, optoCmd, "ReadOptocouplerStatus", 1));
            }
            break;
        }


        case Device::TYPE_BLDC_DRIVER:
        {
            // Для BLDC драйвера - читаем все 5 регистров
            QList<quint16> registers = device->getRegisterAddresses();
            if (!registers.isEmpty()) {
                quint16 startReg = registers.first();
                quint16 numRegs = registers.size();

                QByteArray command;
                command.append(static_cast<char>(slaveId));
                command.append(static_cast<char>(0x03));  // Read Holding Registers
                command.append(static_cast<char>(startReg >> 8));
                command.append(static_cast<char>(startReg & 0xFF));
                command.append(static_cast<char>(numRegs >> 8));
                command.append(static_cast<char>(numRegs & 0xFF));

                command = Device::appendCRC(command);

                tasks.append(PollTask(slaveId, command, "ReadRegisters", 0));
            }
            break;
        }

        case Device::TYPE_VACUUM_SENSOR:
        {
            VacuumPressureSensor* sensor = static_cast<VacuumPressureSensor*>(device);

            // Задача 0: Чтение PV значения (давления)
            QByteArray pvCmd = sensor->generateReadPvValueCommand();
            if (!pvCmd.isEmpty()) {
                tasks.append(PollTask(slaveId, pvCmd, "ReadPressure", 0));
            }

            // Задача 1: Чтение конфигурации (единицы, десятичные знаки)
            QByteArray configCmd = sensor->generateReadConfigCommand();
            if (!configCmd.isEmpty()) {
                tasks.append(PollTask(slaveId, configCmd, "ReadConfig", 1));
            }
            break;
        }


        default:
            qDebug() << "Unknown device type:" << type << "for slave" << slaveId;
            break;
    }

    return tasks;
}

void DevicePoller::sendCommand(const PollTask& task)
{
    if (!m_master) {
        qDebug() << "ModbusMaster is null";
        return;
    }

    qDebug() << "📤 Sending task:" << task.taskName
             << "(slave:" << task.slaveId
             << ", subtask:" << task.subTaskId
             << ", priority:" << (task.isPriority ? "YES" : "NO")
             << ", cmd:" << task.command.toHex() << ")";

    // Отправляем сырые данные
    m_master->sendRawData(task.slaveId, task.command);
}

void DevicePoller::processNextTask()
{
    if (m_isWaiting) {
        qDebug() << "⏳ Still waiting for response, skipping";
        return;
    }

    if (!m_isRunning) {
        qDebug() << "Poller is stopped";
        return;
    }

    // Если очередь пуста, пересоздаем задачи
    if (m_taskQueue.isEmpty()) {
        refreshDeviceList();
        if (m_taskQueue.isEmpty()) {
            qDebug() << "No tasks to process";
            QTimer::singleShot(m_pollInterval, this, &DevicePoller::processNextTask);
            return;
        }
    }

    // Берем следующую задачу
    m_currentTask = m_taskQueue.dequeue();

    qDebug() << "🔄 Processing task:" << m_currentTask.taskName
             << "(slave:" << m_currentTask.slaveId
             << ", priority:" << (m_currentTask.isPriority ? "YES" : "NO")
             << ", remaining:" << m_taskQueue.size() << ")";

    m_isWaiting = true;
    m_pollTimer->stop();
    m_timeoutTimer->start(500);  // 0.5 секунды таймаут

    sendCommand(m_currentTask);
}

void DevicePoller::onDataReady(const QByteArray& data)
{
    qDebug() << "✅ Response received for task:" << m_currentTask.taskName;

    if (m_timeoutTimer && m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }

    if (m_currentTask.slaveId != -1) {
        Device* device = DeviceManager::instance().getDevice(m_currentTask.slaveId);
        if (device) {
            QJsonObject result = device->processData(data);

            // Добавляем информацию о задаче в результат
            result["poll_task"] = m_currentTask.taskName;
            result["subtask_id"] = m_currentTask.subTaskId;

            emit deviceDataReady(m_currentTask.slaveId, result);
            m_successCount++;
            qDebug() << "✅ Device" << m_currentTask.slaveId << "data processed successfully";
        } else {
            qDebug() << "❌ Unknown device for slave:" << m_currentTask.slaveId;
            m_errorCount++;
        }
    }

    m_isWaiting = false;
    m_totalTasksProcessed++;

    // Запускаем следующую задачу
    if (m_isRunning) {
        m_pollTimer->start(m_pollInterval);
    }
}

void DevicePoller::onError(const QString& error)
{
    qDebug() << "❌ Error for task:" << m_currentTask.taskName << "-" << error;

    if (m_timeoutTimer && m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }

    if (m_currentTask.slaveId != -1) {
        if (!error.contains("Timeout")) {
            emit deviceError(m_currentTask.slaveId, error);
            m_errorCount++;
            qDebug() << "❌ Device error (slave:" << m_currentTask.slaveId << "):" << error;
        }
    }

    m_isWaiting = false;
    m_totalTasksProcessed++;

    if (m_isRunning) {
        m_pollTimer->start(m_pollInterval);
    }
}

void DevicePoller::onTimeout()
{
    qDebug() << "⏰ Timeout for task:" << m_currentTask.taskName;

    if (m_currentTask.slaveId != -1) {
        QString errorMsg = QString("Timeout - no response from slave %1 for task %2")
                          .arg(m_currentTask.slaveId)
                          .arg(m_currentTask.taskName);
        emit deviceError(m_currentTask.slaveId, errorMsg);
        m_timeoutCount++;
        qDebug() << "❌ Device timeout (slave:" << m_currentTask.slaveId
                 << ", task:" << m_currentTask.taskName << ")";
    }

    m_isWaiting = false;
    m_totalTasksProcessed++;

    if (m_isRunning) {
        m_pollTimer->start(m_pollInterval);
    }
}


