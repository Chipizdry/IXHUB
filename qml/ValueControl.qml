


import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root
    width: 220
    height: 150
    color: "transparent"

    // Свойства для внешних данных
    property real currentValue: 0.0      // ток, А
    property real voltageValue: 0.0      // напряжение, В
    property real powerValue: 0.0        // мощность, Вт (может быть отрицательной)
    property real energyWh: 0.0          // энергия, Вт·ч

    // Текущий режим отображения: 0 - ток, 1 - напряжение, 2 - мощность, 3 - энергия
    property int displayMode: 0
    onDisplayModeChanged: updateDisplay()

    // Внутренние переменные
    property string displayText: "0.00"
    property string displayUnit: "A"

    // Функция форматирования числа с двумя знаками
    function formatNumber(value) {
        return value.toFixed(2)
    }

    // Обновление отображаемого значения и единиц
    function updateDisplay() {
        switch (displayMode) {
        case 0: // ток
            displayText = formatNumber(currentValue)
            displayUnit = "A"
            break
        case 1: // напряжение
            displayText = formatNumber(voltageValue)
            displayUnit = "V"
            break
        case 2: // мощность
            displayText = formatNumber(powerValue)
            displayUnit = "W"
            break
        case 3: // энергия
            if (energyWh >= 1000) {
                displayText = formatNumber(energyWh / 1000.0)
                displayUnit = "kWh"
            } else {
                displayText = formatNumber(energyWh)
                displayUnit = "Wh"
            }
            break
        }
    }

    // При изменении любого параметра обновляем отображение (если режим соответствует)
    onCurrentValueChanged: { if (displayMode === 0) updateDisplay() }
    onVoltageValueChanged: { if (displayMode === 1) updateDisplay() }
    onPowerValueChanged:   { if (displayMode === 2) updateDisplay() }
    onEnergyWhChanged:     { if (displayMode === 3) updateDisplay() }

    // Фоновый прямоугольник (стиль как в оригинале)
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: 10
        color: "#2c3e50"
        border.color: "#34495e"
        border.width: 2
    }

    // Основной контент
    Column {
        anchors.centerIn: parent
        spacing: 8

        // Отображаемое значение
        Text {
            id: valueText
            anchors.horizontalCenter: parent.horizontalCenter
            text: displayText
            color: "white"
            font.pixelSize: 36
            font.bold: true
            font.family: "monospace"
        }

        // Единицы измерения
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: displayUnit
            color: "#bdc3c7"
            font.pixelSize: 16
            font.bold: true
        }

        // Кнопка переключения режима
        Rectangle {
            id: switchButton
            width: 40
            height: 40
            radius: 20
            color: mouseArea.pressed ? "#2980b9" : "#3498db"
            anchors.horizontalCenter: parent.horizontalCenter

            Text {
                anchors.centerIn: parent
                text: "↻"
                color: "white"
                font.pixelSize: 24
                font.bold: true
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                onClicked: {
                    displayMode = (displayMode + 1) % 4
                }
            }
        }
    }

    // Метка (опционально)
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        text: {
            switch (displayMode) {
                case 0: return "Ток"
                case 1: return "Напряжение"
                case 2: return "Мощность"
                case 3: return "Энергия"
            }
        }
        color: "#ecf0f1"
        font.pixelSize: 10
        font.bold: true
    }

    Component.onCompleted: {
        updateDisplay()
        console.log("ElectricalMonitor ready")
    }
}


