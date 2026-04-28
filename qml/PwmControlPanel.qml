


import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root
    width: 420
    height: 210  // Увеличена высота для размещения кнопки
    color: "transparent"

    // Свойства для частоты ШИМ (TIM1->ARR)
    property real targetFrequency: 0
    property real currentFrequency: 0
    property real minFrequency: 1
    property real maxFrequency: 60
    property real frequencyStep: 0.1

    // Свойства для скважности (заполнения)
    property real targetDuty: 0
    property real currentDuty: 0
    property real minDuty: 0
    property real maxDuty: 100
    property real dutyStep: 0.2

    // Свойство режима работы
    property bool isGenMode: false      // false = Мотор, true = Генератор

    // Режимы отображения
    property bool isFreqTargetMode: false
    property bool isDutyTargetMode: false

    // Сигналы
    signal frequencyTargetChanged(real value)
    signal dutyTargetChanged(real value)
    signal stopRequested()
    signal modeToggleRequested(bool genMode)  // Новый сигнал переключения режима

    function freqMatch() {
        return Math.abs(targetFrequency - currentFrequency) < 0.1
    }

    function dutyMatch() {
        return Math.abs(targetDuty - currentDuty) < 0.5
    }

    function updateFreqMode() {
        isFreqTargetMode = !freqMatch()
    }

    function updateDutyMode() {
        isDutyTargetMode = !dutyMatch()
    }

    // Функция для внешнего обновления режима
    function setGenMode(genMode) {
        isGenMode = genMode
    }

    onTargetFrequencyChanged: updateFreqMode()
    onCurrentFrequencyChanged: updateFreqMode()
    onTargetDutyChanged: updateDutyMode()
    onCurrentDutyChanged: updateDutyMode()

    Component.onCompleted: {
        console.log("PwmControlPanel loaded")
    }

    // Фон
    Rectangle {
        id: background
        anchors.fill: parent
        anchors.margins: 2
        radius: 10
        color: "#2c3e50"
        border.color: "#34495e"
        border.width: 2
    }

    // Основной контейнер
    Item {
        anchors.fill: parent
        anchors.margins: 10

        Column {
            anchors.fill: parent
            spacing: 8

            // ========== СТРОКА УПРАВЛЕНИЯ ЧАСТОТОЙ ==========
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Rectangle {
                    width: 50; height: 38; radius: 6
                    color: freqMinus.pressed ? "#e74c3c" : "#c0392b"
                    border.color: "#e74c3c"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "-"; color: "white"
                        font.pixelSize: 24; font.bold: true
                    }
                    MouseArea {
                        id: freqMinus; anchors.fill: parent
                        onClicked: {
                            var newVal = targetFrequency - frequencyStep
                            if (newVal >= minFrequency) {
                                targetFrequency = newVal
                                root.frequencyTargetChanged(targetFrequency)
                            }
                        }
                        Timer {
                            interval: 200; running: freqMinus.pressed; repeat: true
                            onTriggered: {
                                var newVal = targetFrequency - frequencyStep
                                if (newVal >= minFrequency) {
                                    targetFrequency = newVal
                                    root.frequencyTargetChanged(targetFrequency)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: 120; height: 38; radius: 6
                    color: isFreqTargetMode ? "#f39c12" : "#34495e"
                    border.color: isFreqTargetMode ? "#f1c40f" : "#2c3e50"
                    border.width: 2
                    SequentialAnimation on color {
                        running: isFreqTargetMode; loops: Animation.Infinite
                        ColorAnimation { from: "#f39c12"; to: "#e67e22"; duration: 500 }
                        ColorAnimation { from: "#e67e22"; to: "#f39c12"; duration: 500 }
                    }
                    Column {
                        anchors.centerIn: parent; spacing: 2
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: isFreqTargetMode ? targetFrequency.toFixed(1) : currentFrequency.toFixed(1)
                            color: "white"; font.pixelSize: 18; font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "кГц"; color: "#bdc3c7"; font.pixelSize: 10
                        }
                    }
                    Rectangle {
                        visible: freqMatch() && currentFrequency > 0
                        width: 8; height: 8; radius: 4; color: "#2ecc71"
                        anchors.left: parent.left; anchors.top: parent.top
                        anchors.leftMargin: -2; anchors.topMargin: -2
                    }
                }

                Rectangle {
                    width: 50; height: 38; radius: 6
                    color: freqPlus.pressed ? "#27ae60" : "#2ecc71"
                    border.color: "#27ae60"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "+"; color: "white"
                        font.pixelSize: 24; font.bold: true
                    }
                    MouseArea {
                        id: freqPlus; anchors.fill: parent
                        onClicked: {
                            var newVal = targetFrequency + frequencyStep
                            if (newVal <= maxFrequency) {
                                targetFrequency = newVal
                                root.frequencyTargetChanged(targetFrequency)
                            }
                        }
                        Timer {
                            interval: 200; running: freqPlus.pressed; repeat: true
                            onTriggered: {
                                var newVal = targetFrequency + frequencyStep
                                if (newVal <= maxFrequency) {
                                    targetFrequency = newVal
                                    root.frequencyTargetChanged(targetFrequency)
                                }
                            }
                        }
                    }
                }
            }

            // ========== СТРОКА УПРАВЛЕНИЯ СКВАЖНОСТЬЮ ==========
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Rectangle {
                    width: 50; height: 38; radius: 6
                    color: dutyMinus.pressed ? "#e74c3c" : "#c0392b"
                    border.color: "#e74c3c"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "-"; color: "white"
                        font.pixelSize: 24; font.bold: true
                    }
                    MouseArea {
                        id: dutyMinus; anchors.fill: parent
                        onClicked: {
                            var newVal = targetDuty - dutyStep
                            if (newVal >= minDuty) {
                                targetDuty = newVal
                                root.dutyTargetChanged(targetDuty)
                            }
                        }
                        Timer {
                            interval: 200; running: dutyMinus.pressed; repeat: true
                            onTriggered: {
                                var newVal = targetDuty - dutyStep
                                if (newVal >= minDuty) {
                                    targetDuty = newVal
                                    root.dutyTargetChanged(targetDuty)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: 120; height: 38; radius: 6
                    color: isDutyTargetMode ? "#f39c12" : "#34495e"
                    border.color: isDutyTargetMode ? "#f1c40f" : "#2c3e50"
                    border.width: 2
                    SequentialAnimation on color {
                        running: isDutyTargetMode; loops: Animation.Infinite
                        ColorAnimation { from: "#f39c12"; to: "#e67e22"; duration: 500 }
                        ColorAnimation { from: "#e67e22"; to: "#f39c12"; duration: 500 }
                    }
                    Column {
                        anchors.centerIn: parent; spacing: 2
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: isDutyTargetMode ? targetDuty.toFixed(1) : currentDuty.toFixed(1)
                            color: "white"; font.pixelSize: 18; font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: isGenMode ? "% BOOST" : "%"
                            color: isGenMode ? "#2ecc71" : "#bdc3c7"
                            font.pixelSize: 10
                        }
                    }
                    Rectangle {
                        visible: dutyMatch() && currentDuty > 0
                        width: 8; height: 8; radius: 4; color: "#2ecc71"
                        anchors.left: parent.left; anchors.top: parent.top
                        anchors.leftMargin: -2; anchors.topMargin: -2
                    }
                }

                Rectangle {
                    width: 50; height: 38; radius: 6
                    color: dutyPlus.pressed ? "#27ae60" : "#2ecc71"
                    border.color: "#27ae60"; border.width: 2
                    Text {
                        anchors.centerIn: parent
                        text: "+"; color: "white"
                        font.pixelSize: 24; font.bold: true
                    }
                    MouseArea {
                        id: dutyPlus; anchors.fill: parent
                        onClicked: {
                            var newVal = targetDuty + dutyStep
                            if (newVal <= maxDuty) {
                                targetDuty = newVal
                                root.dutyTargetChanged(targetDuty)
                            }
                        }
                        Timer {
                            interval: 200; running: dutyPlus.pressed; repeat: true
                            onTriggered: {
                                var newVal = targetDuty + dutyStep
                                if (newVal <= maxDuty) {
                                    targetDuty = newVal
                                    root.dutyTargetChanged(targetDuty)
                                }
                            }
                        }
                    }
                }
            }

            // ========== КНОПКА РЕЖИМА (над СТОП) ==========
            Rectangle {
                width: parent.width - 30
                height: 36; radius: 6
                color: modeButton.pressed ? (isGenMode ? "#219a52" : "#1565c0")
                                          : (isGenMode ? "#27ae60" : "#2980b9")
                border.color: isGenMode ? "#1e8449" : "#1a5276"
                border.width: 2
                anchors.horizontalCenter: parent.horizontalCenter

                Text {
                    anchors.centerIn: parent
                    text: isGenMode ? "РЕЖИМ: ГЕНЕРАТОР" : "РЕЖИМ: МОТОР"
                    color: "white"
                    font.pixelSize: 14
                    font.bold: true
                }

                MouseArea {
                    id: modeButton
                    anchors.fill: parent
                    onClicked: {
                        isGenMode = !isGenMode
                        root.modeToggleRequested(isGenMode)
                        console.log("Mode toggled to:", isGenMode ? "GENERATOR" : "MOTOR")
                    }
                }
            }

            // ========== КНОПКА СТОП НА ВСЮ ШИРИНУ ==========
            Rectangle {
                width: parent.width - 30
                height: 36; radius: 6
                color: stopButton.pressed ? "#c0392b" : "#e74c3c"
                border.color: "#c0392b"; border.width: 2
                anchors.horizontalCenter: parent.horizontalCenter
                Text {
                    anchors.centerIn: parent
                    text: "СТОП"; color: "white"
                    font.pixelSize: 16; font.bold: true
                }
                MouseArea {
                    id: stopButton; anchors.fill: parent
                    onClicked: {
                        targetDuty = 0
                        root.dutyTargetChanged(0)
                        root.stopRequested()
                        console.log("STOP pressed - duty set to 0")
                    }
                }
            }
        }
    }
}


