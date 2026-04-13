


#include "nta8a01_device.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

NTA8A01Device::NTA8A01Device(int slaveId, QObject *parent)
    : Device(slaveId, TYPE_NTA8A01, QString("NTA8A01_Slave_%1").arg(slaveId), parent)
    , m_temperature(-273.1f)
    , m_rs485Address(slaveId)
    , m_baudRate(9600)
    , m_temperatureCorrection(0.0f)
    , m_hasSensor(false)
{
}

QList<quint16> NTA8A01Device::getRegisterAddresses()
{
    // Читаем 5 регистров подряд с 0x0000 по 0x0004
    // Это даст нам: 0x0000(темп), 0x0001(не используется), 0x0002(адрес), 0x0003(бод), 0x0004(коррекция)
    return {0x0000, 0x0001, 0x0002, 0x0003, 0x0004};
}

QJsonObject NTA8A01Device::processData(const QByteArray& data)
{
    QJsonObject result;
    result["device_type"] = "NTA8A01";
    result["slave_id"] = m_slaveId;
    result["name"] = m_name;

    qDebug() << "NTA8A01: Raw data size:" << data.size() << "bytes, hex:" << data.toHex();

    QByteArray regData;
    int dataOffset = 0;

    // Проверяем, не является ли это полным Modbus ответом
    if (data.size() >= 3 && (data[0] == m_slaveId || data[0] == 0xFF)) {
        int byteCount = static_cast<quint8>(data[2]);
        dataOffset = 3;

        if (data.size() >= dataOffset + byteCount) {
            regData = data.mid(dataOffset, byteCount);
            qDebug() << "NTA8A01: Full Modbus response detected, byteCount=" << byteCount
                     << ", regData:" << regData.toHex();
        } else {
            qDebug() << "NTA8A01: Incomplete Modbus response";
            return result;
        }
    } else {
        regData = data;
        qDebug() << "NTA8A01: Assuming raw register data, size:" << regData.size();
    }

    // Ожидаем 10 байт данных для 5 регистров (0x0000-0x0004)
    if (regData.size() >= 10) {
        // Регистр 0x0000: Температура (байты 0-1)
        qint16 tempRaw = (static_cast<qint16>(static_cast<quint8>(regData[0])) << 8) |
                          static_cast<quint8>(regData[1]);
        m_temperature = parseTemperature(tempRaw);
        result["temperature_celsius"] = m_temperature;
        result["temperature_raw"] = tempRaw;

        // Регистр 0x0001: НЕ ИСПОЛЬЗУЕТСЯ (байты 2-3) - пропускаем
        quint16 unusedReg = (static_cast<quint16>(static_cast<quint8>(regData[2])) << 8) |
                             static_cast<quint8>(regData[3]);
        qDebug() << "NTA8A01: Unused register (0x0001) value:" << unusedReg;

        // Регистр 0x0002: RS485 адрес (байты 4-5)
        quint16 addrRaw = (static_cast<quint16>(static_cast<quint8>(regData[4])) << 8) |
                           static_cast<quint8>(regData[5]);

        qDebug() << "NTA8A01: addrRaw=" << addrRaw;

        // Адрес должен быть в диапазоне 1-247
        if (addrRaw >= 1 && addrRaw <= 247) {
            m_rs485Address = static_cast<int>(addrRaw);
        } else {
            qDebug() << "NTA8A01: Unexpected RS485 address value:" << addrRaw
                     << "for slave" << m_slaveId << "- keeping existing:" << m_rs485Address;
        }
        result["rs485_address"] = m_rs485Address;

        // Регистр 0x0003: Скорость бода (байты 6-7)
        quint16 baudRaw = (static_cast<quint16>(static_cast<quint8>(regData[6])) << 8) |
                           static_cast<quint8>(regData[7]);

        qDebug() << "NTA8A01: baudRaw=" << baudRaw;

        m_baudRate = parseBaudRate(baudRaw);
        result["baud_rate"] = m_baudRate;
        result["baud_rate_raw"] = baudRaw;

        // Регистр 0x0004: Коррекция температуры (байты 8-9)
        qint16 correctionRaw = (static_cast<qint16>(static_cast<quint8>(regData[8])) << 8) |
                                static_cast<quint8>(regData[9]);
        m_temperatureCorrection = correctionRaw / 10.0f;
        result["temperature_correction"] = m_temperatureCorrection;

        // Статус датчика
        m_hasSensor = (tempRaw != NO_SENSOR_VALUE);
        result["sensor_connected"] = m_hasSensor;
        result["sensor_status"] = m_hasSensor ? "ok" : "no_sensor_or_error";

        qDebug() << QString("NTA8A01 Slave %1: Temp=%2°C (raw=%3), Addr=%4, Baud=%5 (%6), Corr=%7°C, Sensor=%8")
                    .arg(m_slaveId)
                    .arg(m_temperature, 0, 'f', 1)
                    .arg(tempRaw)
                    .arg(m_rs485Address)
                    .arg(m_baudRate)
                    .arg(baudRaw)
                    .arg(m_temperatureCorrection, 0, 'f', 1)
                    .arg(m_hasSensor ? "OK" : "NO");

    } else if (regData.size() >= 2) {
        // Только температура
        qint16 tempRaw = (static_cast<qint16>(static_cast<quint8>(regData[0])) << 8) |
                          static_cast<quint8>(regData[1]);
        m_temperature = parseTemperature(tempRaw);
        result["temperature_celsius"] = m_temperature;
        result["temperature_raw"] = tempRaw;

        m_hasSensor = (tempRaw != NO_SENSOR_VALUE);
        result["sensor_connected"] = m_hasSensor;
        result["sensor_status"] = m_hasSensor ? "ok" : "no_sensor_or_error";

        qDebug() << QString("NTA8A01 Slave %1: Temp=%2°C (raw=%3), Sensor=%4")
                    .arg(m_slaveId)
                    .arg(m_temperature, 0, 'f', 1)
                    .arg(tempRaw)
                    .arg(m_hasSensor ? "OK" : "NO");
    } else {
        qDebug() << QString("NTA8A01 Slave %1: Invalid data size %2")
                    .arg(m_slaveId)
                    .arg(regData.size());
    }

    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return result;
}

float NTA8A01Device::parseTemperature(qint16 raw)
{
    if (raw == NO_SENSOR_VALUE) {
        return -273.1f;
    }

    float temp = raw / 10.0f;

    if (temp > 150.0f || temp < -50.0f) {
        qDebug() << QString("NTA8A01: Suspicious temperature %1°C (raw=%2)")
                    .arg(temp)
                    .arg(raw);
    }

    return temp;
}

int NTA8A01Device::parseBaudRate(quint16 raw)
{
    qDebug() << "NTA8A01: parseBaudRate called with raw=" << raw;

    switch(raw) {
        case 0: return 1200;
        case 1: return 2400;
        case 2: return 4800;
        case 3: return 9600;
        case 4: return 19200;
        case 5:
            qDebug() << "NTA8A01: Factory reset command detected";
            return 9600;
        default:
            qDebug() << "NTA8A01: Unknown baud rate value:" << raw << ", using default 9600";
            return 9600;
    }
}

QJsonObject NTA8A01Device::toJson() const
{
    QJsonObject obj = Device::toJson();
    obj["temperature"] = m_temperature;
    obj["rs485_address"] = m_rs485Address;
    obj["baud_rate"] = m_baudRate;
    obj["temperature_correction"] = m_temperatureCorrection;
    obj["sensor_connected"] = m_hasSensor;
    return obj;
}


