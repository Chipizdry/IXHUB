



#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QJsonArray>
#include "device.h"

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    static DeviceManager& instance();

    // Управление устройствами
    void addDevice(Device* device);
    void removeDevice(int slaveId);
    Device* getDevice(int slaveId);
    QList<int> getActiveSlaves() const;
    QJsonArray getAllDevicesInfo() const;
    int getDeviceCount() const { return m_devices.size(); }

    // Загрузка/сохранение конфигурации
    bool loadConfig(const QString& filename);
    bool saveConfig(const QString& filename);

signals:
    void deviceAdded(int slaveId);
    void deviceRemoved(int slaveId);
    void deviceConfigChanged(int slaveId);

private:
    DeviceManager() = default;
    ~DeviceManager();
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    QMap<int, Device*> m_devices;
    QList<int> m_slaveOrder;
};

#endif // DEVICE_MANAGER_H


