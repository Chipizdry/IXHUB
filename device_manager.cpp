


#include "device_manager.h"
#include "nta8a01_device.h"
#include "relay_device.h"
#include "bldc_driver_device.h"
#include "vacuum_pressure_sensor.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDebug>

DeviceManager& DeviceManager::instance()
{
    static DeviceManager instance;
    return instance;
}

DeviceManager::~DeviceManager()
{
    for (auto device : m_devices) {
        delete device;
    }
}

void DeviceManager::addDevice(Device* device)
{
    if (!device) return;

    int slaveId = device->getSlaveId();
    if (m_devices.contains(slaveId)) {
        qDebug() << "Device with slave ID" << slaveId << "already exists, replacing";
        delete m_devices[slaveId];
        m_devices.remove(slaveId);
        m_slaveOrder.removeAll(slaveId);
    }

    m_devices[slaveId] = device;
    m_slaveOrder.append(slaveId);

    emit deviceAdded(slaveId);
    qDebug() << "Device added:" << device->getName() << "(slave:" << slaveId << ")";
}

void DeviceManager::removeDevice(int slaveId)
{
    if (m_devices.contains(slaveId)) {
        delete m_devices[slaveId];
        m_devices.remove(slaveId);
        m_slaveOrder.removeAll(slaveId);
        emit deviceRemoved(slaveId);
        qDebug() << "Device removed. Slave:" << slaveId;
    }
}

Device* DeviceManager::getDevice(int slaveId)
{
    return m_devices.value(slaveId, nullptr);
}

QList<int> DeviceManager::getActiveSlaves() const
{
    return m_slaveOrder;
}

QJsonArray DeviceManager::getAllDevicesInfo() const
{
    QJsonArray arr;
    for (int slaveId : m_slaveOrder) {
        if (m_devices.contains(slaveId)) {
            arr.append(m_devices[slaveId]->toJson());
        }
    }
    return arr;
}

/*
bool DeviceManager::loadConfig(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open config file:" << filename;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qDebug() << "Invalid JSON config";
        return false;
    }

    QJsonArray devices = doc.array();
    for (const auto& item : devices) {
        QJsonObject obj = item.toObject();
        int slaveId = obj["slave_id"].toInt();
        int type = obj["type"].toInt();
        QString name = obj["name"].toString();

        Device* device = nullptr;
        if (type == Device::TYPE_NTA8A01) {
            device = new NTA8A01Device(slaveId);
            device->setName(name);
            addDevice(device);
        }
    }

    qDebug() << "Loaded" << m_devices.size() << "devices from config";
    return true;
}
*/

bool DeviceManager::loadConfig(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open config file:" << filename;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qDebug() << "Invalid JSON config";
        return false;
    }

    QJsonArray devices = doc.array();
    for (const auto& item : devices) {
        QJsonObject obj = item.toObject();
        int slaveId = obj["slave_id"].toInt();
        int type = obj["type"].toInt();
        QString name = obj["name"].toString();
        QString role = obj["role"].toString();

        Device* device = nullptr;

        switch(type) {
            case Device::TYPE_NTA8A01:
                device = new NTA8A01Device(slaveId);
                break;

            case Device::TYPE_RELAY:
                device = new RelayDevice(slaveId);
                if (!role.isEmpty()) {
                    static_cast<RelayDevice*>(device)->setRole(role);
                }
                // Загружаем имена реле
                if (obj.contains("relay_names")) {
                    QJsonObject relayNamesObj = obj["relay_names"].toObject();
                    QMap<int, QString> relayMap;
                    for (auto it = relayNamesObj.begin(); it != relayNamesObj.end(); ++it) {
                        int relayNum = it.key().toInt();
                        QString relayName = it.value().toString();
                        relayMap[relayNum] = relayName;
                    }
                    static_cast<RelayDevice*>(device)->loadRelayMapping(relayMap);
                }
                break;

            case Device::TYPE_BLDC_DRIVER:
                device = new BldcDriverDevice(slaveId);
                break;

            case Device::TYPE_VACUUM_SENSOR:
                device = new VacuumPressureSensor(slaveId);
                break;

            default:
                qDebug() << "Unknown device type:" << type;
                continue;
        }

        if (device) {
            device->setName(name);

            // Если устройство с таким slaveId уже существует, заменяем
            if (m_devices.contains(slaveId)) {
                delete m_devices[slaveId];
                m_devices.remove(slaveId);
                m_slaveOrder.removeAll(slaveId);
            }

            addDevice(device);
            qDebug() << "Loaded device from config:" << name << "(slave:" << slaveId << ", type:" << type << ")";
        }
    }

    qDebug() << "Loaded" << m_devices.size() << "devices from config";
    return true;
}




bool DeviceManager::saveConfig(const QString& filename)
{
    QJsonArray arr = getAllDevicesInfo();
    QJsonDocument doc(arr);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Cannot save config to:" << filename;
        return false;
    }

    file.write(doc.toJson());
    qDebug() << "Saved" << m_devices.size() << "devices to config";
    return true;
}



