


#include "vacuum_pressure_sensor.h"
#include <QDebug>
#include <QDateTime>
#include <cmath>

VacuumPressureSensor::VacuumPressureSensor(int slaveId, QObject *parent)
    : Device(slaveId, Device::TYPE_VACUUM_SENSOR, QString("Vacuum_Sensor_%1").arg(slaveId), parent)
    , m_rawPvValue(0)
    , m_pressure(0.0f)
    , m_floatPressure(0.0f)
    , m_slaveAddressConfig(slaveId)
    , m_baudRate(9600)
    , m_decimalPoints(1)
    , m_unitCode(UNIT_KPA)
    , m_parity(PARITY_NONE)
{
    qDebug() << "VacuumPressureSensor created: slaveId=" << slaveId;
}

VacuumPressureSensor::~VacuumPressureSensor()
{
}

QList<quint16> VacuumPressureSensor::getRegisterAddresses()
{
    // Основные регистры для чтения: PV значение и конфигурация
    return {REG_PV_VALUE, REG_UNIT, REG_DECIMAL_POINTS};
}

// ==================== ГЕНЕРАЦИЯ КОМАНД ====================

QByteArray VacuumPressureSensor::generateReadRegisterCommand(quint16 regAddr, quint16 numRegs) const
{
    QByteArray command;
    command.append(static_cast<char>(m_slaveId));
    command.append(static_cast<char>(FC_READ_HOLDING));
    command.append(static_cast<char>(regAddr >> 8));
    command.append(static_cast<char>(regAddr & 0xFF));
    command.append(static_cast<char>(numRegs >> 8));
    command.append(static_cast<char>(numRegs & 0xFF));

    return Device::appendCRC(command);
}

QByteArray VacuumPressureSensor::generateReadPvValueCommand() const
{
    qDebug() << "VacuumSensor: Reading PV value from register" << QString("0x%1").arg(REG_PV_VALUE, 4, 16, QChar('0'));
    return generateReadRegisterCommand(REG_PV_VALUE, 1);
}

QByteArray VacuumPressureSensor::generateReadFloatValueCommand() const
{
    qDebug() << "VacuumSensor: Reading float value from register" << QString("0x%1").arg(REG_FLOAT_VALUE, 4, 16, QChar('0'));
    return generateReadRegisterCommand(REG_FLOAT_VALUE, 2);  // 2 регистра = 4 байта
}

QByteArray VacuumPressureSensor::generateReadConfigCommand() const
{
    QByteArray command;
    command.append(static_cast<char>(m_slaveId));
    command.append(static_cast<char>(FC_READ_HOLDING));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x07));  // Читаем 7 регистров (0x0000 - 0x0006)

    return Device::appendCRC(command);
}

QByteArray VacuumPressureSensor::generateWriteRegisterCommand(quint16 regAddr, quint16 value) const
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

QByteArray VacuumPressureSensor::generateWriteFloatValueCommand(float value) const
{
    // Преобразование float в 4 байта (большой endian, ABCD формат)
    union FloatBytes {
        float f;
        quint32 i;
        quint8 b[4];
    } converter;
    converter.f = value;

    QByteArray command;
    command.append(static_cast<char>(m_slaveId));
    command.append(static_cast<char>(FC_WRITE_MULTIPLE));
    command.append(static_cast<char>(REG_FLOAT_VALUE >> 8));
    command.append(static_cast<char>(REG_FLOAT_VALUE & 0xFF));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x02));  // 2 регистра
    command.append(static_cast<char>(0x04));  // 4 байта данных

    // ABCD порядок (старший байт первый)
    command.append(static_cast<char>(converter.b[0]));
    command.append(static_cast<char>(converter.b[1]));
    command.append(static_cast<char>(converter.b[2]));
    command.append(static_cast<char>(converter.b[3]));

    return Device::appendCRC(command);
}

// ==================== ПАРСИНГ ОТВЕТОВ ====================

float VacuumPressureSensor::calculatePressure(qint16 rawValue, int decimalPoints) const
{
    // Для отрицательных значений (вакуум) rawValue может быть отрицательным
    float divisor = std::pow(10.0f, decimalPoints);
    return static_cast<float>(rawValue) / divisor;
}

void VacuumPressureSensor::parsePvValue(const QByteArray& data)
{
    if (data.size() < 5) {
        qDebug() << "VacuumSensor: PV response too small, size:" << data.size();
        return;
    }

    // Формат ответа: [address][0x03][0x02][data H][data L][CRC...]
    m_rawPvValue = (static_cast<qint16>(static_cast<quint8>(data[3])) << 8) |
                    static_cast<quint8>(data[4]);

    m_pressure = calculatePressure(m_rawPvValue, m_decimalPoints);

    QString sign = (m_pressure < 0) ? "-" : "";
    qDebug() << QString("VacuumSensor Slave %1: PV raw=%2, pressure=%3%4 %5")
                .arg(m_slaveId)
                .arg(m_rawPvValue)
                .arg(sign)
                .arg(std::abs(m_pressure), 0, 'f', m_decimalPoints)
                .arg(getUnitString());
}

void VacuumPressureSensor::parseFloatValue(const QByteArray& data)
{
    if (data.size() < 7) {
        qDebug() << "VacuumSensor: Float response too small, size:" << data.size();
        return;
    }

    // Формат ответа: [address][0x03][0x04][4 bytes data][CRC...]
    union FloatBytes {
        float f;
        quint32 i;
        quint8 b[4];
    } converter;

    converter.b[0] = static_cast<quint8>(data[3]);
    converter.b[1] = static_cast<quint8>(data[4]);
    converter.b[2] = static_cast<quint8>(data[5]);
    converter.b[3] = static_cast<quint8>(data[6]);

    m_floatPressure = converter.f;

    qDebug() << QString("VacuumSensor Slave %1: Float pressure=%2 %3")
                .arg(m_slaveId)
                .arg(m_floatPressure, 0, 'f', 3)
                .arg(getUnitString());
}

void VacuumPressureSensor::parseConfiguration(const QByteArray& data)
{
    if (data.size() < 17) {  // 7 регистров * 2 байта + заголовок
        qDebug() << "VacuumSensor: Config response too small, size:" << data.size();
        return;
    }

    int byteCount = static_cast<quint8>(data[2]);

    if (byteCount >= 14) {
        // REG_SLAVE_ADDRESS (0x0000)
        m_slaveAddressConfig = (static_cast<quint8>(data[3]) << 8) | static_cast<quint8>(data[4]);

        // REG_BAUD_RATE (0x0001)
        quint16 baudCode = (static_cast<quint8>(data[5]) << 8) | static_cast<quint8>(data[6]);
        switch(baudCode) {
            case BAUD_1200: m_baudRate = 1200; break;
            case BAUD_2400: m_baudRate = 2400; break;
            case BAUD_4800: m_baudRate = 4800; break;
            case BAUD_9600: m_baudRate = 9600; break;
            case BAUD_19200: m_baudRate = 19200; break;
            case BAUD_38400: m_baudRate = 38400; break;
            case BAUD_57600: m_baudRate = 57600; break;
            case BAUD_115200: m_baudRate = 115200; break;
            default: m_baudRate = 9600; break;
        }

        // REG_UNIT (0x0002)
        m_unitCode = (static_cast<quint8>(data[7]) << 8) | static_cast<quint8>(data[8]);

        // REG_DECIMAL_POINTS (0x0003)
        m_decimalPoints = (static_cast<quint8>(data[9]) << 8) | static_cast<quint8>(data[10]);
        if (m_decimalPoints > 4) m_decimalPoints = 1;

        // REG_PV_VALUE (0x0004) - пропускаем, обрабатывается отдельно

        // REG_RANGE_ZERO (0x0005)
        quint16 rangeZero = (static_cast<quint8>(data[13]) << 8) | static_cast<quint8>(data[14]);

        // REG_RANGE_FULL (0x0006)
        quint16 rangeFull = (static_cast<quint8>(data[15]) << 8) | static_cast<quint8>(data[16]);

        qDebug() << QString("VacuumSensor Slave %1 Config: Addr=%2, Baud=%3, Unit=%4 (%5), DecPoints=%6, Range=[%7, %8]")
                    .arg(m_slaveId)
                    .arg(m_slaveAddressConfig)
                    .arg(m_baudRate)
                    .arg(m_unitCode)
                    .arg(getUnitString())
                    .arg(m_decimalPoints)
                    .arg(rangeZero)
                    .arg(rangeFull);
    }
}

void VacuumPressureSensor::parseWriteResponse(const QByteArray& data)
{
    if (data.size() < 8) {
        qDebug() << "VacuumSensor: Write response too small";
        return;
    }

    quint8 funcCode = static_cast<quint8>(data[1]);

    if (funcCode == FC_WRITE_SINGLE) {
        quint16 regAddr = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
        quint16 value = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);

        qDebug() << "VacuumSensor: Write confirmed - register:" << QString("0x%1").arg(regAddr, 4, 16, QChar('0'))
                 << "value:" << value;
    }
}

// ==================== ОСНОВНОЙ МЕТОД ОБРАБОТКИ ====================

QJsonObject VacuumPressureSensor::processData(const QByteArray& data)
{
    QJsonObject result;
    result["device_type"] = "VacuumPressureSensor";
    result["slave_id"] = m_slaveId;
    result["name"] = m_name;

    qDebug() << "VacuumSensor: Raw data size:" << data.size() << "bytes, hex:" << data.toHex();

    if (data.isEmpty()) {
        qDebug() << "VacuumSensor: Empty data received";
        return result;
    }

    quint8 funcCode = static_cast<quint8>(data[1]);

    // Определяем тип ответа по количеству байт данных
    if (data.size() >= 5 && funcCode == FC_READ_HOLDING) {
        quint8 byteCount = static_cast<quint8>(data[2]);

        if (byteCount == 2) {
            parsePvValue(data);
        } else if (byteCount == 4) {
            parseFloatValue(data);
        } else if (byteCount >= 14) {
            parseConfiguration(data);
        }
    } else if (funcCode == FC_WRITE_SINGLE || funcCode == FC_WRITE_MULTIPLE) {
        parseWriteResponse(data);
    } else {
        qDebug() << "VacuumSensor: Unknown function code:" << funcCode;
    }

    // Заполняем JSON результат
    result["pressure"] = m_pressure;
    result["pressure_float"] = m_floatPressure;
    result["raw_value"] = m_rawPvValue;
    result["unit"] = getUnitString();
    result["unit_code"] = m_unitCode;
    result["decimal_points"] = m_decimalPoints;
    result["is_vacuum"] = isNegative();
    result["absolute_vacuum"] = isNegative() ? std::abs(m_pressure) : 0.0f;
    result["baud_rate"] = m_baudRate;
    result["slave_address_config"] = m_slaveAddressConfig;
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    return result;
}

// ==================== ПУБЛИЧНЫЕ МЕТОДЫ УПРАВЛЕНИЯ ====================

QString VacuumPressureSensor::getUnitString() const
{
    switch(m_unitCode) {
        case UNIT_MPA: return "MPa";
        case UNIT_KPA: return "kPa";
        case UNIT_PA: return "Pa";
        case UNIT_BAR: return "bar";
        case UNIT_MBAR: return "mbar";
        case UNIT_KGCM2: return "kg/cm²";
        case UNIT_PSI: return "PSI";
        case UNIT_MH2O: return "mH₂O";
        case UNIT_MMH2O: return "mmH₂O";
        case UNIT_INH2O: return "inH₂O";
        case UNIT_H2O: return "H₂O";
        case UNIT_MHG: return "mHg";
        case UNIT_MMHG: return "mmHg";
        case UNIT_INHG: return "inHg";
        case UNIT_ATM: return "atm";
        case UNIT_TORR: return "Torr";
        case UNIT_M: return "m";
        case UNIT_CM: return "cm";
        case UNIT_MM: return "mm";
        case UNIT_KG: return "kg";
        case UNIT_C: return "°C";
        case UNIT_PH: return "pH";
        case UNIT_F: return "°F";
        default: return "unknown";
    }
}

void VacuumPressureSensor::setSlaveAddress(quint8 newAddress)
{
    if (newAddress < 1) {
        qDebug() << "VacuumSensor: Invalid address:" << newAddress;
        return;
    }

    qDebug() << "VacuumSensor: Setting device address to:" << newAddress;
    QByteArray command = generateWriteRegisterCommand(REG_SLAVE_ADDRESS, newAddress);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_slaveAddressConfig = newAddress;
    }
}

void VacuumPressureSensor::setBaudRate(int baudRate)
{
    quint8 baudCode;
    switch(baudRate) {
        case 1200: baudCode = BAUD_1200; break;
        case 2400: baudCode = BAUD_2400; break;
        case 4800: baudCode = BAUD_4800; break;
        case 9600: baudCode = BAUD_9600; break;
        case 19200: baudCode = BAUD_19200; break;
        case 38400: baudCode = BAUD_38400; break;
        case 57600: baudCode = BAUD_57600; break;
        case 115200: baudCode = BAUD_115200; break;
        default:
            qDebug() << "VacuumSensor: Unsupported baud rate:" << baudRate << ", using 9600";
            baudCode = BAUD_9600;
            break;
    }

    qDebug() << "VacuumSensor: Setting baud rate to:" << baudRate;
    QByteArray command = generateWriteRegisterCommand(REG_BAUD_RATE, baudCode);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_baudRate = baudRate;
    }
}

void VacuumPressureSensor::setParity(ParityCode parity)
{
    qDebug() << "VacuumSensor: Setting parity to:" << parity;
    QByteArray command = generateWriteRegisterCommand(REG_PARITY, static_cast<quint16>(parity));
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_parity = parity;
    }
}

void VacuumPressureSensor::setZeroOffset(qint16 offset)
{
    qDebug() << "VacuumSensor: Setting zero offset to:" << offset;
    QByteArray command = generateWriteRegisterCommand(REG_ZERO_OFFSET, static_cast<quint16>(offset));
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void VacuumPressureSensor::saveToUserArea()
{
    qDebug() << "VacuumSensor: Saving configuration to user area";
    QByteArray command = generateWriteRegisterCommand(REG_SAVE, 0x0000);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void VacuumPressureSensor::restoreFactorySettings()
{
    qDebug() << "VacuumSensor: Restoring factory settings";
    QByteArray command = generateWriteRegisterCommand(REG_RESTORE_FACTORY, 0x0001);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void VacuumPressureSensor::readPvValue()
{
    qDebug() << "VacuumSensor: Reading PV value...";
    QByteArray command = generateReadPvValueCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void VacuumPressureSensor::readFloatValue()
{
    qDebug() << "VacuumSensor: Reading float value...";
    QByteArray command = generateReadFloatValueCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void VacuumPressureSensor::readConfiguration()
{
    qDebug() << "VacuumSensor: Reading configuration...";
    QByteArray command = generateReadConfigCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

// ==================== TO JSON ====================

QJsonObject VacuumPressureSensor::toJson() const
{
    QJsonObject obj = Device::toJson();

    obj["pressure"] = m_pressure;
    obj["pressure_float"] = m_floatPressure;
    obj["raw_value"] = m_rawPvValue;
    obj["unit"] = getUnitString();
    obj["unit_code"] = m_unitCode;
    obj["decimal_points"] = m_decimalPoints;
    obj["is_vacuum"] = isNegative();
    obj["absolute_vacuum"] = isNegative() ? std::abs(m_pressure) : 0.0f;
    obj["baud_rate"] = m_baudRate;
    obj["slave_address_config"] = m_slaveAddressConfig;
    obj["parity"] = m_parity;

    return obj;
}


