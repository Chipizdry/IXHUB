

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

    // ЕДИНАЯ геометрия
    property real startAngle: 130
    property real angleRange: 280
    property real endAngle: startAngle + angleRange   // 410

    function valueToAngle(value) {
        return startAngle + (value / maxSpeed) * angleRange;
    }

    function degToRad(deg) {
        return deg * Math.PI / 180;
    }

   /* function normalizeAngle(deg) {
        return (deg % 360) * Math.PI / 180;
    } */
    function normalizeAngle(deg) {
        return deg * Math.PI / 180;
    }

    Rectangle {
        anchors.fill: parent
        color: "#2c3e50"
        radius: width / 2
        border.color: "#34495e"
        border.width: 5
    }

    Rectangle {
        width: parent.width - 25
        height: width
        anchors.centerIn: parent
        color: "transparent"
        radius: width / 2
        border.color: "#ecf0f1"
        border.width: 12
    }

    // ===== ШКАЛА =====
    Canvas {
        id: ticksCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);

            var cx = width / 2;
            var cy = height / 2;
            var radius = width / 2 - 30;

            ctx.save();
            ctx.strokeStyle = "#ecf0f1";
            ctx.lineWidth = 2;

            var major = 10;
            var minor = 5;

            for (var i = 0; i <= major; i++) {
                var value = i * (root.maxSpeed / major);
                var rad = degToRad(valueToAngle(value));

                var x1 = cx + (radius - 5) * Math.cos(rad);
                var y1 = cy + (radius - 5) * Math.sin(rad);
                var x2 = cx + (radius + 8) * Math.cos(rad);
                var y2 = cy + (radius + 8) * Math.sin(rad);

                ctx.beginPath();
                ctx.moveTo(x1, y1);
                ctx.lineTo(x2, y2);
                ctx.stroke();

                if (i < major) {
                    for (var j = 1; j <= minor; j++) {
                        var subVal = value + j * (root.maxSpeed / major / (minor + 1));
                        var subRad = degToRad(valueToAngle(subVal));

                        var sx1 = cx + (radius - 2) * Math.cos(subRad);
                        var sy1 = cy + (radius - 2) * Math.sin(subRad);
                        var sx2 = cx + (radius + 4) * Math.cos(subRad);
                        var sy2 = cy + (radius + 4) * Math.sin(subRad);

                        ctx.beginPath();
                        ctx.moveTo(sx1, sy1);
                        ctx.lineTo(sx2, sy2);
                        ctx.stroke();
                    }
                }
            }

            // ===== ЦВЕТОВЫЕ ЗОНЫ =====
            ctx.lineWidth = 6;

            function arc(from, to, color) {
                ctx.beginPath();
                ctx.arc(cx, cy, radius - 15,
                        normalizeAngle(valueToAngle(from)),
                        normalizeAngle(valueToAngle(to)));
                ctx.strokeStyle = color;
                ctx.stroke();
            }

            arc(0, 4000, "#2ecc71");     // зеленая
            arc(4000, 7000, "#f1c40f");  // желтая
            arc(7000, 10000, "#e74c3c"); // красная

            ctx.restore();
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // ===== ПОДПИСИ =====
    Repeater {
        model: 11
        Item {
            property real value: index * 1000
            property real rad: root.degToRad(root.valueToAngle(value))
            property real cx: root.width / 2
            property real cy: root.height / 2
            property real r: root.width / 2 - 45

            x: cx + r * Math.cos(rad) - width / 2
            y: cy + r * Math.sin(rad) - height / 2
            width: 40
            height: 20

            Text {
                anchors.centerIn: parent
                text: value === 0 ? "0" : (value / 1000) + "k"
                color: "white"
                font.pixelSize: 10
                font.bold: value % 2000 === 0
            }
        }
    }

    // ===== СТРЕЛКА =====
    Canvas {
        id: needleCanvas
        anchors.fill: parent
        antialiasing: true

        property real currentValue: root.speedValue
        onCurrentValueChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);

            var cx = width / 2;
            var cy = height / 2;

            var angle = normalizeAngle(valueToAngle(currentValue) + 90);

            ctx.save();
            ctx.translate(cx, cy);
            ctx.rotate(angle);

            var len = width / 2 - 55;
            var w = 8;

            ctx.beginPath();
            ctx.moveTo(-w / 2, 0);
            ctx.lineTo(w / 2, 0);
            ctx.lineTo(w / 2, -len);
            ctx.lineTo(-w / 2, -len);
            ctx.closePath();

            ctx.fillStyle = currentValue > 7000 ? "#ff0000" : "#e74c3c";
            ctx.fill();

            ctx.beginPath();
            ctx.moveTo(-w, -len);
            ctx.lineTo(0, -len - 10);
            ctx.lineTo(w, -len);
            ctx.fill();

            ctx.restore();

            ctx.beginPath();
            ctx.arc(cx, cy, 12, 0, Math.PI * 2);
            ctx.fillStyle = "#ecf0f1";
            ctx.fill();
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
    // ===== ЦИФРЫ =====
    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 70
        text: Math.round(root.speedValue) + "\nОб/Мин."
        color: "white"
        font.pixelSize: 22
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Скорость маховика"
        color: "#bdc3c7"
        font.pixelSize: 11
        font.bold: true
    }

    Behavior on speedValue {
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    onWidthChanged: ticksCanvas.requestPaint()
    onHeightChanged: ticksCanvas.requestPaint()
}

