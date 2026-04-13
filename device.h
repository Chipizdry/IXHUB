

#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

class Device : public QObject
{
    Q_OBJECT
public:
    enum DeviceType {
        TYPE_NTA8A01,      // Датчик температуры
        TYPE_RELAY,
        TYPE_UNKNOWN
    };
    Q_ENUM(DeviceType)


    static quint16 calculateCRC(const QByteArray& data);
    static QByteArray appendCRC(const QByteArray& command);


    explicit Device(int slaveId, DeviceType type, const QString& name, QObject *parent = nullptr);
    virtual ~Device();

    // Виртуальные методы для переопределения
    virtual QList<quint16> getRegisterAddresses() = 0;     // Какие регистры читать
    virtual QJsonObject processData(const QByteArray& data) = 0; // Обработка полученных данных

    // Геттеры
    int getSlaveId() const { return m_slaveId; }
    DeviceType getType() const { return m_type; }
    QString getName() const { return m_name; }

    // Сеттеры
    void setName(const QString& name) { m_name = name; }

    // Сериализация
    virtual QJsonObject toJson() const;

protected:
    int m_slaveId;
    DeviceType m_type;
    QString m_name;
};

#endif // DEVICE_H

