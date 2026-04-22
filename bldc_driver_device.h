


#ifndef BLDC_DRIVER_DEVICE_H
#define BLDC_DRIVER_DEVICE_H

#include "device.h"
#include <QObject>
#include <QJsonObject>
#include <QList>

class BldcDriverDevice : public Device
{
    Q_OBJECT

public:
    // Modbus function codes
    static constexpr quint8 FC_READ_HOLDING = 0x03;
    static constexpr quint8 FC_WRITE_SINGLE = 0x06;
    static constexpr quint8 FC_WRITE_MULTIPLE = 0x10;

    // Register addresses
    static constexpr quint16 REG_PWM = 0x0000;  // Целевое значение ШИМ (запись)
    static constexpr quint16 REG_PWM_HZ = 0x0001;       // Частота ШИМ (Гц)
    static constexpr quint16 REG_RPM = 0x0002;          // Обороты/мин
    static constexpr quint16 REG_TIMER_ARR = 0x0003;    // TIM1->ARR (период таймера)
    static constexpr quint16 REG_PWM_VALUE = 0x0004;    // Значение ШИМ
    static constexpr quint16 REG_ANGLE_PHAZE = 0x0005;  // Угол опережения
    //static constexpr quint16 REG_PWM_VALUE = 0x0006;    // Значение ШИМ


    explicit BldcDriverDevice(int slaveId, QObject *parent = nullptr);
    ~BldcDriverDevice();

    // Device interface
    QList<quint16> getRegisterAddresses() override;
    QJsonObject processData(const QByteArray& data) override;
    QJsonObject toJson() const override;

    // Генерация команд для чтения
    QByteArray generateReadAllRegistersCommand() const;
    QByteArray generateReadRegisterCommand(quint16 regAddr) const;

    // Генерация команд для записи
    QByteArray generateWriteRegisterCommand(quint16 regAddr, quint16 value) const;
    QByteArray generateWriteMultipleRegistersCommand(const QList<quint16>& addresses, const QList<quint16>& values) const;

    // Управление двигателем
    void setPwmKhz(quint16 khz);           // Установка частоты ШИМ (кГц)
    void setPwmHz(quint16 hz);             // Установка частоты ШИМ (Гц)
    void setPwmValue(quint16 value);       // Установка значения ШИМ (0-1000)
    void setSpeed(quint16 rpm);            // Установка целевых оборотов
    void setCoils(quint8 coilState);       // Установка состояния катушек (биты 1-4)
    void setAutoMode(bool enabled);        // Автоматический режим
    void setPowerOn(bool on);              // Включение/выключение питания

    // Чтение состояния
    void readAllRegisters();
    void readRpm();

    // Геттеры
    quint16 getPwmKhz() const { return m_pwmKhz; }
    quint16 getPwmHz() const { return m_pwmHz; }
    quint16 getRpm() const { return m_rpm; }
    quint16 getTimerArr() const { return m_timerArr; }
    quint16 getPwmValue() const { return m_pwmValue; }
    bool getCoil1() const { return m_coil1; }
    bool getCoil2() const { return m_coil2; }
    bool getCoil3() const { return m_coil3; }
    bool getCoil4() const { return m_coil4; }
    bool getAutoMode() const { return m_autoMode; }
    bool getPowerOn() const { return m_powerOn; }
    quint8 getCoilsByte() const;

signals:
    void commandGenerated(const QByteArray& command);
    void dataUpdated();

private:
    void parseHoldingRegisters(const QByteArray& data);
    void parseCoilsAndStatus(quint8 statusByte);
    void parseWriteResponse(const QByteArray& data);

    // Данные устройства
    quint16 m_pwmKhz;       // Частота ШИМ (кГц)
    quint16 m_pwmHz;        // Частота ШИМ (Гц)
    quint16 m_rpm;          // Текущие обороты
    quint16 m_timerArr;     // Период таймера
    quint16 m_pwmValue;     // Значение ШИМ (0-1000)

    // Состояния (из 8-го регистра)
    bool m_coil1;           // Бит 1
    bool m_coil2;           // Бит 2
    bool m_coil3;           // Бит 3
    bool m_coil4;           // Бит 4
    bool m_autoMode;        // Бит 5 - автоматический режим
    bool m_powerOn;         // Бит 6 - питание включено

    static constexpr int NUM_REGISTERS = 5;
};

#endif // BLDC_DRIVER_DEVICE_H


