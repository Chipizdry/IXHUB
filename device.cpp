

#include "device.h"

Device::Device(int slaveId, DeviceType type, const QString& name, QObject *parent)
    : QObject(parent)
    , m_slaveId(slaveId)
    , m_type(type)
    , m_name(name)
{
}

Device::~Device()
{
}

QJsonObject Device::toJson() const
{
    QJsonObject obj;
    obj["slave_id"] = m_slaveId;
    obj["type"] = static_cast<int>(m_type);
    obj["type_name"] = [this]() {
        switch(m_type) {
            case TYPE_NTA8A01: return "NTA8A01";
            default: return "UNKNOWN";
        }
    }();
    obj["name"] = m_name;
    return obj;
}


quint16 Device::calculateCRC(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

QByteArray Device::appendCRC(const QByteArray& command)
{
    QByteArray result = command;
    quint16 crc = calculateCRC(command);
    result.append(static_cast<char>(crc & 0xFF));
    result.append(static_cast<char>(crc >> 8));
    return result;
}


