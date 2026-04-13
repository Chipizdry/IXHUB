


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
    qDebug() << "🔵 ModbusMaster: Sending to slave" << slaveAddr << ":" << req.toHex();

    rxBuffer.clear();
    qint64 written = serial.write(req);

    if (written != req.size()) {
        qDebug() << "❌ ModbusMaster: Failed to write all data";
        emit error("Failed to write all data");
        return;
    }

    serial.flush();
    timeoutTimer.start(1000);
    qDebug() << "🔵 ModbusMaster: Timeout timer started (1000ms)";
}

void ModbusMaster::sendRawData(quint8 /*slaveAddr*/, const QByteArray& data)
{
    if (!serial.isOpen()) {
        qDebug() << "Serial port not open";
        emit error("Serial port not open");
        return;
    }

    qDebug() << "📤 Sending raw data:" << data.toHex();

    rxBuffer.clear();
    qint64 bytesWritten = serial.write(data);
    if (bytesWritten != data.size()) {
        qDebug() << "Failed to send all data, written:" << bytesWritten << "expected:" << data.size();
        emit error("Failed to send all data");
        return;
    }

    serial.flush();
    timeoutTimer.start(1000);
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
    QByteArray newData = serial.readAll();
    rxBuffer += newData;

    qDebug() << "🔵 Received raw:" << newData.toHex() << "(total:" << rxBuffer.size() << "bytes)";

    if (rxBuffer.size() < 5) {
        qDebug() << "🔵 Waiting for more data...";
        return;
    }

    timeoutTimer.stop();

    QByteArray data = rxBuffer.left(rxBuffer.size() - 2);
    quint16 crcRecv = (quint8)rxBuffer.at(rxBuffer.size() - 2) |
                      ((quint8)rxBuffer.at(rxBuffer.size() - 1) << 8);
    quint16 crcCalc = calcCRC(data);

    if (crcRecv != crcCalc) {
        qDebug() << "❌ CRC error! Received:" << QString::number(crcRecv, 16)
                 << "Calculated:" << QString::number(crcCalc, 16);
        emit error("CRC error");
        return;
    }

    qDebug() << "✅ Valid Modbus response received";
    emit dataReady(rxBuffer);
}

void ModbusMaster::onTimeout()
{
    qDebug() << "⏰ Modbus timeout - no response";
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


