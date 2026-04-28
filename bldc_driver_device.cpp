


#include "bldc_driver_device.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <QTimer>

// ==================== ОПРЕДЕЛЕНИЯ СТАТИЧЕСКИХ КОНСТАНТ ====================
constexpr quint8  BldcDriverDevice::FC_READ_HOLDING;
constexpr quint8  BldcDriverDevice::FC_WRITE_SINGLE;
constexpr quint8  BldcDriverDevice::FC_WRITE_MULTIPLE;

constexpr quint16 BldcDriverDevice::REG_FREQ_HI;
constexpr quint16 BldcDriverDevice::REG_FREQ_LO;
constexpr quint16 BldcDriverDevice::REG_RPM;
constexpr quint16 BldcDriverDevice::REG_TIMER_ARR;
constexpr quint16 BldcDriverDevice::REG_PWM_CH1;
constexpr quint16 BldcDriverDevice::REG_PWM_CH2;
constexpr quint16 BldcDriverDevice::REG_PWM_CH3;
constexpr quint16 BldcDriverDevice::REG_STATUS;
constexpr quint16 BldcDriverDevice::REG_PWM_GEN;

constexpr quint16 BldcDriverDevice::PWM_MIN;
constexpr quint16 BldcDriverDevice::PWM_MAX;
constexpr quint16 BldcDriverDevice::TIMER_ARR_MIN;
constexpr quint16 BldcDriverDevice::TIMER_ARR_MAX;

//constexpr int BldcDriverDevice::WRITE_DEBOUNCE_MS;
constexpr int BldcDriverDevice::NUM_REGISTERS;

// ==================== КОНСТРУКТОР ====================
BldcDriverDevice::BldcDriverDevice(int slaveId, QObject *parent)
    : Device(slaveId, Device::TYPE_BLDC_DRIVER, QString("BLDC_Driver_%1").arg(slaveId), parent)
        , m_frequencyHz(0)
        , m_rpm(0)
        , m_timerArr(0)
        , m_pwmValue(0)
        , m_statusByte(0)
        , m_targetPwm(0)
        , m_targetTimerArr(0)
        , m_targetStatus(0)
        , m_targetPwmGen(0)
        , m_pwmGenValue(0)
        , m_pwmDirty(false)
        , m_timerArrDirty(false)
        , m_statusDirty(false)
        , m_pwmGenDirty(false)
        , m_coil1(false)
        , m_coil2(false)
        , m_coil3(false)
        , m_coil4(false)
        , m_autoMode(false)
        , m_powerOn(false)
        , m_bldcMode(false)
        , m_genMode(false)
{
    qDebug() << "BldcDriverDevice created: slaveId=" << slaveId;

    m_writeTimer = new QTimer(this);
    m_writeTimer->setSingleShot(true);
    connect(m_writeTimer, &QTimer::timeout, this, &BldcDriverDevice::onWriteTimerTimeout);
}

BldcDriverDevice::~BldcDriverDevice()
{
}

// ==================== DEVICE INTERFACE ====================
QList<quint16> BldcDriverDevice::getRegisterAddresses()
{
    // Читаем 9 регистров: 0x0000 - 0x0008
    return {0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,0x0008};
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

QByteArray BldcDriverDevice::generateWriteMultipleCommand(const QList<quint16>& addresses, const QList<quint16>& values) const
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

    // Проверяем, что получили все 9 регистров (18 байт)
    if (regData.size() >= 18) {
        // ИСПРАВЛЕНО: Правильные смещения согласно data_reg[] в прошивке
        // data_reg[0-1]: FREQ (4 байта, но пишется одинаковое значение)
        m_frequencyHz = (static_cast<quint8>(regData[0]) << 8) | static_cast<quint8>(regData[1]);

        // data_reg[2]: RPM (смещение 4 байта)
        m_rpm = (static_cast<quint8>(regData[4]) << 8) | static_cast<quint8>(regData[5]);

        // data_reg[3]: TIMER_ARR (смещение 6 байт)
        m_timerArr = (static_cast<quint8>(regData[6]) << 8) | static_cast<quint8>(regData[7]);

        // data_reg[4]: PWM_CH1 (смещение 8 байт)
        m_pwmValue = (static_cast<quint8>(regData[8]) << 8) | static_cast<quint8>(regData[9]);

        // data_reg[7]: STATUS - В ПРОШИВКЕ НЕ ЗАПОЛНЯЕТСЯ! (смещение 14 байт)
        m_statusByte = static_cast<quint8>(regData[14]);

        // data_reg[8]: PWM_GEN (смещение 16 байт)
        m_targetPwmGen = (static_cast<quint8>(regData[16]) << 8) | static_cast<quint8>(regData[17]);

        updateFromStatusByte();

        qDebug() << QString("BLDC Slave %1: FREQ=%2 Hz, RPM=%3, ARR=%4, PWM=%5, STATUS=0x%6, PWM_GEN=%7")
                    .arg(m_slaveId)
                    .arg(m_frequencyHz)
                    .arg(m_rpm)
                    .arg(m_timerArr)
                    .arg(m_pwmValue)
                    .arg(m_statusByte, 2, 16, QChar('0'))
                    .arg(m_targetPwmGen);
        emit dataUpdated();
    } else {
        qDebug() << "BldcDriverDevice: Invalid register data size:" << regData.size();
    }
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
        qDebug() << "BLDC: Write confirmed - register 0x" << QString::number(regAddr, 16) << " value:" << value;
    } else if (funcCode == FC_WRITE_MULTIPLE) {
        qDebug() << "BLDC: Multiple write confirmed";
    }
}

void BldcDriverDevice::updateFromStatusByte()
{
    // Биты соответствуют прошивке:
    // coil_1    = (rcv_data_reg[7] >> 1) & 0x01;
    // coil_2    = (rcv_data_reg[7] >> 2) & 0x01;
    // coil_3    = (rcv_data_reg[7] >> 3) & 0x01;
    // coil_4    = (rcv_data_reg[7] >> 4) & 0x01;
    // auto_mode = (rcv_data_reg[7] >> 5) & 0x01;
    // pwr_on    = (rcv_data_reg[7] >> 6) & 0x01;
    // bldc_mode = (rcv_data_reg[7] >> 7) & 0x01;

    m_coil1 = (m_statusByte >> 1) & 0x01;
    m_coil2 = (m_statusByte >> 2) & 0x01;
    m_coil3 = (m_statusByte >> 3) & 0x01;
    m_coil4 = (m_statusByte >> 4) & 0x01;
    m_autoMode = (m_statusByte >> 5) & 0x01;
    m_powerOn = (m_statusByte >> 6) & 0x01;
    m_bldcMode = (m_statusByte >> 7) & 0x01;
    // Биты 0 и 1 - не используются в прошивке

    qDebug() << QString("  Status: Coils=%1%2%3%4, Auto=%5, Power=%6, BLDC_Mode=%7")
                .arg(m_coil1 ? "1" : "0")
                .arg(m_coil2 ? "1" : "0")
                .arg(m_coil3 ? "1" : "0")
                .arg(m_coil4 ? "1" : "0")
                .arg(m_autoMode ? "ON" : "OFF")
                .arg(m_powerOn ? "ON" : "OFF")
                .arg(m_bldcMode ? "GEN" : "DRIVE");
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

    result["frequency_hz"] = m_frequencyHz;
    result["rpm"] = m_rpm;
    result["timer_arr"] = m_timerArr;
    result["pwm_value"] = m_pwmValue;
    result["pwm_gen"] = m_targetPwmGen;
    result["status_byte"] = m_statusByte;

    QJsonArray coils;
    coils.append(m_coil1);
    coils.append(m_coil2);
    coils.append(m_coil3);
    coils.append(m_coil4);
    result["coils"] = coils;
    result["auto_mode"] = m_autoMode;
    result["power_on"] = m_powerOn;
    result["bldc_mode"] = m_bldcMode;  // 1 = генерация, 0 = двигатель
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    return result;
}

// ==================== КЭШИРОВАНИЕ И ГРУППОВАЯ ЗАПИСЬ ====================

/*
void BldcDriverDevice::markDirty(quint16 regAddr, quint16 value)
{
    switch(regAddr) {
        case REG_PWM_CH1:
            m_targetPwm = value;
            break;
        case REG_TIMER_ARR:
            m_targetTimerArr = value;
            break;
        case REG_STATUS:
            m_targetStatus = static_cast<quint8>(value);
            break;
        case REG_PWM_GEN:
            m_targetPwmGen = value;
            m_pwmGenDirty = true;
            break;
        default:
            break;
    }
    // При любом изменении помечаем все регистры как dirty для групповой записи
    m_pwmDirty = true;
    m_timerArrDirty = true;
    m_statusDirty = true;

    scheduleWrite();
}

*/

void BldcDriverDevice::markDirty(quint16 regAddr, quint16 value)
{
    switch(regAddr) {
        case REG_PWM_CH1:
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

        case REG_PWM_GEN:
            m_targetPwmGen = value;
            m_pwmGenDirty = true;
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

    // Регистры для записи: 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008
    addresses.append(REG_TIMER_ARR);  // 0x0003
    values.append(m_targetTimerArr);

    addresses.append(REG_PWM_CH1);    // 0x0004
    values.append(m_targetPwm);

    addresses.append(REG_PWM_CH2);    // 0x0005
    values.append(m_targetPwm);

    addresses.append(REG_PWM_CH3);    // 0x0006
    values.append(m_targetPwm);

    addresses.append(REG_STATUS);     // 0x0007
    values.append(m_targetStatus);

    addresses.append(REG_PWM_GEN);    // 0x0008
    values.append(m_targetPwmGen);

    if (!addresses.isEmpty()) {
        QByteArray command = generateWriteMultipleCommand(addresses, values);
        if (!command.isEmpty()) {
            emit commandGenerated(command);
            qDebug() << "📤 BLDC: Sent MULTIPLE write command for" << addresses.size()
                     << "registers (0x0003-0x0008)";
        }
    }

    m_pwmDirty = false;
    m_timerArrDirty = false;
    m_statusDirty = false;
    m_pwmGenDirty = false;
}



void BldcDriverDevice::flushWriteCache()
{
    if (m_writeTimer->isActive()) {
        m_writeTimer->stop();
    }
    onWriteTimerTimeout();
}

// ==================== ПУБЛИЧНЫЕ МЕТОДЫ УПРАВЛЕНИЯ ====================
quint8 BldcDriverDevice::getStatusByte() const
{
    quint8 result = 0;
    // Биты строго как в прошивке (rcv_data_reg[7])
    if (m_coil1) result |= (1 << 1);
    if (m_coil2) result |= (1 << 2);
    if (m_coil3) result |= (1 << 3);
    if (m_coil4) result |= (1 << 4);
    if (m_autoMode) result |= (1 << 5);
    if (m_powerOn) result |= (1 << 6);
    if (m_bldcMode) result |= (1 << 7);
    // Бит 0 не используется
    return result;
}

void BldcDriverDevice::setTargetPwm(quint16 value)
{
    if (value > PWM_MAX) {
        qDebug() << "BLDC: PWM value limited from" << value << "to" << PWM_MAX;
        value = PWM_MAX;
    }
    qDebug() << "BLDC: Set target PWM (reg 0x0004):" << value;
    markDirty(REG_PWM_CH1, value);
}

void BldcDriverDevice::setTimerArr(quint16 value)
{
    qDebug() << "BLDC: Set TIMER_ARR (reg 0x0003):" << value;
    markDirty(REG_TIMER_ARR, value);
}

void BldcDriverDevice::setPwmGen(quint16 value)
{
    qDebug() << "BLDC: Set PWM GEN (reg 0x0008):" << value;
    markDirty(REG_PWM_GEN, value);
}

void BldcDriverDevice::setStatus(quint8 status)
{
    qDebug() << "BLDC: Set status byte (reg 0x0007): 0x" << QString::number(status, 16);
    markDirty(REG_STATUS, status);
}

void BldcDriverDevice::setAutoMode(bool enabled)
{
    qDebug() << "BLDC: Auto mode:" << (enabled ? "ON" : "OFF");
    m_autoMode = enabled;
    setStatus(getStatusByte());
}

void BldcDriverDevice::setPowerOn(bool on)
{
    qDebug() << "BLDC: Power:" << (on ? "ON" : "OFF");
    m_powerOn = on;
    setStatus(getStatusByte());
}

void BldcDriverDevice::setBldcMode(bool genMode)
{
    qDebug() << "BLDC: Mode:" << (genMode ? "GENERATOR (regen)" : "DRIVE (motor)");
    m_bldcMode = genMode;  // true = генерация, false = двигатель
    setStatus(getStatusByte());
}

void BldcDriverDevice::readAllRegisters()
{
    qDebug() << "BLDC: Reading all registers (0x0000-0x0007)...";
    QByteArray command = generateReadAllRegistersCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

// ==================== TO JSON ====================
QJsonObject BldcDriverDevice::toJson() const
{
    QJsonObject obj = Device::toJson();
    obj["frequency_hz"] = m_frequencyHz;
    obj["rpm"] = m_rpm;
    obj["timer_arr"] = m_timerArr;
    obj["pwm_value"] = m_pwmValue;
    obj["status_byte"] = m_statusByte;
    obj["pwm_gen"] = m_targetPwmGen;

    QJsonArray coils;
    coils.append(m_coil1);
    coils.append(m_coil2);
    coils.append(m_coil3);
    coils.append(m_coil4);
    obj["coils"] = coils;

    obj["auto_mode"] = m_autoMode;
    obj["power_on"] = m_powerOn;
    obj["bldc_mode"] = m_bldcMode;  // Добавлено: режим генерации/двигателя

    return obj;
}




