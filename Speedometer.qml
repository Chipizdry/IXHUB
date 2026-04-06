import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root
    width: 350
    height: 350
    color: "#2c3e50"
    radius: width / 2

    property real speedValue: 0
    property real maxSpeed: 10000

    // Углы для шкалы: от -120° (8 часов, 0 RPM) до +120° (4 часа, 10000 RPM)
    property real startAngle: 130
    property real endAngle: -270
    property real angleRange: 280

    // Фон спидометра
    Rectangle {
        id: background
        anchors.fill: parent
        color: "#2c3e50"
        radius: width / 2
        border.color: "#34495e"
        border.width: 5
    }

    // Внутренняя дуга (шкала)
    Rectangle {
        id: scaleArc
        width: parent.width - 30
        height: width
        anchors.centerIn: parent
        color: "transparent"
        radius: width / 2
        border.color: "#ecf0f1"
        border.width: 12
    }

    // Рисуем риски (насечки)
    Canvas {
        id: ticksCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);

            var centerX = width / 2;
            var centerY = height / 2;
            var radius = width / 2 - 30;

            var startAngle = root.startAngle;
            var endAngle = root.endAngle;
            var angleRange = root.angleRange;

            var majorTickCount = 10;
            var minorTickCount = 5;

            ctx.save();
            ctx.strokeStyle = "#ecf0f1";
            ctx.lineWidth = 2;

            // Рисуем основные риски
            for (var i = 0; i <= majorTickCount; i++) {
                var value = i * (root.maxSpeed / majorTickCount);
                var angle = startAngle + (value / root.maxSpeed) * angleRange;
                var rad = angle * Math.PI / 180;

                var innerRadius = radius - 5;
                var outerRadius = radius + 8;

                var x1 = centerX + innerRadius * Math.cos(rad);
                var y1 = centerY + innerRadius * Math.sin(rad);
                var x2 = centerX + outerRadius * Math.cos(rad);
                var y2 = centerY + outerRadius * Math.sin(rad);

                ctx.beginPath();
                ctx.moveTo(x1, y1);
                ctx.lineTo(x2, y2);
                ctx.stroke();

                // Подриски между основными
                if (i < majorTickCount) {
                    for (var j = 1; j <= minorTickCount; j++) {
                        var subValue = value + j * (root.maxSpeed / majorTickCount / (minorTickCount + 1));
                        if (subValue <= root.maxSpeed) {
                            var subAngle = startAngle + (subValue / root.maxSpeed) * angleRange;
                            var subRad = subAngle * Math.PI / 180;

                            var subInnerRadius = radius - 2;
                            var subOuterRadius = radius + 4;

                            var sx1 = centerX + subInnerRadius * Math.cos(subRad);
                            var sy1 = centerY + subInnerRadius * Math.sin(subRad);
                            var sx2 = centerX + subOuterRadius * Math.cos(subRad);
                            var sy2 = centerY + subOuterRadius * Math.sin(subRad);

                            ctx.beginPath();
                            ctx.moveTo(sx1, sy1);
                            ctx.lineTo(sx2, sy2);
                            ctx.stroke();
                        }
                    }
                }
            }

            // Зеленая зона (0-4000 RPM)
            var greenEnd = 3000;
            var greenAngle = startAngle + (greenEnd / root.maxSpeed) * angleRange;
            var greenRad = greenAngle * Math.PI / 180;

            ctx.beginPath();
            ctx.arc(centerX, centerY, radius - 15, (startAngle * Math.PI / 180), greenRad);
            ctx.lineWidth = 6;
            ctx.strokeStyle = "#2ecc71";
            ctx.stroke();

            // Желтая зона (4000-7000 RPM)
            var yellowStart = 4000;
            var yellowEnd = 7000;
            var yellowStartAngle = startAngle + (yellowStart / root.maxSpeed) * angleRange;
            var yellowEndAngle = startAngle + (yellowEnd / root.maxSpeed) * angleRange;

            ctx.beginPath();
            ctx.arc(centerX, centerY, radius - 15, (yellowStartAngle * Math.PI / 180), (yellowEndAngle * Math.PI / 180));
            ctx.strokeStyle = "#f1c40f";
            ctx.stroke();

            // Красная зона (7000-10000 RPM)
            var redStart = 7000;
            var redStartAngle = startAngle + (redStart / root.maxSpeed) * angleRange;

            ctx.beginPath();
            ctx.arc(centerX, centerY, radius - 15, (redStartAngle * Math.PI / 180), (endAngle * Math.PI / 180));
            ctx.strokeStyle = "#e74c3c";
            ctx.stroke();

            ctx.restore();
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // Текстовые метки для основных рисок
    Repeater {
        model: 11
        Item {
            property real value: index * 1000
            property real angle: root.startAngle + (value / root.maxSpeed) * root.angleRange
            property real rad: angle * Math.PI / 180
            property real centerX: root.width / 2
            property real centerY: root.height / 2
            property real radius: root.width / 2 - 45

            x: centerX + (radius - 5) * Math.cos(rad) - width / 2
            y: centerY + (radius - 5) * Math.sin(rad) - height / 2
            width: 40
            height: 20

            Text {
                anchors.centerIn: parent
                text: parent.value === 0 ? "0" : (parent.value / 1000).toFixed(0) + "k"
                color: "white"
                font.pixelSize: 10
                font.bold: parent.value % 2000 === 0
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // Стрелка (исправленная)
    Image {
        id: needle
        anchors.centerIn: parent
        source: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="

        visible: false
    }

    // Рисуем стрелку вручную через Canvas
    Canvas {
        id: needleCanvas
        anchors.fill: parent
        antialiasing: true

        property real needleAngle: root.startAngle + (root.speedValue / root.maxSpeed) * root.angleRange

        onNeedleAngleChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        function degToRad(deg) {
            return deg * Math.PI / 180;
        }

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);

            var centerX = width / 2;
            var centerY = height / 2;
            var needleLength = width / 2 - 55;
            var needleWidth = 8;

            var rad = degToRad(needleAngle);

            // Рисуем стрелку
            ctx.save();
            ctx.translate(centerX, centerY);
            ctx.rotate(rad);

            // Основная линия стрелки
            ctx.beginPath();
            ctx.moveTo(-needleWidth / 2, 0);
            ctx.lineTo(needleWidth / 2, 0);
            ctx.lineTo(needleWidth / 2, -needleLength);
            ctx.lineTo(-needleWidth / 2, -needleLength);
            ctx.closePath();

            // Цвет стрелки
            if (root.speedValue > 7000) {
                ctx.fillStyle = "#ff0000";
            } else {
                ctx.fillStyle = "#e74c3c";
            }
            ctx.fill();

            // Кончик стрелки (треугольник)
            ctx.beginPath();
            ctx.moveTo(-needleWidth, -needleLength);
            ctx.lineTo(0, -needleLength - 10);
            ctx.lineTo(needleWidth, -needleLength);
            ctx.fill();

            ctx.restore();

            // Центральная точка поверх стрелки
            ctx.beginPath();
            ctx.arc(centerX, centerY, 12, 0, Math.PI * 2);
            ctx.fillStyle = "#ecf0f1";
            ctx.fill();
            ctx.strokeStyle = "#2c3e50";
            ctx.lineWidth = 2;
            ctx.stroke();

            // Маленький центр
            ctx.beginPath();
            ctx.arc(centerX, centerY, 5, 0, Math.PI * 2);
            ctx.fillStyle = "#e74c3c";
            ctx.fill();
        }

        // Принудительная перерисовка при изменении скорости
        Connections {
            target: root
            function onSpeedValueChanged() {
                needleCanvas.needleAngle = root.startAngle + (root.speedValue / root.maxSpeed) * root.angleRange;
            }
        }
    }

    // Цифровое значение скорости
    Text {
        anchors.centerIn: parent
        text: Math.round(root.speedValue) + "\nRPM"
        color: "white"
        font.pixelSize: 22
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        lineHeight: 1.2
        z: 1
    }

    // Название
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Скорость маховика"
        color: "#bdc3c7"
        font.pixelSize: 11
        font.bold: true
        z: 1
    }

    // Анимация
    Behavior on speedValue {
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutCubic
            onRunningChanged: {
                if (!running) {
                    needleCanvas.requestPaint();
                }
            }
        }
    }

    onWidthChanged: ticksCanvas.requestPaint()
    onHeightChanged: ticksCanvas.requestPaint()
}

