#include "ModbusMaster.h"
#include <QDebug>

ModbusMaster::ModbusMaster(QObject *parent) : QObject(parent)
{
    connect(&serial, &QSerialPort::readyRead, this, &ModbusMaster::onReadyRead);

    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, this, &ModbusMaster::onTimeout);
}

ModbusMaster::~ModbusMaster()
{
    if (serial.isOpen()) serial.close();
}

bool ModbusMaster::init(const QString &portName, int baudrate)
{
    serial.setPortName(portName);
    serial.setBaudRate(baudrate);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::ReadWrite)) {
        emit error("UART open failed");
        return false;
    }
    return true;
}

void ModbusMaster::readHoldingRegisters(quint8 slaveAddr, quint16 startAddr, quint16 count)
{
    if (!serial.isOpen()) {
        emit error("UART not open");
        return;
    }

    QByteArray req = buildRequest(slaveAddr, 0x03, startAddr, count);
    rxBuffer.clear();
    serial.write(req);
    timeoutTimer.start(1000); // таймаут 1 сек
}

QByteArray ModbusMaster::buildRequest(quint8 slave, quint8 func, quint16 addr, quint16 count)
{
    QByteArray req;
    req.append(slave);
    req.append(func);
    req.append(addr >> 8);
    req.append(addr & 0xFF);
    req.append(count >> 8);
    req.append(count & 0xFF);

    quint16 crc = calcCRC(req);
    req.append(crc & 0xFF);
    req.append(crc >> 8);

    return req;
}

void ModbusMaster::onReadyRead()
{
    rxBuffer += serial.readAll();

    // минимальная длина ответа: slave + func + byte count + crc
    if (rxBuffer.size() < 5) return;

    timeoutTimer.stop();

    QByteArray data = rxBuffer.left(rxBuffer.size() - 2);

    quint16 crcRecv =
        (quint8)rxBuffer.at(rxBuffer.size() - 2) |
        ((quint8)rxBuffer.at(rxBuffer.size() - 1) << 8);

    quint16 crcCalc = calcCRC(data);

    if (crcRecv != crcCalc) {
        emit error("CRC error");
        return;
    }

    emit dataReady(rxBuffer);
}

void ModbusMaster::onTimeout()
{
    emit error("Timeout waiting for response");
}

quint16 ModbusMaster::calcCRC(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (auto b : data) {
        crc ^= static_cast<quint8>(b);
        for (int i = 0; i < 8; i++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}


