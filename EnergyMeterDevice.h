


#ifndef ENERGYMETERDEVICE_H
#define ENERGYMETERDEVICE_H

#include "device.h"

class EnergyMeterDevice : public Device
{
    Q_OBJECT
public:
    explicit EnergyMeterDevice(int slaveId, QObject *parent = nullptr);

    // Device interface
    QList<quint16> getRegisterAddresses() override;
    QJsonObject processData(const QByteArray &data) override;
    QJsonObject toJson() const override;

    // Getters для текущих значений
    float current_mA() const { return m_current_mA; }
    float voltage() const { return m_voltage; }      // предположительно reg0
    float temperature() const { return m_temperature; }
    quint16 adcChannel3() const { return m_adc3; }
    quint16 adcChannel4() const { return m_adc4; }
    quint16 adcChannel5() const { return m_adc5; }
    quint16 adcChannel6() const { return m_adc6; }

private:
    float parseTemperature(qint16 raw);

    float m_voltage;          // регистр 0 (может быть масштабирован)
    float m_current_mA;       // регистр 1 (ток в мА)
    float m_temperature;      // регистр 2 (в градусах Цельсия)
    quint16 m_adc3;           // регистр 3
    quint16 m_adc4;           // регистр 4
    quint16 m_adc5;           // регистр 5
    quint16 m_adc6;           // регистр 6
};

#endif // ENERGYMETERDEVICE_H


