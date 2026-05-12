


#ifndef PI30_COMMAND_HANDLER_H
#define PI30_COMMAND_HANDLER_H

#include <QObject>
#include <QString>
#include <QByteArray>

struct Pi30Command {
    QString rawHex;          // исходная hex-строка
    QByteArray rawData;      // бинарные данные
    QString commandString;   // ASCII команда без CRC и CR (например "FWSTATUS")
    quint16 crc;             // CRC (2 байта, если есть)
    bool valid;              // флаг корректности (проверка CRC опционально)
};

class Pi30CommandHandler : public QObject
{
    Q_OBJECT
public:
    explicit Pi30CommandHandler(QObject *parent = nullptr);

    // Основной метод: на вход hex-строка вида "46 57 53 ..."
    void processHexData(const QString &hexData);

signals:
    // Сигналы для каждого типа команды (можно объединить в один с типом)
    void fwStatusReceived(const QByteArray &fullData);
    void fwDcStatusReceived(const QByteArray &fullData);
    void fwMotorStatusReceived(const QByteArray &fullData);
    void fwGeneratorStatusReceived(const QByteArray &fullData);
    void fwSetReceived(const QByteArray &fullData);
    void unknownCommandReceived(const QString &commandString);

private:
    Pi30Command parse(const QString &hexData);
    void dispatch(const Pi30Command &cmd);

    // Обработчики (можно сделать виртуальными для переопределения)
    void handleFwStatus(const Pi30Command &cmd);
    void handleFwDcStatus(const Pi30Command &cmd);
    void handleFwMotorStatus(const Pi30Command &cmd);
    void handleFwGeneratorStatus(const Pi30Command &cmd);
    void handleFwSet(const Pi30Command &cmd);
    void handleUnknown(const QString &cmdStr);
};

#endif // PI30_COMMAND_HANDLER_H


