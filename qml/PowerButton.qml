


import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    id: powerButton
    width: 60
    height: 60

    property bool isOn: false
    property color colorOn: "#2ecc71"
    property color colorOff: "#e74c3c"

    signal toggled(bool state)

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: powerButton.isOn ? powerButton.colorOn : powerButton.colorOff
        opacity: 0.15
        scale: 1.1

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }

    Rectangle {
        id: mainCircle
        anchors.fill: parent
        radius: width / 2
        color: "white"
        border.width: 2
        border.color: powerButton.isOn ? powerButton.colorOn : powerButton.colorOff

        Behavior on border.color {
            ColorAnimation { duration: 200 }
        }

        // Символ питания
        Item {
            id: symbolContainer
            anchors.centerIn: parent
            width: parent.width * 0.6
            height: parent.height * 0.6

            // Вертикальная линия (черта) - с анимацией высоты и вертикального смещения
            Rectangle {
                id: verticalLine
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width * 0.08
                height: parent.height * (powerButton.isOn ? 0.3 : 0.5)
                radius: width / 2
                color: powerButton.isOn ? powerButton.colorOn : powerButton.colorOff

                // Явное указание позиции Y с динамическим расчётом
                y: powerButton.isOn ? parent.height * 0.35 : parent.height * 0.07

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }

                Behavior on height {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.InOutQuad
                    }
                }

                Behavior on y {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutBack
                    }
                }
            }

            // Дуга (анимированный разрезанный круг)
            Canvas {
                id: arcCanvas
                anchors.fill: parent
                antialiasing: true

                property real startAngle: powerButton.isOn ? -Math.PI / 2 - 0.3 : -Math.PI / 2 + 0.4
                property real endAngle: powerButton.isOn ? Math.PI * 1.5 + 0.3 : Math.PI * 1.5 - 0.4

                onStartAngleChanged: requestPaint()
                onEndAngleChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);

                    var centerX = width / 2;
                    var centerY = height / 2;
                    var radius = width / 2;

                    ctx.save();
                    ctx.strokeStyle = powerButton.isOn ? powerButton.colorOn : powerButton.colorOff;
                    ctx.lineWidth = radius * 0.15;
                    ctx.lineCap = "round";

                    // Рисуем дугу с анимированными углами
                    ctx.beginPath();
                    ctx.arc(centerX, centerY, radius * 0.7, startAngle, endAngle);
                    ctx.stroke();

                    ctx.restore();
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                powerButton.isOn = !powerButton.isOn;
                powerButton.toggled(powerButton.isOn);

                // Анимация нажатия
                mainCircle.scale = 0.95;
                scaleTimer.start();
            }
            onPressed: mainCircle.scale = 0.95
            onReleased: {
                if (!scaleTimer.running) mainCircle.scale = 1;
            }
        }

        Timer {
            id: scaleTimer
            interval: 100
            onTriggered: mainCircle.scale = 1
        }

        Behavior on scale {
            NumberAnimation { duration: 100 }
        }
    }

    Text {
        anchors.top: parent.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        text: powerButton.isOn ? "ВКЛ" : "ВЫКЛ"
        color: powerButton.isOn ? powerButton.colorOn : powerButton.colorOff
        font.pixelSize: 10
        font.weight: Font.Bold

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }
}


