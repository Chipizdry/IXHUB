


#ifndef VACUUM_PRESSURE_SENSOR_H
#define VACUUM_PRESSURE_SENSOR_H

#include "device.h"
#include <QObject>
#include <QJsonObject>
#include <QList>

class VacuumPressureSensor : public Device
{
    Q_OBJECT

public:
    // Modbus function codes
    static constexpr quint8 FC_READ_HOLDING = 0x03;
    static constexpr quint8 FC_READ_INPUT = 0x04;
    static constexpr quint8 FC_WRITE_SINGLE = 0x06;
    static constexpr quint8 FC_WRITE_MULTIPLE = 0x10;

    // Register addresses (from documentation)
    static constexpr quint16 REG_SLAVE_ADDRESS = 0x0000;      // Адрес ведомого (1-255)
    static constexpr quint16 REG_BAUD_RATE = 0x0001;          // Скорость бода
    static constexpr quint16 REG_UNIT = 0x0002;               // Единицы измерения
    static constexpr quint16 REG_DECIMAL_POINTS = 0x0003;     // Количество десятичных знаков (0-4)
    static constexpr quint16 REG_PV_VALUE = 0x0004;           // Текущее значение давления (PV)
    static constexpr quint16 REG_RANGE_ZERO = 0x0005;         // Ноль диапазона
    static constexpr quint16 REG_RANGE_FULL = 0x0006;         // Полная шкала диапазона
    static constexpr quint16 REG_ZERO_OFFSET = 0x000C;        // Смещение нуля
    static constexpr quint16 REG_FLOAT_VALUE = 0x0016;        // Значение в формате float (4 байта)
    static constexpr quint16 REG_PARITY = 0x0025;             // Биты четности (0-2)
    static constexpr quint16 REG_SAVE = 0x000F;               // Сохранение в пользовательскую область
    static constexpr quint16 REG_RESTORE_FACTORY = 0x0010;    // Восстановление заводских настроек

    // Baud rate codes
    enum BaudRateCode {
        BAUD_1200 = 0,
        BAUD_2400 = 1,
        BAUD_4800 = 2,
        BAUD_9600 = 3,
        BAUD_19200 = 4,
        BAUD_38400 = 5,
        BAUD_57600 = 6,
        BAUD_115200 = 7
    };

    // Unit codes
    enum UnitCode {
        UNIT_MPA = 0,      // MPa
        UNIT_KPA = 1,      // kPa (рекомендуется для вакуума)
        UNIT_PA = 2,       // Pa
        UNIT_BAR = 3,      // bar
        UNIT_MBAR = 4,     // mbar
        UNIT_KGCM2 = 5,    // kg/cm²
        UNIT_PSI = 6,      // PSI
        UNIT_MH2O = 7,     // mH₂O
        UNIT_MMH2O = 8,    // mmH₂O
        UNIT_INH2O = 9,    // inH₂O
        UNIT_H2O = 10,     // H₂O
        UNIT_MHG = 11,     // mHg
        UNIT_MMHG = 12,    // mmHg
        UNIT_INHG = 13,    // inHg
        UNIT_ATM = 14,     // atm
        UNIT_TORR = 15,    // Torr
        UNIT_M = 16,       // m
        UNIT_CM = 17,      // cm
        UNIT_MM = 18,      // mm
        UNIT_KG = 19,      // kg
        UNIT_C = 20,       // °C
        UNIT_PH = 21,      // pH
        UNIT_F = 22        // °F
    };

    // Parity codes
    enum ParityCode {
        PARITY_NONE = 0,
        PARITY_ODD = 1,
        PARITY_EVEN = 2
    };

    explicit VacuumPressureSensor(int slaveId, QObject *parent = nullptr);
    ~VacuumPressureSensor();

    // Device interface
    QList<quint16> getRegisterAddresses() override;
    QJsonObject processData(const QByteArray& data) override;
    QJsonObject toJson() const override;

    // Генерация команд для чтения
    QByteArray generateReadRegisterCommand(quint16 regAddr, quint16 numRegs = 1) const;
    QByteArray generateReadPvValueCommand() const;           // Чтение текущего давления (REG_PV_VALUE)
    QByteArray generateReadFloatValueCommand() const;        // Чтение значения в формате float
    QByteArray generateReadConfigCommand() const;            // Чтение всей конфигурации

    // Генерация команд для записи
    QByteArray generateWriteRegisterCommand(quint16 regAddr, quint16 value) const;
    QByteArray generateWriteFloatValueCommand(float value) const;

    // Управление датчиком
    void setSlaveAddress(quint8 newAddress);                 // Изменить Modbus адрес
    void setBaudRate(int baudRate);                          // Изменить скорость бода
    void setParity(ParityCode parity);                       // Изменить биты четности
    void setZeroOffset(qint16 offset);                       // Установить смещение нуля
    void saveToUserArea();                                   // Сохранить настройки
    void restoreFactorySettings();                           // Восстановить заводские настройки

    // Команды чтения
    void readPvValue();                                      // Прочитать текущее давление
    void readFloatValue();                                   // Прочитать float значение
    void readConfiguration();                                // Прочитать конфигурацию

    // Геттеры
    qint16 getRawPvValue() const { return m_rawPvValue; }
    float getPressure() const { return m_pressure; }
    float getFloatPressure() const { return m_floatPressure; }
    int getSlaveAddressConfig() const { return m_slaveAddressConfig; }
    int getBaudRate() const { return m_baudRate; }
    int getDecimalPoints() const { return m_decimalPoints; }
    int getUnitCode() const { return m_unitCode; }
    QString getUnitString() const;
    ParityCode getParity() const { return m_parity; }
    bool isNegative() const { return m_pressure < 0; }       // Отрицательное давление (вакуум)

signals:
    void commandGenerated(const QByteArray& command);
    void pressureUpdated(float pressure, const QString& unit);
    void vacuumLevelChanged(float vacuumLevel);              // Уровень вакуума (абсолютное значение)
    void configurationChanged();

private:
    void parsePvValue(const QByteArray& data);               // Парсинг целочисленного значения
    void parseFloatValue(const QByteArray& data);            // Парсинг float значения
    void parseConfiguration(const QByteArray& data);         // Парсинг конфигурации
    void parseWriteResponse(const QByteArray& data);
    float calculatePressure(qint16 rawValue, int decimalPoints) const;

    // Данные устройства
    qint16 m_rawPvValue;          // Сырое значение PV (целое)
    float m_pressure;             // Рассчитанное давление (с учетом десятичных знаков)
    float m_floatPressure;        // Давление из float регистра

    // Конфигурация
    int m_slaveAddressConfig;     // Настроенный Modbus адрес
    int m_baudRate;               // Скорость бода (bps)
    int m_decimalPoints;          // Количество десятичных знаков (0-4)
    int m_unitCode;               // Код единиц измерения
    ParityCode m_parity;          // Биты четности

    static constexpr int DEFAULT_TIMEOUT_MS = 500;
};

#endif // VACUUM_PRESSURE_SENSOR_H


