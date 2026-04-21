


import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root
    width: 180
    height: 70
    color: "transparent"

    // Свойства компонента
    property real targetValue: 0          // Целевое значение (устанавливается кнопками)
    property real currentValue: 0         // Текущее значение (получаемое с платы)
    property real minValue: 0
    property real maxValue: 2000
    property real step: 5                 // Шаг изменения
    property string label: "Значение"
    property string unit: ""
    property bool isTargetMode: false     // Режим отображения целевого значения

    // Сигнал при изменении целевого значения
    signal pwmTargetChanged(real value)

    // Проверка, совпадают ли значения
    function valuesMatch() {
        return Math.abs(targetValue - currentValue) < 0.1
    }

    // Функция для обновления режима отображения
    function updateDisplayMode() {
        isTargetMode = (Math.abs(targetValue - currentValue) > 0.1)
    }

    // Отслеживаем изменения значений
    onTargetValueChanged: {
        updateDisplayMode()
        root.pwmTargetChanged(targetValue)
    }

    onCurrentValueChanged: {
        updateDisplayMode()
    }

    Component.onCompleted: {
        console.log("ValueControl loaded - Label:", label, "Unit:", unit)
    }

    // Фон - в стиле VerticalGauge
    Rectangle {
        id: background
        anchors.fill: parent
        anchors.margins: 2                // Уменьшенная рамка
        radius: 8                         // Чуть меньший радиус
        color: "#2c3e50"
        border.color: "#34495e"
        border.width: 2
    }

    // Основной контейнер для содержимого
    Item {
        anchors.fill: parent
        anchors.margins: 8                // Отступы для содержимого

        Row {
            anchors.centerIn: parent
            spacing: 8

            // Кнопка уменьшения
            Rectangle {
                width: 45
                height: 45
                radius: 6
                color: minusButton.pressed ? "#e74c3c" : "#c0392b"
                border.color: "#e74c3c"
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "-"
                    color: "white"
                    font.pixelSize: 28
                    font.bold: true
                }

                MouseArea {
                    id: minusButton
                    anchors.fill: parent
                    onClicked: {
                        var newValue = targetValue - step
                        if (newValue >= minValue) {
                            targetValue = newValue
                        }
                    }

                    // Автоповтор при удержании
                    Timer {
                        interval: 200
                        running: minusButton.pressed
                        repeat: true
                        onTriggered: {
                            var newValue = targetValue - step
                            if (newValue >= minValue) {
                                targetValue = newValue
                            }
                        }
                    }
                }
            }

            // Поле отображения значения
            Rectangle {
                width: 85
                height: 45
                radius: 6
                color: isTargetMode ? "#f39c12" : "#34495e"
                border.color: isTargetMode ? "#f1c40f" : "#2c3e50"
                border.width: 2

                // Анимация мигания для целевого режима
                SequentialAnimation on color {
                    running: isTargetMode
                    loops: Animation.Infinite
                    ColorAnimation { from: "#f39c12"; to: "#e67e22"; duration: 500 }
                    ColorAnimation { from: "#e67e22"; to: "#f39c12"; duration: 500 }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: isTargetMode ? targetValue.toFixed(0) : currentValue.toFixed(0)
                        color: "white"
                        font.pixelSize: 20
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: unit
                        color: "#bdc3c7"
                        font.pixelSize: 9
                        visible: unit !== ""
                    }
                }
            }

            // Кнопка увеличения
            Rectangle {
                width: 45
                height: 45
                radius: 6
                color: plusButton.pressed ? "#27ae60" : "#2ecc71"
                border.color: "#27ae60"
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: "white"
                    font.pixelSize: 28
                    font.bold: true
                }

                MouseArea {
                    id: plusButton
                    anchors.fill: parent
                    onClicked: {
                        var newValue = targetValue + step
                        if (newValue <= maxValue) {
                            targetValue = newValue
                        }
                    }

                    // Автоповтор при удержании
                    Timer {
                        interval: 200
                        running: plusButton.pressed
                        repeat: true
                        onTriggered: {
                            var newValue = targetValue + step
                            if (newValue <= maxValue) {
                                targetValue = newValue
                            }
                        }
                    }
                }
            }
        }

        // Метка - в стиле VerticalGauge
        Text {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -2
            anchors.horizontalCenter: parent.horizontalCenter
            text: label
            color: "#ecf0f1"
            font.pixelSize: 9
            font.bold: true
        }

        // Индикатор совпадения значений
        Rectangle {
            visible: valuesMatch() && currentValue > 0
            width: 8
            height: 8
            radius: 4
            color: "#2ecc71"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: -2
            anchors.topMargin: -2

            SequentialAnimation {
                running: valuesMatch() && currentValue > 0
                loops: 3
                PropertyAnimation { target: parent; property: "scale"; from: 1.0; to: 1.5; duration: 300 }
                PropertyAnimation { target: parent; property: "scale"; from: 1.5; to: 1.0; duration: 300 }
            }
        }
    }
}


