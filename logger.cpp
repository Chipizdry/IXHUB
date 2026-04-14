

#include "logger.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

void Logger::log(const QString& message, const QString& level)
{
    QFile file("/var/log/modbus_devices.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
            << " [" << level << "] " << message << "\n";
        file.close();
    }
}

void Logger::debug(const QString& message)
{
    log(message, "DEBUG");
    qDebug() << "[DEBUG]" << message;
}

void Logger::warning(const QString& message)
{
    log(message, "WARNING");
    qWarning() << "[WARNING]" << message;
}

void Logger::error(const QString& message)
{
    log(message, "ERROR");
    qCritical() << "[ERROR]" << message;
}

void Logger::data(const QString& message)
{
    log(message, "DATA");
    qDebug() << "[DATA]" << message;
}

