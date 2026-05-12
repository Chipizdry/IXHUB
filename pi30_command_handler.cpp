


// pi30_command_handler.cpp
#include "pi30_command_handler.h"
#include <QDebug>
#include <QRegularExpression>

Pi30CommandHandler::Pi30CommandHandler(QObject *parent)
    : QObject(parent)
{
}

void Pi30CommandHandler::processHexData(const QString &hexData)
{
    Pi30Command cmd = parse(hexData);
    if (cmd.valid) {
        dispatch(cmd);
    } else {
        qWarning() << "Invalid PI30 command, raw hex:" << hexData;
    }
}

Pi30Command Pi30CommandHandler::parse(const QString &hexData)
{
    Pi30Command cmd;
    cmd.rawHex = hexData;
    cmd.valid = false;

    // Удаляем все пробелы и преобразуем hex → QByteArray
    QString cleaned = hexData.simplified();
    cleaned.remove(' ');
    cmd.rawData = QByteArray::fromHex(cleaned.toUtf8());
    if (cmd.rawData.isEmpty()) {
        return cmd;
    }

    // Ожидаемая структура: [команда ASCII] [2 байта CRC] [0x0D]
    // Минимальная длина: команда "FWSTATUS" (8) + CRC2 + CR = 11 байт
    if (cmd.rawData.size() < 3) {
        return cmd;
    }

    // Последний байт должен быть 0x0D (CR)
    if (cmd.rawData[cmd.rawData.size() - 1] != '\r') {
        qWarning() << "Missing CR at end of PI30 command";
        return cmd;
    }

    // Извлекаем CRC (два байта перед CR)
    int crcPos = cmd.rawData.size() - 3;
    if (crcPos < 0) return cmd;
    cmd.crc = static_cast<quint16>(
        (static_cast<quint8>(cmd.rawData[crcPos]) << 8) |
        static_cast<quint8>(cmd.rawData[crcPos + 1])
    );

    // ASCII команда — от начала до позиции CRC
    QByteArray asciiPart = cmd.rawData.left(crcPos);
    cmd.commandString = QString::fromLatin1(asciiPart);

    // Базовая проверка: ASCII строка состоит из букв и цифр
    static QRegularExpression re("^[A-Za-z0-9]+$");
    if (!re.match(cmd.commandString).hasMatch()) {
        qWarning() << "Command contains non-ASCII characters:" << cmd.commandString;
        return cmd;
    }

    // TODO: опционально проверить CRC (алгоритм XOR или другой)
    // Пока считаем валидным, если команда распознана
    cmd.valid = true;
    return cmd;
}

void Pi30CommandHandler::dispatch(const Pi30Command &cmd)
{
    if (cmd.commandString == "FWSTATUS") {
        handleFwStatus(cmd);
        emit fwStatusReceived(cmd.rawData);
    }
    else if (cmd.commandString == "FWDCSTATUS") {
        handleFwDcStatus(cmd);
        emit fwDcStatusReceived(cmd.rawData);
    }
    else if (cmd.commandString == "FWMSTATUS") {
        handleFwMotorStatus(cmd);
        emit fwMotorStatusReceived(cmd.rawData);
    }
    else if (cmd.commandString == "FWGSTATUS") {
        handleFwGeneratorStatus(cmd);
        emit fwGeneratorStatusReceived(cmd.rawData);
    }
    else if (cmd.commandString == "FWSET") {
        handleFwSet(cmd);
        emit fwSetReceived(cmd.rawData);
    }
    else {
        handleUnknown(cmd.commandString);
        emit unknownCommandReceived(cmd.commandString);
    }
}

// Реализация обработчиков (пока просто логируем)
void Pi30CommandHandler::handleFwStatus(const Pi30Command &cmd)
{
    qDebug() << "📊 FWSTATUS received, CRC=" << hex << cmd.crc;
    // Здесь будет парсинг статуса системы
}

void Pi30CommandHandler::handleFwDcStatus(const Pi30Command &cmd)
{
    qDebug() << "⚡ FWDCSTATUS received, CRC=" << hex << cmd.crc;
    // Парсинг состояния DC
}

void Pi30CommandHandler::handleFwMotorStatus(const Pi30Command &cmd)
{
    qDebug() << "🔄 FWMSTATUS received, CRC=" << hex << cmd.crc;
    // Парсинг состояния мотора маховика
}

void Pi30CommandHandler::handleFwGeneratorStatus(const Pi30Command &cmd)
{
    qDebug() << "🔧 FWGSTATUS received, CRC=" << hex << cmd.crc;
    // Парсинг состояния генератора
}

void Pi30CommandHandler::handleFwSet(const Pi30Command &cmd)
{
    qDebug() << "⚙️ FWSET received (set parameters), CRC=" << hex << cmd.crc;
    // Установка параметров (будет позже)
}

void Pi30CommandHandler::handleUnknown(const QString &cmdStr)
{
    qDebug() << "❓ Unknown PI30 command:" << cmdStr;
}



