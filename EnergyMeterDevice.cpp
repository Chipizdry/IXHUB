



#include "EnergyMeterDevice.h"
#include <QDebug>
#include <QDateTime>

static const qint16 NO_SENSOR_VALUE = 0x8000; // -32768, признак отсутствия датчика

EnergyMeterDevice::EnergyMeterDevice(int slaveId, QObject *parent)
    : Device(slaveId, TYPE_ENERGY_METER, QString("EnergyMeter_%1").arg(slaveId), parent)
    , m_voltage(0.0f)
    , m_current_mA(0.0f)
    , m_temperature(-273.1f)
    , m_adc3(0)
    , m_adc4(0)
    , m_adc5(0)
    , m_adc6(0)
{
}

QList<quint16> EnergyMeterDevice::getRegisterAddresses()
{
    // читаем 7 регистров подряд: 0x00 – 0x06
    return {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006};
}

QJsonObject EnergyMeterDevice::processData(const QByteArray &data)
{
    QJsonObject result;
    result["device_type"] = "EnergyMeter";
    result["slave_id"] = m_slaveId;
    result["name"] = m_name;

    qDebug() << "EnergyMeter: Raw data size:" << data.size() << "bytes, hex:" << data.toHex();

    QByteArray regData;
    int dataOffset = 0;

    // Проверка на полный Modbus ответ (с адресом, функцией, счетчиком байт)
    if (data.size() >= 3 && (data[0] == m_slaveId || data[0] == 0xFF)) {
        int byteCount = static_cast<quint8>(data[2]);
        dataOffset = 3;
        if (data.size() >= dataOffset + byteCount) {
            regData = data.mid(dataOffset, byteCount);
            qDebug() << "EnergyMeter: Full Modbus response, byteCount=" << byteCount
                     << ", regData:" << regData.toHex();
        } else {
            qDebug() << "EnergyMeter: Incomplete Modbus response";
            return result;
        }
    } else {
        regData = data;
        qDebug() << "EnergyMeter: Assuming raw register data, size:" << regData.size();
    }

    // Ожидаем 14 байт для 7 регистров (2 байта на регистр)
    if (regData.size() >= 14) {
        // Регистр 0x0000: напряжение (или ток) - как uint16, преобразуем в float если нужно
        quint16 reg0 = (static_cast<quint16>(static_cast<quint8>(regData[0])) << 8) |
                       static_cast<quint8>(regData[1]);
        // Пример масштабирования: если регистр содержит значение 0-1000 = 0-100.0 В
        m_voltage = reg0 * 0.1f;   // предположим, что 1 LSB = 0.1 В
        result["voltage_V"] = m_voltage;
        result["voltage_raw"] = reg0;

        // Регистр 0x0001: ток в мА (uint16)
        quint16 reg1 = (static_cast<quint16>(static_cast<quint8>(regData[2])) << 8) |
                       static_cast<quint8>(regData[3]);
        m_current_mA = reg1;
        result["current_mA"] = m_current_mA;
        result["current_A"] = m_current_mA / 1000.0f;

        // Регистр 0x0002: температура в 0.1°C (int16)
        qint16 tempRaw = (static_cast<qint16>(static_cast<quint8>(regData[4])) << 8) |
                         static_cast<quint8>(regData[5]);
        m_temperature = parseTemperature(tempRaw);
        result["temperature_celsius"] = m_temperature;
        result["temperature_raw"] = tempRaw;

        // Регистры 0x0003 – 0x0006: дополнительные каналы ADC
        quint16 reg3 = (static_cast<quint16>(static_cast<quint8>(regData[6])) << 8) |
                       static_cast<quint8>(regData[7]);
        quint16 reg4 = (static_cast<quint16>(static_cast<quint8>(regData[8])) << 8) |
                       static_cast<quint8>(regData[9]);
        quint16 reg5 = (static_cast<quint16>(static_cast<quint8>(regData[10])) << 8) |
                       static_cast<quint8>(regData[11]);
        quint16 reg6 = (static_cast<quint16>(static_cast<quint8>(regData[12])) << 8) |
                       static_cast<quint8>(regData[13]);

        m_adc3 = reg3;
        m_adc4 = reg4;
        m_adc5 = reg5;
        m_adc6 = reg6;

        result["adc_channel_3"] = m_adc3;
        result["adc_channel_4"] = m_adc4;
        result["adc_channel_5"] = m_adc5;
        result["adc_channel_6"] = m_adc6;

        qDebug() << QString("EnergyMeter Slave %1: U=%2V, I=%3mA, T=%4°C, ADC3=%5, ADC4=%6, ADC5=%7, ADC6=%8")
                    .arg(m_slaveId)
                    .arg(m_voltage, 0, 'f', 1)
                    .arg(m_current_mA)
                    .arg(m_temperature, 0, 'f', 1)
                    .arg(m_adc3)
                    .arg(m_adc4)
                    .arg(m_adc5)
                    .arg(m_adc6);
    } else {
        qDebug() << QString("EnergyMeter Slave %1: Invalid data size %2")
                    .arg(m_slaveId)
                    .arg(regData.size());
    }

    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return result;
}

float EnergyMeterDevice::parseTemperature(qint16 raw)
{
    if (raw == NO_SENSOR_VALUE) {
        return -273.1f;
    }
    float temp = raw / 10.0f;
    if (temp > 150.0f || temp < -50.0f) {
        qDebug() << QString("EnergyMeter: Suspicious temperature %1°C (raw=%2)")
                    .arg(temp)
                    .arg(raw);
    }
    return temp;
}

QJsonObject EnergyMeterDevice::toJson() const
{
    QJsonObject obj = Device::toJson();
    obj["voltage_V"] = m_voltage;
    obj["current_mA"] = m_current_mA;
    obj["temperature_celsius"] = m_temperature;
    obj["adc_channel_3"] = m_adc3;
    obj["adc_channel_4"] = m_adc4;
    obj["adc_channel_5"] = m_adc5;
    obj["adc_channel_6"] = m_adc6;
    return obj;
}


