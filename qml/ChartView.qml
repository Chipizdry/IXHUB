


import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.0

Rectangle {
    id: chartContainer
    width: 1100
    height: 750
    color: "#1a1a2e"
    radius: 15

    property string chartTitle: "График"
    property string xAxisLabel: "Время"
    property string yAxisLabel: "Значение"
    property real maxYValue: 100
    property real minYValue: 0
    property variant dataPoints: []  // Массив точек {x, y}

    // Внутренние отступы
    property real leftMargin: 50
    property real rightMargin: 30
    property real topMargin: 50
    property real bottomMargin: 40

    // Заголовок
    Text {
        id: titleText
        anchors.top: parent.top
        anchors.topMargin: 15
        anchors.horizontalCenter: parent.horizontalCenter
        text: chartTitle
        color: "#ecf0f1"
        font.pixelSize: 20
        font.bold: true
    }

    // Область для графика
    Rectangle {
        id: plotArea
        x: leftMargin
        y: topMargin
        width: parent.width - leftMargin - rightMargin
        height: parent.height - topMargin - bottomMargin
        color: "#16213e"
        border.color: "#0f3460"
        border.width: 2
        radius: 5

        // Координатная сетка
        Canvas {
            id: gridCanvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.clearRect(0, 0, width, height);

                ctx.strokeStyle = "#0f3460";
                ctx.lineWidth = 1;

                // Вертикальные линии (5 линий)
                var vStep = width / 10;
                for (var i = 1; i <= 9; i++) {
                    ctx.beginPath();
                    ctx.moveTo(i * vStep, 0);
                    ctx.lineTo(i * vStep, height);
                    ctx.stroke();
                }

                // Горизонтальные линии (5 линий)
                var hStep = height / 10;
                for (var i = 1; i <= 9; i++) {
                    ctx.beginPath();
                    ctx.moveTo(0, i * hStep);
                    ctx.lineTo(width, i * hStep);
                    ctx.stroke();
                }

                // Рамка
                ctx.strokeStyle = "#3498db";
                ctx.lineWidth = 2;
                ctx.strokeRect(0, 0, width, height);
            }

            Connections {
                target: chartContainer
                function onWidthChanged() { gridCanvas.requestPaint(); }
                function onHeightChanged() { gridCanvas.requestPaint(); }
            }
        }

        // График (линия)
        Canvas {
            id: chartCanvas
            anchors.fill: parent
            antialiasing: true

            function drawChart() {
                if (dataPoints.length < 2) return;

                var ctx = getContext("2d");
                ctx.reset();
                ctx.clearRect(0, 0, width, height);

                // Рисуем линию графика
                ctx.beginPath();
                ctx.strokeStyle = "#2ecc71";
                ctx.lineWidth = 3;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";

                var xStep = width / (dataPoints.length - 1);

                for (var i = 0; i < dataPoints.length; i++) {
                    var x = i * xStep;
                    var y = height - ((dataPoints[i].y - minYValue) / (maxYValue - minYValue)) * height;
                    y = Math.max(0, Math.min(height, y));

                    if (i === 0) {
                        ctx.moveTo(x, y);
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                ctx.stroke();

                // Рисуем точки на графике
                ctx.fillStyle = "#e74c3c";
                for (var i = 0; i < dataPoints.length; i++) {
                    var x = i * xStep;
                    var y = height - ((dataPoints[i].y - minYValue) / (maxYValue - minYValue)) * height;
                    y = Math.max(0, Math.min(height, y));

                    ctx.beginPath();
                    ctx.arc(x, y, 5, 0, Math.PI * 2);
                    ctx.fill();
                }
            }

            onPaint: drawChart()

            Connections {
                target: chartContainer
                function onDataPointsChanged() { chartCanvas.requestPaint(); }
            }
        }

        // Заливка под графиком
        Canvas {
            id: fillCanvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                if (dataPoints.length < 2) return;

                var ctx = getContext("2d");
                ctx.reset();
                ctx.clearRect(0, 0, width, height);

                ctx.beginPath();
                var xStep = width / (dataPoints.length - 1);

                for (var i = 0; i < dataPoints.length; i++) {
                    var x = i * xStep;
                    var y = height - ((dataPoints[i].y - minYValue) / (maxYValue - minYValue)) * height;
                    y = Math.max(0, Math.min(height, y));

                    if (i === 0) {
                        ctx.moveTo(x, y);
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                ctx.lineTo(width, height);
                ctx.lineTo(0, height);
                ctx.closePath();

                ctx.fillStyle = Qt.rgba(46, 204, 113, 0.2);
                ctx.fill();
            }

            Connections {
                target: chartContainer
                function onDataPointsChanged() { fillCanvas.requestPaint(); }
            }
        }
    }

    // Подписи осей
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        text: xAxisLabel
        color: "#bdc3c7"
        font.pixelSize: 14
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 15
        anchors.verticalCenter: parent.verticalCenter
        text: yAxisLabel
        color: "#bdc3c7"
        font.pixelSize: 14
        rotation: -90
    }

    // Значения по оси Y
    Column {
        x: 10
        y: plotArea.y
        spacing: (plotArea.height - 20) / 5

        Repeater {
            model: 6
            Text {
                text: Math.round(maxYValue - (index * (maxYValue - minYValue) / 5))
                color: "#95a5a6"
                font.pixelSize: 11
                width: 45
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    // Значения по оси X
    Row {
        x: plotArea.x
        y: plotArea.y + plotArea.height + 5
        spacing: (plotArea.width) / 5

        Repeater {
            model: 6
            Text {
                text: Math.round(index * (dataPoints.length - 1) / 5)
                color: "#95a5a6"
                font.pixelSize: 11
                width: 50
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}


