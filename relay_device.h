


#ifndef RELAY_DEVICE_H
#define RELAY_DEVICE_H

#include "device.h"
#include <QJsonObject>
#include <QJsonArray>

class RelayDevice : public Device
{
    Q_OBJECT

public:
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

    // Генерация команд (публичные методы для DevicePoller)
    QByteArray generateReadRelayStatusCommand() const;
    QByteArray generateReadOptocouplerCommand() const;
    QByteArray generateSetRelayCommand(int relayNumber, bool on) const;

signals:
    void commandGenerated(const QByteArray& command);

private:
    // Состояния
    bool m_relayStates[8];
    bool m_optocouplerStates[8];
    quint8 m_deviceAddress;
    int m_baudRate;

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

    // Приватные методы генерации команд
    QByteArray generateReadAddressCommand() const;
    QByteArray generateReadBaudRateCommand() const;

    QByteArray generateSetAllRelaysCommand(bool on) const;
    QByteArray generateSetAddressCommand(quint8 newAddress) const;
    QByteArray generateSetBaudRateCommand(int baudRate) const;
};

#endif // RELAY_DEVICE_H


