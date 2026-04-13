


#pragma once

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray>

class ModbusMaster : public QObject
{
    Q_OBJECT

public:
    explicit ModbusMaster(QObject *parent = nullptr);
    ~ModbusMaster();

    // Настройка порта
    bool init(const QString &portName, int baudrate = 9600);

    // Отправка запроса: чтение holding registers
    void readHoldingRegisters(quint8 slaveAddr, quint16 startAddr, quint16 count);

    // Отправка сырых данных (для реле и других устройств)
    void sendRawData(quint8 slaveAddr, const QByteArray& data);  // Изменено с bool на void

signals:
    void dataReady(QByteArray data);
    void error(QString err);

private slots:
    void onReadyRead();
    void onTimeout();

private:
    QSerialPort serial;
    QTimer timeoutTimer;
    QByteArray rxBuffer;

    QByteArray buildRequest(quint8 slave, quint8 func, quint16 addr, quint16 count);
    quint16 calcCRC(const QByteArray &data);
};


