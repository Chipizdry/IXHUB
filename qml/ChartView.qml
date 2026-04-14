


import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id: chartContainer
    width: 1020
    height: 600
    color: "#1a1a2e"
    radius: 10

    property string chartTitle: "Мультипараметрический график"
    property string xAxisLabel: "Время"
    property int maxDataPoints: 150

    // Структура для хранения параметров с разными шкалами
    property variant parameters: ({})
    property variant parameterConfig: ({
        "Обороты": { color: "#00cc00", unit: "об/мин", minY: 0, maxY: 8000, visible: true, yAxis: "left" },
        "Ток": { color: "#3498db", unit: "А", minY: 0, maxY: 100, visible: true, yAxis: "left" },
        "Ёмкость": { color: "#f39c12", unit: "Ач", minY: 0, maxY: 100, visible: false, yAxis: "right" },
        "Температура": { color: "#e74c3c", unit: "°C", minY: -20, maxY: 150, visible: false, yAxis: "right" },
        "Напряжение": { color: "#9b59b6", unit: "В", minY: 0, maxY: 500, visible: false, yAxis: "right" }
    })

    // Диапазоны для левой и правой шкалы
    property real leftMinY: 0
    property real leftMaxY: 100
    property real rightMinY: 0
    property real rightMaxY: 100
    property bool autoScaleLeft: true
    property bool autoScaleRight: true

    // Общая шкала времени
    property variant timePoints: []
    property real startTime: 0

    // Функция регистрации нового параметра
    function registerParameter(name, config) {
        if (!parameters[name]) {
            parameters[name] = [];
            parameterConfig[name] = config;
            console.log("Registered new parameter:", name);
            updateYScales();
            canvas.requestPaint();
        }
    }

    // Функция добавления точки для параметра
    function addDataPoint(parameterName, value) {
        console.log("addDataPoint for", parameterName, ":", value);

        var currentTime = (new Date()).getTime() / 1000;

        // Инициализируем параметр если его нет
        if (!parameters[parameterName]) {
            var defaultConfig = {
                color: getRandomColor(),
                unit: "",
                minY: 0,
                maxY: 100,
                visible: true,
                yAxis: "left"
            };
            registerParameter(parameterName, defaultConfig);
        }

        // Инициализируем временные метки
        if (Object.keys(parameters).length === 1 && parameters[parameterName].length === 0) {
            startTime = currentTime;
        }

        var newPoint = {"x": currentTime - startTime, "y": value, "realTime": currentTime};
        var newPoints = parameters[parameterName].slice();
        newPoints.push(newPoint);

        // Ограничиваем количество точек
        if (newPoints.length > maxDataPoints) {
            newPoints = newPoints.slice(newPoints.length - maxDataPoints);
        }

        parameters[parameterName] = newPoints;

        // Обновляем шкалы Y
        if ((parameterConfig[parameterName].yAxis === "left" && autoScaleLeft) ||
            (parameterConfig[parameterName].yAxis === "right" && autoScaleRight)) {
            updateYScales();
        }

        canvas.requestPaint();
    }

    // Обновление шкал Y на основе данных
    function updateYScales() {
        if (autoScaleLeft) {
            var leftMin = Infinity;
            var leftMax = -Infinity;

            for (var name in parameters) {
                if (parameterConfig[name].visible && parameterConfig[name].yAxis === "left") {
                    for (var i = 0; i < parameters[name].length; i++) {
                        var y = parameters[name][i].y;
                        if (y < leftMin) leftMin = y;
                        if (y > leftMax) leftMax = y;
                    }
                }
            }

            if (leftMin !== Infinity && leftMax !== -Infinity) {
                var leftPadding = (leftMax - leftMin) * 0.1;
                leftMinY = Math.max(0, leftMin - leftPadding);
                leftMaxY = leftMax + leftPadding;
                if (leftMinY === leftMaxY) {
                    leftMaxY = leftMinY + 1;
                }
            }
        }

        if (autoScaleRight) {
            var rightMin = Infinity;
            var rightMax = -Infinity;

            for (var name in parameters) {
                if (parameterConfig[name].visible && parameterConfig[name].yAxis === "right") {
                    for (var i = 0; i < parameters[name].length; i++) {
                        var y = parameters[name][i].y;
                        if (y < rightMin) rightMin = y;
                        if (y > rightMax) rightMax = y;
                    }
                }
            }

            if (rightMin !== Infinity && rightMax !== -Infinity) {
                var rightPadding = (rightMax - rightMin) * 0.1;
                rightMinY = Math.max(0, rightMin - rightPadding);
                rightMaxY = rightMax + rightPadding;
                if (rightMinY === rightMaxY) {
                    rightMaxY = rightMinY + 1;
                }
            }
        }
    }

    // Очистка всех данных
    function clearChart() {
        console.log("Clearing all parameters");
        for (var name in parameters) {
            parameters[name] = [];
        }
        timePoints = [];
        startTime = 0;
        canvas.requestPaint();
    }

    // Очистка конкретного параметра
    function clearParameter(parameterName) {
        if (parameters[parameterName]) {
            parameters[parameterName] = [];
            console.log("Cleared parameter:", parameterName);
            updateYScales();
            canvas.requestPaint();
        }
    }

    // Генерация случайного цвета
    function getRandomColor() {
        var letters = "0123456789ABCDEF";
        var color = "#";
        for (var i = 0; i < 6; i++) {
            color += letters[Math.floor(Math.random() * 16)];
        }
        return color;
    }

    // Форматирование времени
    function formatTime(seconds) {
        if (seconds < 60) {
            return seconds.toFixed(1) + "с";
        } else if (seconds < 3600) {
            var minutes = Math.floor(seconds / 60);
            var secs = seconds % 60;
            return minutes + "м " + secs.toFixed(0) + "с";
        } else {
            var hours = Math.floor(seconds / 3600);
            var minutes = Math.floor((seconds % 3600) / 60);
            return hours + "ч " + minutes + "м";
        }
    }

    // Заголовок
    Text {
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        text: chartTitle
        color: "white"
        font.pixelSize: 18
        font.bold: true
    }

    // Панель управления параметрами
    Rectangle {
        id: controlPanel
        anchors.top: parent.top
        anchors.topMargin: 45
        anchors.left: parent.left
        anchors.leftMargin: 10
        width: 180
        height: 300
        color: "#0f3460"
        radius: 5
        border.color: "#3498db"
        border.width: 1

        ScrollView {
            anchors.fill: parent
            anchors.margins: 5
            clip: true

            ColumnLayout {
                width: controlPanel.width - 20
                spacing: 8

                Text {
                    text: "Параметры"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignHCenter
                }

                Repeater {
                    model: Object.keys(parameterConfig)

                    ColumnLayout {
                        spacing: 3

                        RowLayout {
                            spacing: 5

                            CheckBox {
                                id: cb
                                checked: parameterConfig[modelData].visible
                                onCheckedChanged: {
                                    parameterConfig[modelData].visible = checked;
                                    if (autoScaleLeft || autoScaleRight) updateYScales();
                                    canvas.requestPaint();
                                }

                                indicator: Rectangle {
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    color: cb.checked ? parameterConfig[modelData].color : "#2c3e50"
                                    border.color: "#3498db"
                                    border.width: 1
                                    radius: 3

                                    Text {
                                        text: "✓"
                                        color: "white"
                                        font.pixelSize: 12
                                        anchors.centerIn: parent
                                        visible: cb.checked
                                    }
                                }
                            }

                            Rectangle {
                                width: 16
                                height: 16
                                radius: 2
                                color: parameterConfig[modelData].color
                            }

                            Text {
                                text: modelData
                                color: "white"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Text {
                                text: parameterConfig[modelData].unit
                                color: "#a5a5a5"
                                font.pixelSize: 8
                            }
                        }

                        RowLayout {
                            spacing: 5
                            visible: parameterConfig[modelData].visible

                            Text {
                                text: "Y:"
                                color: "#a5a5a5"
                                font.pixelSize: 8
                            }

                            Text {
                                text: parameterConfig[modelData].yAxis === "left" ? "←" : "→"
                                color: parameterConfig[modelData].yAxis === "left" ? "#00cc00" : "#f39c12"
                                font.pixelSize: 10
                            }

                            Button {
                                text: "Очистить"
                                height: 20
                                width: 60
                                background: Rectangle {
                                    color: parent.pressed ? "#c0392b" : "#e74c3c"
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 8
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                onClicked: {
                                    clearParameter(modelData);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Область графика
    Rectangle {
        id: plotArea
        anchors.top: parent.top
        anchors.topMargin: 50
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        anchors.left: parent.left
        anchors.leftMargin: 70
        anchors.right: parent.right
        anchors.rightMargin: 70
        color: "#16213e"
        border.color: "#3498db"
        border.width: 2
        radius: 5

        // Canvas для рисования
        Canvas {
            id: canvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.clearRect(0, 0, width, height);

                // Собираем все временные метки
                var allTimes = [];
                for (var name in parameters) {
                    for (var i = 0; i < parameters[name].length; i++) {
                        allTimes.push(parameters[name][i].x);
                    }
                }

                if (allTimes.length < 2) {
                    drawGrid(ctx, null, null);
                    return;
                }

                var minX = Math.min.apply(null, allTimes);
                var maxX = Math.max.apply(null, allTimes);
                var xRange = maxX - minX;

                if (xRange <= 0) return;

                // Рисуем сетку
                drawGrid(ctx, minX, maxX);

                // Рисуем все видимые параметры
                for (var paramName in parameters) {
                    if (parameterConfig[paramName].visible && parameters[paramName].length >= 2) {
                        var yMin, yMax;
                        if (parameterConfig[paramName].yAxis === "left") {
                            yMin = leftMinY;
                            yMax = leftMaxY;
                        } else {
                            yMin = rightMinY;
                            yMax = rightMaxY;
                        }
                        drawParameter(ctx, parameters[paramName], parameterConfig[paramName].color, minX, maxX, xRange, yMin, yMax);
                    }
                }
            }

            function drawParameter(ctx, data, color, minX, maxX, xRange, yMin, yMax) {
                if (data.length < 2 || yMax === yMin) return;

                // Рисуем линию
                ctx.beginPath();
                ctx.strokeStyle = color;
                ctx.lineWidth = 2;

                var firstPoint = true;
                for (var i = 0; i < data.length; i++) {
                    var x = ((data[i].x - minX) / xRange) * width;
                    var y = height - ((data[i].y - yMin) / (yMax - yMin)) * height;
                    y = Math.max(0, Math.min(height, y));

                    if (firstPoint) {
                        ctx.moveTo(x, y);
                        firstPoint = false;
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                ctx.stroke();

                // Рисуем точки
                ctx.fillStyle = color;
                for (var i = 0; i < data.length; i++) {
                    var x = ((data[i].x - minX) / xRange) * width;
                    var y = height - ((data[i].y - yMin) / (yMax - yMin)) * height;
                    y = Math.max(0, Math.min(height, y));

                    ctx.beginPath();
                    ctx.arc(x, y, 2, 0, Math.PI * 2);
                    ctx.fill();
                }
            }

            function drawGrid(ctx, minX, maxX) {
                // Вертикальные линии времени
                ctx.strokeStyle = "#0f3460";
                ctx.lineWidth = 1;
                ctx.fillStyle = "#a5a5a5";
                ctx.font = "10px Arial";

                var vLines = 10;
                for (var i = 0; i <= vLines; i++) {
                    var xPos = (i / vLines) * width;
                    ctx.beginPath();
                    ctx.moveTo(xPos, 0);
                    ctx.lineTo(xPos, height);
                    ctx.stroke();

                    if (minX !== null && maxX !== null) {
                        var timeValue = minX + (i / vLines) * (maxX - minX);
                        var timeText = formatTime(timeValue);
                        ctx.fillText(timeText, xPos - 20, height - 5);
                    }
                }

                // Горизонтальные линии левой шкалы
                var hLines = 5;
                for (var i = 0; i <= hLines; i++) {
                    var yPos = (i / hLines) * height;
                    ctx.beginPath();
                    ctx.moveTo(0, yPos);
                    ctx.lineTo(width, yPos);
                    ctx.stroke();
                }
            }
        }
    }

    // Левая шкала Y
    Column {
        x: 55
        y: plotArea.y
        spacing: (plotArea.height - 20) / 5

        Repeater {
            model: 6
            Text {
                text: Math.round(leftMaxY - (index * (leftMaxY - leftMinY) / 5))
                color: "#00cc00"
                font.pixelSize: 10
                horizontalAlignment: Text.AlignRight
                width: 40
            }
        }
    }

    // Правая шкала Y
    Column {
        x: plotArea.x + plotArea.width + 5
        y: plotArea.y
        spacing: (plotArea.height - 20) / 5

        Repeater {
            model: 6
            Text {
                text: Math.round(rightMaxY - (index * (rightMaxY - rightMinY) / 5))
                color: "#f39c12"
                font.pixelSize: 10
            }
        }
    }

    // Подписи осей
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        text: xAxisLabel
        color: "#bdc3c7"
        font.pixelSize: 12
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 15
        anchors.verticalCenter: parent.verticalCenter
        text: "Левая шкала"
        color: "#00cc00"
        font.pixelSize: 10
        rotation: -90
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 15
        anchors.verticalCenter: parent.verticalCenter
        text: "Правая шкала"
        color: "#f39c12"
        font.pixelSize: 10
        rotation: 90
    }

    // Кнопки управления
    Button {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 10
        width: 80
        height: 30
        text: "Очистить"

        background: Rectangle {
            color: parent.pressed ? "#c0392b" : "#e74c3c"
            radius: 5
        }

        contentItem: Text {
            text: parent.text
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        onClicked: {
            clearChart();
        }
    }

    Button {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 100
        width: 80
        height: 30
        text: "Автошкала"

        background: Rectangle {
            color: "#27ae60"
            radius: 5
        }

        contentItem: Text {
            text: parent.text
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        onClicked: {
            autoScaleLeft = !autoScaleLeft;
            autoScaleRight = !autoScaleRight;
            updateYScales();
            canvas.requestPaint();
        }
    }

    Component.onCompleted: {
        console.log("Multi-parameter chart loaded");

        // Инициализируем тестовые данные для демонстрации
        for (var i = 0; i < 50; i++) {
            addDataPoint("Обороты", 3000 + Math.random() * 5000);
            addDataPoint("Ток", 20 + Math.random() * 80);
            addDataPoint("Температура", 30 + Math.random() * 120);
        }
    }
}


