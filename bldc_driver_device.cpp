


#include "bldc_driver_device.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <QTimer>
#include <QString>
#include <QChar>


// ==================== ОПРЕДЕЛЕНИЯ СТАТИЧЕСКИХ КОНСТАНТ ====================
// Это обязательно для static constexpr членов класса в C++11/14
constexpr quint8  BldcDriverDevice::FC_READ_HOLDING;
constexpr quint8  BldcDriverDevice::FC_WRITE_SINGLE;
constexpr quint8  BldcDriverDevice::FC_WRITE_MULTIPLE;

constexpr quint16 BldcDriverDevice::REG_PWM;
constexpr quint16 BldcDriverDevice::REG_PWM_HZ;
constexpr quint16 BldcDriverDevice::REG_RPM;
constexpr quint16 BldcDriverDevice::REG_TIMER_ARR;
constexpr quint16 BldcDriverDevice::REG_PWM_VALUE;
constexpr quint16 BldcDriverDevice::REG_ANGLE_PHAZE;
constexpr quint16 BldcDriverDevice::REG_STATUS;

constexpr quint16 BldcDriverDevice::PWM_MIN;
constexpr quint16 BldcDriverDevice::PWM_MAX;
constexpr quint16 BldcDriverDevice::TIMER_ARR_MIN;
constexpr quint16 BldcDriverDevice::TIMER_ARR_MAX;

constexpr int BldcDriverDevice::WRITE_DEBOUNCE_MS;
constexpr int BldcDriverDevice::NUM_REGISTERS;


BldcDriverDevice::BldcDriverDevice(int slaveId, QObject *parent)
    : Device(slaveId, Device::TYPE_BLDC_DRIVER, QString("BLDC_Driver_%1").arg(slaveId), parent)
    , m_pwmKhz(0)
    , m_pwmHz(0)
    , m_rpm(0)
    , m_timerArr(0)
    , m_pwmValue(0)
    , m_targetPwm(0)
    , m_targetTimerArr(0)
    , m_targetStatus(0)
    , m_pwmDirty(false)
    , m_timerArrDirty(false)
    , m_statusDirty(false)
    , m_coil1(false)
    , m_coil2(false)
    , m_coil3(false)
    , m_coil4(false)
    , m_autoMode(false)
    , m_powerOn(false)
{
    qDebug() << "BldcDriverDevice created: slaveId=" << slaveId;

    m_writeTimer = new QTimer(this);
    m_writeTimer->setSingleShot(true);
    connect(m_writeTimer, &QTimer::timeout, this, &BldcDriverDevice::onWriteTimerTimeout);
}

BldcDriverDevice::~BldcDriverDevice()
{
}

QList<quint16> BldcDriverDevice::getRegisterAddresses()
{
    return {REG_PWM, REG_PWM_HZ, REG_RPM, REG_TIMER_ARR, REG_PWM_VALUE, REG_ANGLE_PHAZE, REG_STATUS};
}

// ==================== ГЕНЕРАЦИЯ КОМАНД ====================

QByteArray BldcDriverDevice::generateReadAllRegistersCommand() const
{
    QByteArray command;
    command.append(static_cast<char>(m_slaveId));
    command.append(static_cast<char>(FC_READ_HOLDING));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(NUM_REGISTERS));

    return Device::appendCRC(command);
}

QByteArray BldcDriverDevice::generateReadRegisterCommand(quint16 regAddr) const
{
    QByteArray command;
    command.append(static_cast<char>(m_slaveId));
    command.append(static_cast<char>(FC_READ_HOLDING));
    command.append(static_cast<char>(regAddr >> 8));
    command.append(static_cast<char>(regAddr & 0xFF));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x01));

    return Device::appendCRC(command);
}

QByteArray BldcDriverDevice::generateWriteRegisterCommand(quint16 regAddr, quint16 value) const
{
    QByteArray command;
    command.append(static_cast<char>(m_slaveId));
    command.append(static_cast<char>(FC_WRITE_SINGLE));
    command.append(static_cast<char>(regAddr >> 8));
    command.append(static_cast<char>(regAddr & 0xFF));
    command.append(static_cast<char>(value >> 8));
    command.append(static_cast<char>(value & 0xFF));

    return Device::appendCRC(command);
}

QByteArray BldcDriverDevice::generateWriteMultipleRegistersCommand(const QList<quint16>& addresses, const QList<quint16>& values) const
{
    if (addresses.size() != values.size() || addresses.isEmpty()) {
        qDebug() << "Invalid parameters for write multiple registers";
        return QByteArray();
    }

    quint16 startAddr = addresses.first();
    quint16 numRegs = addresses.size();

    QByteArray command;
    command.append(static_cast<char>(m_slaveId));
    command.append(static_cast<char>(FC_WRITE_MULTIPLE));
    command.append(static_cast<char>(startAddr >> 8));
    command.append(static_cast<char>(startAddr & 0xFF));
    command.append(static_cast<char>(numRegs >> 8));
    command.append(static_cast<char>(numRegs & 0xFF));
    command.append(static_cast<char>(numRegs * 2));

    for (quint16 value : values) {
        command.append(static_cast<char>(value >> 8));
        command.append(static_cast<char>(value & 0xFF));
    }

    return Device::appendCRC(command);
}

// ==================== ПАРСИНГ ОТВЕТОВ ====================

void BldcDriverDevice::parseHoldingRegisters(const QByteArray& data)
{
    if (data.size() < 4) {
        qDebug() << "BldcDriverDevice: Response too small, size:" << data.size();
        return;
    }

    int byteCount = static_cast<quint8>(data[2]);
    int expectedSize = 3 + byteCount;

    if (data.size() < expectedSize) {
        qDebug() << "BldcDriverDevice: Incomplete response, expected:" << expectedSize
                 << "got:" << data.size();
        return;
    }

    const QByteArray regData = data.mid(3, byteCount);

    qDebug() << "BldcDriverDevice: Register data size:" << regData.size()
             << "bytes, hex:" << regData.toHex();

    if (regData.size() >= 10) {
        m_pwmKhz = (static_cast<quint8>(regData[0]) << 8) | static_cast<quint8>(regData[1]);
        m_pwmHz = (static_cast<quint8>(regData[2]) << 8) | static_cast<quint8>(regData[3]);
        m_rpm = (static_cast<quint8>(regData[4]) << 8) | static_cast<quint8>(regData[5]);
        m_timerArr = (static_cast<quint8>(regData[6]) << 8) | static_cast<quint8>(regData[7]);
        m_pwmValue = (static_cast<quint8>(regData[8]) << 8) | static_cast<quint8>(regData[9]);

        qDebug() << QString("BLDC Slave %1: PWM_KHZ=%2, PWM_HZ=%3, RPM=%4, TIMER_ARR=%5, PWM_VAL=%6")
                    .arg(m_slaveId)
                    .arg(m_pwmKhz)
                    .arg(m_pwmHz)
                    .arg(m_rpm)
                    .arg(m_timerArr)
                    .arg(m_pwmValue);
        emit dataUpdated();

    } else if (regData.size() >= 2) {
        m_rpm = (static_cast<quint8>(regData[0]) << 8) | static_cast<quint8>(regData[1]);
        qDebug() << QString("BLDC Slave %1: RPM=%2").arg(m_slaveId).arg(m_rpm);
        emit dataUpdated();
    } else {
        qDebug() << "BldcDriverDevice: Invalid register data size:" << regData.size();
    }
}

void BldcDriverDevice::parseCoilsAndStatus(quint8 statusByte)
{
    m_coil1 = (statusByte >> 1) & 0x01;
    m_coil2 = (statusByte >> 2) & 0x01;
    m_coil3 = (statusByte >> 3) & 0x01;
    m_coil4 = (statusByte >> 4) & 0x01;
    m_autoMode = (statusByte >> 5) & 0x01;
    m_powerOn = (statusByte >> 6) & 0x01;

    qDebug() << QString("BLDC Slave %1: Coils: %2%3%4%5, Auto=%6, Power=%7")
                .arg(m_slaveId)
                .arg(m_coil1 ? "1" : "0")
                .arg(m_coil2 ? "1" : "0")
                .arg(m_coil3 ? "1" : "0")
                .arg(m_coil4 ? "1" : "0")
                .arg(m_autoMode ? "ON" : "OFF")
                .arg(m_powerOn ? "ON" : "OFF");
}

void BldcDriverDevice::parseWriteResponse(const QByteArray& data)
{
    if (data.size() < 8) {
        qDebug() << "BldcDriverDevice: Write response too small";
        return;
    }

    quint8 funcCode = static_cast<quint8>(data[1]);

    if (funcCode == FC_WRITE_SINGLE) {
        quint16 regAddr = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
        quint16 value = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);

        qDebug() << "BLDC: Write confirmed - register:" << QString("0x%1").arg(regAddr, 4, 16, QChar('0'))
                 << "value:" << value;

        switch(regAddr) {
            case REG_PWM: m_targetPwm = value; break;
            case REG_PWM_HZ: m_pwmHz = value; break;
            case REG_RPM: m_rpm = value; break;
            case REG_TIMER_ARR: m_targetTimerArr = value; break;
            case REG_PWM_VALUE: m_pwmValue = value; break;
        }
    } else if (funcCode == FC_WRITE_MULTIPLE) {
        qDebug() << "BLDC: Multiple write confirmed";
    }
}

// ==================== ОСНОВНОЙ МЕТОД ОБРАБОТКИ ====================

QJsonObject BldcDriverDevice::processData(const QByteArray& data)
{
    QJsonObject result;
    result["device_type"] = "BLDCDriver";
    result["slave_id"] = m_slaveId;
    result["name"] = m_name;

    qDebug() << "BldcDriverDevice: Raw data size:" << data.size() << "bytes, hex:" << data.toHex();

    if (data.isEmpty()) {
        qDebug() << "BldcDriverDevice: Empty data received";
        return result;
    }

    quint8 funcCode = static_cast<quint8>(data[1]);

    switch(funcCode) {
        case FC_READ_HOLDING:
            parseHoldingRegisters(data);
            break;

        case FC_WRITE_SINGLE:
        case FC_WRITE_MULTIPLE:
            parseWriteResponse(data);
            break;

        default:
            qDebug() << "BldcDriverDevice: Unknown function code:" << funcCode;
            break;
    }

    result["pwm_khz"] = m_pwmKhz;
    result["pwm_hz"] = m_pwmHz;
    result["rpm"] = m_rpm;
    result["timer_arr"] = m_timerArr;
    result["pwm_value"] = m_pwmValue;

    QJsonArray coils;
    coils.append(m_coil1);
    coils.append(m_coil2);
    coils.append(m_coil3);
    coils.append(m_coil4);
    result["coils"] = coils;
    result["auto_mode"] = m_autoMode;
    result["power_on"] = m_powerOn;

    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    return result;
}

// ==================== КЭШИРОВАНИЕ И ГРУППОВАЯ ЗАПИСЬ ====================

void BldcDriverDevice::markDirty(quint16 regAddr, quint16 value)
{
    switch(regAddr) {
        case REG_PWM:
            m_targetPwm = value;
            m_pwmDirty = true;
            break;
        case REG_TIMER_ARR:
            m_targetTimerArr = value;
            m_timerArrDirty = true;
            break;
        case REG_STATUS:
            m_targetStatus = static_cast<quint8>(value);
            m_statusDirty = true;
            break;
        default:
            break;
    }
    scheduleWrite();
}

void BldcDriverDevice::scheduleWrite()
{
    if (!m_writeTimer->isActive()) {
        m_writeTimer->start(WRITE_DEBOUNCE_MS);
    }
}

void BldcDriverDevice::onWriteTimerTimeout()
{
    QList<quint16> addresses;
    QList<quint16> values;

    if (m_pwmDirty) {
        addresses.append(REG_PWM);        // 0x0000 - для PWM
        values.append(m_targetPwm);
        qDebug() << "BLDC: Queue PWM write:" << m_targetPwm;
    }

    if (m_timerArrDirty) {
        // ИСПРАВЛЕНО: используем REG_PWM_VALUE (0x0004) для записи ARR
        addresses.append(REG_PWM_VALUE);  // 0x0004, а не 0x0003
        values.append(m_targetTimerArr);
        qDebug() << "BLDC: Queue TIMER_ARR write:" << m_targetTimerArr;
    }

    if (m_statusDirty) {
        addresses.append(REG_STATUS);     // 0x0007
        values.append(m_targetStatus);
        qDebug() << "BLDC: Queue STATUS write:" << QString("0x%1").arg(m_targetStatus, 2, 16, QChar('0'));
    }

    if (!addresses.isEmpty()) {
        QByteArray command = generateWriteMultipleRegistersCommand(addresses, values);
        if (!command.isEmpty()) {
            emit commandGenerated(command);
            qDebug() << "📤 BLDC: Sent MULTIPLE write command (0x10) for"
                     << addresses.size() << "registers";
            // Выводим HEX команды для отладки
            qDebug() << "   Command hex:" << command.toHex();
        }
    }

    m_pwmDirty = false;
    m_timerArrDirty = false;
    m_statusDirty = false;
}





void BldcDriverDevice::flushWriteCache()
{
    if (m_writeTimer->isActive()) {
        m_writeTimer->stop();
    }
    onWriteTimerTimeout();
}

// ==================== ПУБЛИЧНЫЕ МЕТОДЫ УПРАВЛЕНИЯ ====================

quint8 BldcDriverDevice::getCoilsByte() const
{
    quint8 result = 0;
    if (m_coil1) result |= (1 << 1);
    if (m_coil2) result |= (1 << 2);
    if (m_coil3) result |= (1 << 3);
    if (m_coil4) result |= (1 << 4);
    if (m_autoMode) result |= (1 << 5);
    if (m_powerOn) result |= (1 << 6);
    return result;
}

void BldcDriverDevice::setTargetPwm(quint16 value)
{
    if (value > PWM_MAX) {
        qDebug() << "BLDC: PWM value limited from" << value << "to" << PWM_MAX;
        value = PWM_MAX;
    }
    qDebug() << "BLDC: Set target PWM:" << value;
    markDirty(REG_PWM, value);
}

void BldcDriverDevice::setTimerArr(quint16 value)
{
    qDebug() << "BLDC: Set TIMER_ARR:" << value;
    markDirty(REG_TIMER_ARR, value);
}

void BldcDriverDevice::setStatus(quint8 status)
{
    qDebug() << "BLDC: Set status byte:" << QString("0x%1").arg(status, 2, 16, QChar('0'));

    m_coil1 = (status >> 1) & 0x01;
    m_coil2 = (status >> 2) & 0x01;
    m_coil3 = (status >> 3) & 0x01;
    m_coil4 = (status >> 4) & 0x01;
    m_autoMode = (status >> 5) & 0x01;
    m_powerOn = (status >> 6) & 0x01;

    markDirty(REG_STATUS, status);
}

void BldcDriverDevice::setAutoMode(bool enabled)
{
    qDebug() << "BLDC: Auto mode:" << (enabled ? "ON" : "OFF");
    m_autoMode = enabled;
    setStatus(getCoilsByte());
}

void BldcDriverDevice::setPowerOn(bool on)
{
    qDebug() << "BLDC: Power:" << (on ? "ON" : "OFF");
    m_powerOn = on;
    setStatus(getCoilsByte());
}

void BldcDriverDevice::setPwmValue(quint16 value)
{
    setTargetPwm(value);
}

void BldcDriverDevice::setPwmKhz(quint16 khz)
{
    qDebug() << "BLDC: Set PWM frequency (kHz):" << khz;
    QByteArray command = generateWriteRegisterCommand(REG_PWM, khz);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_pwmKhz = khz;
        emit dataUpdated();
    }
}

void BldcDriverDevice::setPwmHz(quint16 hz)
{
    qDebug() << "BLDC: Set PWM frequency (Hz):" << hz;
    QByteArray command = generateWriteRegisterCommand(REG_PWM_HZ, hz);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_pwmHz = hz;
        emit dataUpdated();
    }
}

void BldcDriverDevice::setSpeed(quint16 rpm)
{
    qDebug() << "BLDC: Set target RPM:" << rpm;
    QByteArray command = generateWriteRegisterCommand(REG_RPM, rpm);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_rpm = rpm;
        emit dataUpdated();
    }
}

void BldcDriverDevice::setCoils(quint8 coilState)
{
    setStatus(coilState);
}

void BldcDriverDevice::readAllRegisters()
{
    qDebug() << "BLDC: Reading all registers...";
    QByteArray command = generateReadAllRegistersCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void BldcDriverDevice::readRpm()
{
    qDebug() << "BLDC: Reading RPM...";
    QByteArray command = generateReadRegisterCommand(REG_RPM);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

// ==================== TO JSON ====================

QJsonObject BldcDriverDevice::toJson() const
{
    QJsonObject obj = Device::toJson();

    obj["pwm_khz"] = m_pwmKhz;
    obj["pwm_hz"] = m_pwmHz;
    obj["rpm"] = m_rpm;
    obj["timer_arr"] = m_timerArr;
    obj["pwm_value"] = m_pwmValue;

    QJsonArray coils;
    coils.append(m_coil1);
    coils.append(m_coil2);
    coils.append(m_coil3);
    coils.append(m_coil4);
    obj["coils"] = coils;
    obj["auto_mode"] = m_autoMode;
    obj["power_on"] = m_powerOn;

    return obj;
}


