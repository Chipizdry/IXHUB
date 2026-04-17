


#ifndef RELAY_DEVICE_H
#define RELAY_DEVICE_H

#include "device.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

class RelayDevice : public Device
{
    Q_OBJECT
    Q_PROPERTY(QString role READ getRole WRITE setRole NOTIFY roleChanged)

public:
    // Роли релейного модуля
    enum RelayRole {
        ROLE_UNDEFINED = 0,      // Не определена
        ROLE_POWER_CONTROL = 1,   // Управление питанием (балласт, главное питание)
        ROLE_COOLING_SYSTEM = 2,  // Система охлаждения (насосы, вентиляторы)
        ROLE_AUXILIARY = 3,       // Вспомогательные устройства
        ROLE_SAFETY = 4,          // Система безопасности
        ROLE_LIGHTING = 5,        // Освещение
        ROLE_ALARM = 6            // Сигнализация
    };
    Q_ENUM(RelayRole)

    explicit RelayDevice(int slaveId = 0xFF, QObject *parent = nullptr);
    ~RelayDevice();

    // Реализация виртуальных методов Device
    QList<quint16> getRegisterAddresses() override;
    QJsonObject processData(const QByteArray& data) override;
    QJsonObject toJson() const override;

    // Команды управления (публичные методы)
    void setRelay(int relayNumber, bool on);
    void setAllRelays(bool on);
    void readRelayStatus();
    void readOptocouplerStatus();
    void setDeviceAddress(quint8 newAddress);
    void readDeviceAddress();
    void setBaudRate(int baudRate);
    void readBaudRate();

    // НОВЫЕ МЕТОДЫ: Управление по логическому имени
    void setRelayByName(const QString& relayName, bool on);
    bool getRelayState(int relayNumber) const;
    bool getRelayStateByName(const QString& relayName) const;

    // НОВЫЕ МЕТОДЫ: Управление ролью устройства
    QString getRole() const;
    void setRole(RelayRole role);
    void setRole(const QString& role);

    // НОВЫЕ МЕТОДЫ: Конфигурация имен реле
    void setRelayName(int relayNumber, const QString& name);
    QString getRelayName(int relayNumber) const;
    void loadRelayMapping(const QMap<int, QString>& mapping);

    // Генерация команд (публичные методы для DevicePoller)
    QByteArray generateReadRelayStatusCommand() const;
    QByteArray generateReadOptocouplerCommand() const;
    QByteArray generateSetRelayCommand(int relayNumber, bool on) const;

signals:
    void commandGenerated(const QByteArray& command);
    void roleChanged();                                    // НОВЫЙ СИГНАЛ
    void relayStateChanged(int relayNumber, bool state);   // НОВЫЙ СИГНАЛ
    void relayStateChangedByName(const QString& relayName, bool state); // НОВЫЙ СИГНАЛ

private:
    // Состояния
    bool m_relayStates[8];
    bool m_optocouplerStates[8];
    quint8 m_deviceAddress;
    int m_baudRate;

    // НОВЫЕ ПОЛЯ
    RelayRole m_role;                    // Роль устройства
    QMap<int, QString> m_relayNames;     // Номер реле -> логическое имя

    // Константы
    static const quint16 RELAY_ADDRESSES[8];

    // Функции Modbus
    static const quint8 FC_READ_COILS = 0x01;
    static const quint8 FC_READ_DISCRETE = 0x02;
    static const quint8 FC_READ_HOLDING = 0x03;
    static const quint8 FC_WRITE_SINGLE = 0x05;
    static const quint8 FC_WRITE_MULTIPLE = 0x0F;
    static const quint8 FC_WRITE_MULTIPLE_REG = 0x10;

    // Вспомогательные методы
    quint16 calculateCRC(const QByteArray& data) const;
    void parseRelayStatus(const QByteArray& data);
    void parseOptocouplerStatus(const QByteArray& data);
    void parseAddress(const QByteArray& data);
    void parseBaudRate(const QByteArray& data);
    void parseWriteResponse(const QByteArray& data);

    // НОВЫЙ ВСПОМОГАТЕЛЬНЫЙ МЕТОД
    int getRelayNumberByName(const QString& name) const;

    // Приватные методы генерации команд
    QByteArray generateReadAddressCommand() const;
    QByteArray generateReadBaudRateCommand() const;

    QByteArray generateSetAllRelaysCommand(bool on) const;
    QByteArray generateSetAddressCommand(quint8 newAddress) const;
    QByteArray generateSetBaudRateCommand(int baudRate) const;
};

#endif // RELAY_DEVICE_H


