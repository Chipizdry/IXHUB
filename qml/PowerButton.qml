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
            anchors.centerIn: parent
            width: parent.width * 0.6
            height: parent.height * 0.6

            // Вертикальная линия (черта) - поднята вверх
            Rectangle {
                id: verticalLine
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.15
                width: parent.width * 0.06
                height: parent.height * 0.45
                radius: width / 2
                color: powerButton.isOn ? powerButton.colorOn : powerButton.colorOff

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }

                // Анимация масштаба при переключении
                scale: powerButton.isOn ? 0.9 : 1
                Behavior on scale {
                    NumberAnimation { duration: 150; easing.type: Easing.OutBack }
                }
            }

            // Дуга (разрезанный круг)
            Canvas {
                id: arcCanvas
                anchors.fill: parent
                antialiasing: true

                // Явно указываем зависимость от isOn для перерисовки
                property bool needsRedraw: powerButton.isOn
                onNeedsRedrawChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);

                    var centerX = width / 2;
                    var centerY = height / 2;
                    var radius = width / 2;

                    ctx.save();
                    // Цвет дуги меняется в зависимости от isOn
                    ctx.strokeStyle = powerButton.isOn ? powerButton.colorOn : powerButton.colorOff;
                    ctx.lineWidth = radius * 0.15;
                    ctx.lineCap = "round";

                    // Рисуем дугу окружности (незамкнутый круг)
                    var startAngle = -Math.PI / 2 + 0.4;
                    var endAngle = Math.PI * 1.5 - 0.4;

                    ctx.beginPath();
                    ctx.arc(centerX, centerY, radius * 0.7, startAngle, endAngle);
                    ctx.stroke();

                    ctx.restore();
                }

                // Дополнительный вызов перерисовки при изменении isOn
                Connections {
                    target: powerButton
                    function onIsOnChanged() {
                        arcCanvas.requestPaint();
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                powerButton.isOn = !powerButton.isOn;
                powerButton.toggled(powerButton.isOn);
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


