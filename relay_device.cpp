


#include "relay_device.h"
#include "device.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>

// Определение статических констант
const quint16 RelayDevice::RELAY_ADDRESSES[8] = {0x0000, 0x0001, 0x0002, 0x0003,
                                                  0x0004, 0x0005, 0x0006, 0x0007};

RelayDevice::RelayDevice(int slaveId, QObject *parent)
    : Device(slaveId, Device::TYPE_RELAY, QString("Relay_Block_%1").arg(slaveId), parent)
    , m_deviceAddress(static_cast<quint8>(slaveId))
    , m_baudRate(9600)
    , m_role(ROLE_UNDEFINED)  // ИНИЦИАЛИЗАЦИЯ НОВОГО ПОЛЯ
{
    for (int i = 0; i < 8; i++) {
        m_relayStates[i] = false;
        m_optocouplerStates[i] = false;
        // Инициализация имен по умолчанию
        m_relayNames[i] = QString("Relay_%1").arg(i + 1);
    }

    qDebug() << "RelayDevice created: slaveId=" << slaveId << ", address=" << m_deviceAddress;
}

RelayDevice::~RelayDevice()
{
}

QList<quint16> RelayDevice::getRegisterAddresses()
{
    return {0x0000, 0x03E8};
}

// ==================== НОВЫЕ МЕТОДЫ УПРАВЛЕНИЯ РОЛЯМИ ====================

QString RelayDevice::getRole() const
{
    switch(m_role) {
        case ROLE_POWER_CONTROL: return "power_control";
        case ROLE_COOLING_SYSTEM: return "cooling_system";
        case ROLE_AUXILIARY: return "auxiliary";
        case ROLE_SAFETY: return "safety";
        case ROLE_LIGHTING: return "lighting";
        case ROLE_ALARM: return "alarm";
        default: return "undefined";
    }
}

void RelayDevice::setRole(RelayRole role)
{
    if (m_role != role) {
        m_role = role;

        // Автоматически устанавливаем имена реле в зависимости от роли
        switch(role) {
            case ROLE_POWER_CONTROL:
                setRelayName(1, "Ballast_Resistor");
                setRelayName(2, "Main_Power");
                setRelayName(3, "Standby_Power");
                setRelayName(4, "Aux_Power");
                break;

            case ROLE_COOLING_SYSTEM:
                setRelayName(1, "Cooling_Pump");
                setRelayName(2, "Radiator_Fan");
                setRelayName(3, "Chiller");
                setRelayName(4, "Ventilation");
                break;

            case ROLE_SAFETY:
                setRelayName(1, "Emergency_Stop");
                setRelayName(2, "Fire_Suppression");
                setRelayName(3, "Gas_Valve");
                setRelayName(4, "Alarm_Siren");
                break;

            case ROLE_LIGHTING:
                setRelayName(1, "Main_Light");
                setRelayName(2, "Work_Light");
                setRelayName(3, "Emergency_Light");
                break;

            case ROLE_ALARM:
                setRelayName(1, "Siren");
                setRelayName(2, "Strobe");
                setRelayName(3, "Notification");
                break;

            default:
                // Оставляем имена по умолчанию
                for (int i = 0; i < 8; i++) {
                    m_relayNames[i] = QString("Relay_%1").arg(i + 1);
                }
                break;
        }

        emit roleChanged();
        qDebug() << "RelayDevice role changed:" << getRole() << "for slave" << m_slaveId;
    }
}

void RelayDevice::setRole(const QString& role)
{
    if (role == "power_control") setRole(ROLE_POWER_CONTROL);
    else if (role == "cooling_system") setRole(ROLE_COOLING_SYSTEM);
    else if (role == "auxiliary") setRole(ROLE_AUXILIARY);
    else if (role == "safety") setRole(ROLE_SAFETY);
    else if (role == "lighting") setRole(ROLE_LIGHTING);
    else if (role == "alarm") setRole(ROLE_ALARM);
    else setRole(ROLE_UNDEFINED);
}

// ==================== НОВЫЕ МЕТОДЫ УПРАВЛЕНИЯ ИМЕНАМИ РЕЛЕ ====================

void RelayDevice::setRelayName(int relayNumber, const QString& name)
{
    if (relayNumber >= 1 && relayNumber <= 8) {
        if (m_relayNames[relayNumber - 1] != name) {
            m_relayNames[relayNumber - 1] = name;
            qDebug() << "Relay" << relayNumber << "renamed to:" << name;
        }
    }
}

QString RelayDevice::getRelayName(int relayNumber) const
{
    if (relayNumber >= 1 && relayNumber <= 8) {
        return m_relayNames[relayNumber - 1];
    }
    return QString();
}

void RelayDevice::loadRelayMapping(const QMap<int, QString>& mapping)
{
    for (auto it = mapping.begin(); it != mapping.end(); ++it) {
        setRelayName(it.key(), it.value());
    }
}

int RelayDevice::getRelayNumberByName(const QString& name) const
{
    for (int i = 0; i < 8; i++) {
        if (m_relayNames[i] == name) {
            return i + 1;
        }
    }
    return -1;
}

// ==================== НОВЫЕ МЕТОДЫ УПРАВЛЕНИЯ ПО ИМЕНАМ ====================

void RelayDevice::setRelayByName(const QString& relayName, bool on)
{
    int relayNumber = getRelayNumberByName(relayName);
    if (relayNumber != -1) {
        setRelay(relayNumber, on);
    } else {
        qDebug() << "Relay name not found:" << relayName;
    }
}

bool RelayDevice::getRelayState(int relayNumber) const
{
    if (relayNumber >= 1 && relayNumber <= 8) {
        return m_relayStates[relayNumber - 1];
    }
    return false;
}

bool RelayDevice::getRelayStateByName(const QString& relayName) const
{
    int relayNumber = getRelayNumberByName(relayName);
    if (relayNumber != -1) {
        return m_relayStates[relayNumber - 1];
    }
    return false;
}

// ==================== ГЕНЕРАЦИЯ КОМАНД (БЕЗ ИЗМЕНЕНИЙ) ====================

QByteArray RelayDevice::generateReadRelayStatusCommand() const
{
    QByteArray command;
    command.append(static_cast<char>(m_deviceAddress));
    command.append(static_cast<char>(FC_READ_COILS));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x08));

    return Device::appendCRC(command);
}

QByteArray RelayDevice::generateReadOptocouplerCommand() const
{
    QByteArray command;
    command.append(static_cast<char>(m_deviceAddress));
    command.append(static_cast<char>(FC_READ_DISCRETE));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x08));

    return Device::appendCRC(command);
}

QByteArray RelayDevice::generateReadAddressCommand() const
{
    QByteArray command;
    command.append(static_cast<char>(0x00));  // Широковещательный запрос
    command.append(static_cast<char>(FC_READ_HOLDING));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x01));

    return Device::appendCRC(command);
}

QByteArray RelayDevice::generateReadBaudRateCommand() const
{
    QByteArray command;
    command.append(static_cast<char>(m_deviceAddress));
    command.append(static_cast<char>(FC_READ_HOLDING));
    command.append(static_cast<char>(0x03));
    command.append(static_cast<char>(0xE8));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x01));

    return Device::appendCRC(command);
}

QByteArray RelayDevice::generateSetRelayCommand(int relayNumber, bool on) const
{
    if (relayNumber < 1 || relayNumber > 8) {
        qDebug() << "Invalid relay number:" << relayNumber;
        return QByteArray();
    }

    QByteArray command;
    command.append(static_cast<char>(m_deviceAddress));
    command.append(static_cast<char>(FC_WRITE_SINGLE));

    quint16 address = RELAY_ADDRESSES[relayNumber - 1];
    command.append(static_cast<char>(address >> 8));
    command.append(static_cast<char>(address & 0xFF));

    // FF00 = ON, 0000 = OFF
    command.append(static_cast<char>(on ? 0xFF : 0x00));
    command.append(static_cast<char>(on ? 0x00 : 0x00));

    return Device::appendCRC(command);
}

QByteArray RelayDevice::generateSetAllRelaysCommand(bool on) const
{
    QByteArray command;
    command.append(static_cast<char>(m_deviceAddress));
    command.append(static_cast<char>(FC_WRITE_MULTIPLE));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x08));
    command.append(static_cast<char>(0x01));  // 1 байт данных

    quint8 data = on ? 0xFF : 0x00;
    command.append(static_cast<char>(data));

    return Device::appendCRC(command);
}

QByteArray RelayDevice::generateSetAddressCommand(quint8 newAddress) const
{
    QByteArray command;
    command.append(static_cast<char>(0x00));  // Широковещательный адрес
    command.append(static_cast<char>(FC_WRITE_MULTIPLE_REG));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x01));
    command.append(static_cast<char>(0x02));  // 2 байта данных
    command.append(static_cast<char>(0x00));  // Старший байт
    command.append(static_cast<char>(newAddress));  // Младший байт - новый адрес

    return Device::appendCRC(command);
}

QByteArray RelayDevice::generateSetBaudRateCommand(int baudRate) const
{
    // Преобразуем скорость бода в код
    quint8 baudCode;
    switch(baudRate) {
        case 4800:  baudCode = 0x02; break;
        case 9600:  baudCode = 0x03; break;
        case 19200: baudCode = 0x04; break;
        default:
            qDebug() << "Unsupported baud rate:" << baudRate << ", using 9600";
            baudCode = 0x03;
            break;
    }

    QByteArray command;
    command.append(static_cast<char>(m_deviceAddress));
    command.append(static_cast<char>(FC_WRITE_MULTIPLE_REG));
    command.append(static_cast<char>(0x03));
    command.append(static_cast<char>(0xE9));
    command.append(static_cast<char>(0x00));
    command.append(static_cast<char>(0x01));
    command.append(static_cast<char>(0x02));  // 2 байта данных
    command.append(static_cast<char>(0x00));  // Старший байт
    command.append(static_cast<char>(baudCode));  // Код скорости

    return Device::appendCRC(command);
}

// ==================== ПАРСИНГ ОТВЕТОВ (С ДОБАВЛЕНИЕМ СИГНАЛОВ) ====================

void RelayDevice::parseRelayStatus(const QByteArray& data)
{
    if (data.size() >= 4) {
        quint8 statusByte = static_cast<quint8>(data[3]);

        for (int i = 0; i < 8; i++) {
            bool newState = (statusByte >> i) & 0x01;
            if (m_relayStates[i] != newState) {
                m_relayStates[i] = newState;
                emit relayStateChanged(i + 1, newState);
                emit relayStateChangedByName(m_relayNames[i], newState);
                qDebug() << QString("Relay %1 (%2) changed: %3")
                            .arg(i + 1)
                            .arg(m_relayNames[i])
                            .arg(newState ? "ON" : "OFF");
            }
        }

        qDebug() << QString("Relay states: 0x%1").arg(statusByte, 2, 16, QChar('0'));
    }
}

void RelayDevice::parseOptocouplerStatus(const QByteArray& data)
{
    if (data.size() >= 4) {
        quint8 statusByte = static_cast<quint8>(data[3]);

        for (int i = 0; i < 8; i++) {
            m_optocouplerStates[i] = (statusByte >> i) & 0x01;
        }

        qDebug() << QString("Optocoupler states: 0x%1").arg(statusByte, 2, 16, QChar('0'));
    }
}

void RelayDevice::parseAddress(const QByteArray& data)
{
    if (data.size() >= 5) {
        m_deviceAddress = static_cast<quint8>(data[4]);
        qDebug() << "Device address:" << m_deviceAddress;
    }
}

void RelayDevice::parseBaudRate(const QByteArray& data)
{
    if (data.size() >= 5) {
        quint8 baudCode = static_cast<quint8>(data[4]);

        switch(baudCode) {
            case 0x02: m_baudRate = 4800; break;
            case 0x03: m_baudRate = 9600; break;
            case 0x04: m_baudRate = 19200; break;
            default: m_baudRate = 9600; break;
        }

        qDebug() << "Baud rate:" << m_baudRate << "(code:" << QString("0x%1").arg(baudCode, 2, 16, QChar('0')) << ")";
    }
}

void RelayDevice::parseWriteResponse(const QByteArray& data)
{
    qDebug() << "Write command confirmed:" << data.toHex();

    if (data.size() >= 8 && static_cast<quint8>(data[1]) == FC_WRITE_SINGLE) {
        quint16 address = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
        bool on = (static_cast<quint8>(data[4]) == 0xFF);

        for (int i = 0; i < 8; i++) {
            if (RELAY_ADDRESSES[i] == address) {
                m_relayStates[i] = on;
                emit relayStateChanged(i + 1, on);
                emit relayStateChangedByName(m_relayNames[i], on);
                qDebug() << QString("Relay %1 (%2) confirmed: %3")
                            .arg(i + 1)
                            .arg(m_relayNames[i])
                            .arg(on ? "ON" : "OFF");
                break;
            }
        }
    }
}

// ==================== ОСНОВНОЙ МЕТОД ОБРАБОТКИ (С ДОБАВЛЕНИЕМ ROLE) ====================

QJsonObject RelayDevice::processData(const QByteArray& data)
{
    QJsonObject result;
    result["device_type"] = "RelayBlock";
    result["slave_id"] = m_slaveId;
    result["name"] = m_name;
    result["role"] = getRole();  // ДОБАВЛЯЕМ РОЛЬ В РЕЗУЛЬТАТ

    qDebug() << "RelayDevice: Raw data size:" << data.size() << "bytes, hex:" << data.toHex();

    if (data.isEmpty()) {
        qDebug() << "RelayDevice: Empty data received";
        return result;
    }

    quint8 funcCode = static_cast<quint8>(data[1]);

    switch(funcCode) {
        case FC_READ_COILS:
            parseRelayStatus(data);
            break;

        case FC_READ_DISCRETE:
            parseOptocouplerStatus(data);
            break;

        case FC_READ_HOLDING:
            if (data.size() >= 5) {
                quint16 regValue = (static_cast<quint8>(data[3]) << 8) | static_cast<quint8>(data[4]);
                if (regValue <= 255) {
                    parseAddress(data);
                } else {
                    parseBaudRate(data);
                }
            }
            break;

        case FC_WRITE_SINGLE:
        case FC_WRITE_MULTIPLE:
        case FC_WRITE_MULTIPLE_REG:
            parseWriteResponse(data);
            break;

        default:
            qDebug() << "RelayDevice: Unknown function code:" << funcCode;
            break;
    }

    QJsonArray relayArray;
    QJsonArray optoArray;
    QJsonObject relayStatesNamed;  // НОВОЕ: именованные состояния

    for (int i = 0; i < 8; i++) {
        relayArray.append(m_relayStates[i]);
        optoArray.append(m_optocouplerStates[i]);
        relayStatesNamed[m_relayNames[i]] = m_relayStates[i];  // ДОБАВЛЯЕМ ИМЕНОВАННЫЕ СОСТОЯНИЯ
    }

    result["relay_states"] = relayArray;
    result["relay_states_named"] = relayStatesNamed;  // НОВОЕ ПОЛЕ
    result["optocoupler_states"] = optoArray;
    result["device_address"] = m_deviceAddress;
    result["baud_rate"] = m_baudRate;
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString relayStr;
    QString optoStr;
    for (int i = 0; i < 8; i++) {
        relayStr += m_relayStates[i] ? "1" : "0";
        optoStr += m_optocouplerStates[i] ? "1" : "0";
    }

    qDebug() << QString("RelayDevice Slave %1 [%2]: Addr=%3, Baud=%4, Relays=%5, Opto=%6")
                .arg(m_slaveId)
                .arg(getRole())
                .arg(m_deviceAddress)
                .arg(m_baudRate)
                .arg(relayStr)
                .arg(optoStr);

    return result;
}

// ==================== ПУБЛИЧНЫЕ МЕТОДЫ УПРАВЛЕНИЯ (БЕЗ ИЗМЕНЕНИЙ) ====================

void RelayDevice::setRelay(int relayNumber, bool on)
{
    if (relayNumber < 1 || relayNumber > 8) {
        qDebug() << "Invalid relay number:" << relayNumber;
        return;
    }

    qDebug() << QString("Set relay %1 (%2): %3")
                .arg(relayNumber)
                .arg(m_relayNames[relayNumber - 1])
                .arg(on ? "ON" : "OFF");

    QByteArray command = generateSetRelayCommand(relayNumber, on);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_relayStates[relayNumber - 1] = on;
    }
}

void RelayDevice::setAllRelays(bool on)
{
    qDebug() << "Set all relays:" << (on ? "ON" : "OFF");

    QByteArray command = generateSetAllRelaysCommand(on);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        for (int i = 0; i < 8; i++) {
            m_relayStates[i] = on;
        }
    }
}

void RelayDevice::readRelayStatus()
{
    qDebug() << "Reading relay status...";
    QByteArray command = generateReadRelayStatusCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void RelayDevice::readOptocouplerStatus()
{
    qDebug() << "Reading optocoupler status...";
    QByteArray command = generateReadOptocouplerCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void RelayDevice::setDeviceAddress(quint8 newAddress)
{
    if (newAddress < 1) {
        qDebug() << "Invalid address:" << newAddress;
        return;
    }

    qDebug() << "Setting device address to:" << newAddress;
    QByteArray command = generateSetAddressCommand(newAddress);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_deviceAddress = newAddress;
    }
}

void RelayDevice::readDeviceAddress()
{
    qDebug() << "Reading device address...";
    QByteArray command = generateReadAddressCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

void RelayDevice::setBaudRate(int baudRate)
{
    qDebug() << "Setting baud rate to:" << baudRate;

    QByteArray command = generateSetBaudRateCommand(baudRate);
    if (!command.isEmpty()) {
        emit commandGenerated(command);
        m_baudRate = baudRate;
    }
}

void RelayDevice::readBaudRate()
{
    qDebug() << "Reading baud rate...";
    QByteArray command = generateReadBaudRateCommand();
    if (!command.isEmpty()) {
        emit commandGenerated(command);
    }
}

// ==================== TO JSON (С ДОБАВЛЕНИЕМ ROLE) ====================

QJsonObject RelayDevice::toJson() const
{
    QJsonObject obj = Device::toJson();

    QJsonArray relayArray;
    QJsonArray optoArray;
    QJsonObject relayNamesObj;

    for (int i = 0; i < 8; i++) {
        relayArray.append(m_relayStates[i]);
        optoArray.append(m_optocouplerStates[i]);
        relayNamesObj[QString::number(i + 1)] = m_relayNames[i];
    }

    obj["relay_states"] = relayArray;
    obj["optocoupler_states"] = optoArray;
    obj["relay_names"] = relayNamesObj;
    obj["device_address"] = m_deviceAddress;
    obj["baud_rate"] = m_baudRate;
    obj["role"] = getRole();  // ДОБАВЛЯЕМ РОЛЬ

    return obj;
}


