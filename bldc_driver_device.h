


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

    // Register addresses (соответствуют прошивке)
    static constexpr quint16 REG_PWM = 0x0000;           // Значение ШИМ (запись/чтение)
    static constexpr quint16 REG_PWM_HZ = 0x0001;        // Частота ШИМ (Гц)
    static constexpr quint16 REG_RPM = 0x0002;           // Обороты/мин (чтение)
    static constexpr quint16 REG_TIMER_ARR = 0x0003;     // TIM1->ARR (период таймера)
    static constexpr quint16 REG_PWM_VALUE = 0x0004;     // Значение ШИМ (только чтение)
    static constexpr quint16 REG_ANGLE_PHAZE = 0x0005;   // Угол опережения
    static constexpr quint16 REG_STATUS = 0x0007;        // Статус (coils, auto_mode, pwr_on)

    // Диапазоны
    static constexpr quint16 PWM_MIN = 0;
    static constexpr quint16 PWM_MAX = 2000;
    static constexpr quint16 TIMER_ARR_MIN = 0;
    static constexpr quint16 TIMER_ARR_MAX = 65535;

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

    // Запись всех изменённых регистров одной командой
    void flushWriteCache();

    // Управление двигателем (кэшируют значения и запускают таймер отправки)
    void setTargetPwm(quint16 value);      // Установка ШИМ (0-2000)
    void setTimerArr(quint16 value);       // Установка периода таймера
    void setStatus(quint8 status);         // Установка статуса
    void setAutoMode(bool enabled);        // Автоматический режим
    void setPowerOn(bool on);              // Включение/выключение питания

    // Старые методы для совместимости
    void setPwmValue(quint16 value);       // Установка значения ШИМ (0-2000)
    void setPwmKhz(quint16 khz);           // Установка частоты ШИМ (кГц)
    void setPwmHz(quint16 hz);             // Установка частоты ШИМ (Гц)
    void setSpeed(quint16 rpm);            // Установка целевых оборотов
    void setCoils(quint8 coilState);       // Установка состояния катушек (биты 1-4)

    // Чтение состояния
    void readAllRegisters();
    void readRpm();

    // Геттеры
    quint16 getPwmKhz() const { return m_pwmKhz; }
    quint16 getPwmHz() const { return m_pwmHz; }
    quint16 getRpm() const { return m_rpm; }
    quint16 getTimerArr() const { return m_timerArr; }
    quint16 getPwmValue() const { return m_pwmValue; }
    quint16 getTargetPwm() const { return m_targetPwm; }
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

private slots:
    void onWriteTimerTimeout();

private:
    void parseHoldingRegisters(const QByteArray& data);
    void parseCoilsAndStatus(quint8 statusByte);
    void parseWriteResponse(const QByteArray& data);
    void markDirty(quint16 regAddr, quint16 value);
    void scheduleWrite();

    // Данные устройства (чтение)
    quint16 m_pwmKhz;
    quint16 m_pwmHz;
    quint16 m_rpm;
    quint16 m_timerArr;
    quint16 m_pwmValue;


    qreal targetFrequencyKhz;
    qreal currentFrequencyKhz;
    qreal targetDutyPercent;
    qreal currentDutyPercent;


    // Кэш для записи
    quint16 m_targetPwm;            // Целевое значение ШИМ (0-2000)
    quint16 m_targetTimerArr;       // Целевой период таймера
    quint8  m_targetStatus;         // Целевой статус

    // Флаги изменений
    bool m_pwmDirty;
    bool m_timerArrDirty;
    bool m_statusDirty;

    // Таймер для групповой отправки
    QTimer* m_writeTimer;
    static constexpr int WRITE_DEBOUNCE_MS = 50;  // Задержка перед отправкой

    // Состояния (из регистра 0x0007)
    bool m_coil1;
    bool m_coil2;
    bool m_coil3;
    bool m_coil4;
    bool m_autoMode;
    bool m_powerOn;

    static constexpr int NUM_REGISTERS = 8;  // Читаем 8 регистров
};

#endif // BLDC_DRIVER_DEVICE_H


