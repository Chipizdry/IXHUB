


#ifndef LOGGER_H
#define LOGGER_H

#include <QString>

class Logger {
public:
    static void log(const QString& message, const QString& level = "INFO");
    static void debug(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);
    static void data(const QString& message);
};

#endif // LOGGER_H


