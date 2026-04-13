



#ifndef NTA8A01_DEVICE_H
#define NTA8A01_DEVICE_H

#include "device.h"

class NTA8A01Device : public Device
{
    Q_OBJECT
public:
    enum RegisterMap {
        REG_TEMPERATURE = 0x0000,      // Температура
        REG_RS485_ADDR = 0x0002,       // RS485 адрес
        REG_BAUD_RATE = 0x0003,        // Скорость бода
        REG_TEMP_CORRECTION = 0x0004   // Коррекция температуры
    };

    explicit NTA8A01Device(int slaveId, QObject *parent = nullptr);

    // Реализация виртуальных методов
    QList<quint16> getRegisterAddresses() override;
    QJsonObject processData(const QByteArray& data) override;
    QJsonObject toJson() const override;

    // Специфичные методы для NTA8A01
    float getTemperature() const { return m_temperature; }
    int getRs485Address() const { return m_rs485Address; }
    int getBaudRate() const { return m_baudRate; }
    float getTemperatureCorrection() const { return m_temperatureCorrection; }
    bool hasSensor() const { return m_hasSensor; }

private:
    float parseTemperature(qint16 raw);  // Изменено: qint16 вместо quint16
    int parseBaudRate(quint16 raw);

    // Константы
    static constexpr quint16 NO_SENSOR_VALUE = 0xF555;
    static constexpr quint16 INVALID_ADDR_VALUE = 0xFFFF;

    // Кэшированные значения
    float m_temperature;
    int m_rs485Address;
    int m_baudRate;
    float m_temperatureCorrection;
    bool m_hasSensor;
};

#endif // NTA8A01_DEVICE_H


