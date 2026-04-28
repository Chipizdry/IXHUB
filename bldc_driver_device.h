


#ifndef BLDC_DRIVER_DEVICE_H
#define BLDC_DRIVER_DEVICE_H

#include "device.h"
#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QTimer>

class BldcDriverDevice : public Device
{
    Q_OBJECT

public:
    // Modbus function codes
    static constexpr quint8 FC_READ_HOLDING = 0x03;
    static constexpr quint8 FC_WRITE_SINGLE = 0x06;
    static constexpr quint8 FC_WRITE_MULTIPLE = 0x10;

    // Универсальные адреса регистров (чтение и запись)
    static constexpr quint16 REG_FREQ_HI = 0x0000;      // Частота ШИМ в Гц (старшая часть)
    static constexpr quint16 REG_FREQ_LO = 0x0001;      // Частота ШИМ в Гц (младшая часть)
    static constexpr quint16 REG_RPM = 0x0002;          // Обороты/мин
    static constexpr quint16 REG_TIMER_ARR = 0x0003;    // TIM1->ARR (период таймера)
    static constexpr quint16 REG_PWM_CH1 = 0x0004;      // PWM значение канал 1 (TIM1->CCR1)
    static constexpr quint16 REG_PWM_CH2 = 0x0005;      // PWM значение канал 2 (TIM1->CCR2)
    static constexpr quint16 REG_PWM_CH3 = 0x0006;      // PWM значение канал 3 (TIM1->CCR3)
    static constexpr quint16 REG_STATUS = 0x0007;       // Статус (coils, auto_mode, pwr_on)
    static constexpr quint16 REG_PWM_GEN = 0x0008;      // PWM значение в режиме генерации

    // Диапазоны
    static constexpr quint16 PWM_MIN = 0;
    static constexpr quint16 PWM_MAX = 4000;
    static constexpr quint16 TIMER_ARR_MIN = 0;
    static constexpr quint16 TIMER_ARR_MAX = 65535;

    explicit BldcDriverDevice(int slaveId, QObject *parent = nullptr);
    ~BldcDriverDevice();

    // Device interface
    QList<quint16> getRegisterAddresses() override;
    QJsonObject processData(const QByteArray& data) override;
    QJsonObject toJson() const override;

    // Генерация команд
    QByteArray generateReadAllRegistersCommand() const;
    QByteArray generateReadRegisterCommand(quint16 regAddr) const;
    QByteArray generateWriteSingleCommand(quint16 regAddr, quint16 value) const;
    QByteArray generateWriteMultipleCommand(const QList<quint16>& addresses, const QList<quint16>& values) const;

    // Запись всех изменённых регистров
    void flushWriteCache();

    // Управление двигателем
    void setTargetPwm(quint16 value);      // Установка PWM (все три канала)
    void setTimerArr(quint16 value);       // Установка периода таймера
    void setPwmGen(quint16 value);
    void setStatus(quint8 status);         // Установка статуса
    void setAutoMode(bool enabled);
    void setPowerOn(bool on);
    void setSpeed(quint16 rpm);            // Установка целевых оборотов
    void setBldcMode(bool genMode);

    void readAllRegisters();

    // Геттеры
    quint16 getFrequencyHz() const { return m_frequencyHz; }
    quint16 getTimerArr() const { return m_timerArr; }
    quint16 getPwmValue() const { return m_pwmValue; }
    quint16 getRpm() const { return m_rpm; }
    quint16 getPwmGen() const { return m_targetPwmGen; }
    bool getPowerOn() const { return m_powerOn; }
    quint8 getStatusByte() const;

signals:
    void commandGenerated(const QByteArray& command);
    void dataUpdated();

private slots:
    void onWriteTimerTimeout();

private:
    void parseHoldingRegisters(const QByteArray& data);
    void parseWriteResponse(const QByteArray& data);
    void markDirty(quint16 regAddr, quint16 value);
    void scheduleWrite();
    void updateFromStatusByte();

    // Данные устройства (чтение)
    quint16 m_frequencyHz;   // из регистров 0x0000-0x0001
    quint16 m_rpm;           // из регистра 0x0002
    quint16 m_timerArr;      // из регистра 0x0003
    quint16 m_pwmValue;      // из регистра 0x0004 (значение ШИМ)
    quint8  m_statusByte;    // из регистра 0x0007

    // Кэш для записи
    quint16 m_targetPwm;       // Целевое значение ШИМ (0x0004)
    quint16 m_targetTimerArr;  // Целевой период таймера (0x0003)
    quint8  m_targetStatus;    // Целевой статус (0x0007)
    quint16 m_targetPwmGen;
    quint16 m_pwmGenValue;

    bool m_pwmDirty;
    bool m_timerArrDirty;
    bool m_statusDirty;
    bool m_pwmGenDirty;

    QTimer* m_writeTimer;
    static constexpr int WRITE_DEBOUNCE_MS = 50;
    static constexpr int NUM_REGISTERS = 9;  // Читаем 9 регистров (0x0000 - 0x0008)
    // Состояния (из регистра 0x0007)
    bool m_coil1;
    bool m_coil2;
    bool m_coil3;
    bool m_coil4;
    bool m_autoMode;
    bool m_powerOn;
    bool m_bldcMode;
    bool m_genMode;

};

#endif // BLDC_DRIVER_DEVICE_H


