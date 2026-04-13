
import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root
    width: 120
    height: 400
    color: "transparent"

    // Свойства компонента - получаем из контекста
    property real value: 0
    property real minValue: typeof gaugeMinValue !== 'undefined' ? gaugeMinValue : 0
    property real maxValue: typeof gaugeMaxValue !== 'undefined' ? gaugeMaxValue : 10000
    property string label: typeof gaugeLabel !== 'undefined' ? gaugeLabel : "Мощность"
    property string unit: typeof gaugeUnit !== 'undefined' ? gaugeUnit : "RPM"

    // Цвета для градиента (храним как строки)
    property string lowColorStr: "#2ecc71"    // Зелёный
    property string midColorStr: "#f1c40f"    // Жёлтый
    property string highColorStr: "#e74c3c"   // Красный

    // Цвета как color объекты для использования
    property color lowColor: lowColorStr
    property color midColor: midColorStr
    property color highColor: highColorStr

    // Функция для вычисления цвета в зависимости от значения
    function getColor(value) {
        var ratio = (value - minValue) / (maxValue - minValue);
        ratio = Math.min(1, Math.max(0, ratio));

        // Парсим строки цветов
        function parseColor(hexColor) {
            return {
                r: parseInt(hexColor.substring(1,3), 16),
                g: parseInt(hexColor.substring(3,5), 16),
                b: parseInt(hexColor.substring(5,7), 16)
            };
        }

        var low = parseColor(lowColorStr);
        var mid = parseColor(midColorStr);
        var high = parseColor(highColorStr);

        var r, g, b;

        if (ratio <= 0.5) {
            // От зелёного к жёлтому
            var t = ratio * 2;
            r = Math.round(low.r + (mid.r - low.r) * t);
            g = Math.round(low.g + (mid.g - low.g) * t);
            b = Math.round(low.b + (mid.b - low.b) * t);
        } else {
            // От жёлтого к красному
            var t = (ratio - 0.5) * 2;
            r = Math.round(mid.r + (high.r - mid.r) * t);
            g = Math.round(mid.g + (high.g - mid.g) * t);
            b = Math.round(mid.b + (high.b - mid.b) * t);
        }

        return Qt.rgba(r/255, g/255, b/255, 1.0);
    }

    // Фон
    Rectangle {
        id: background
        anchors.fill: parent
        radius: 10
        color: "#2c3e50"
        border.color: "#34495e"
        border.width: 2
    }

    // Заполненная часть (однородная заливка с динамическим цветом)
    Rectangle {
        id: fillBar
        width: parent.width - 20
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        height: (parent.height - 50) * Math.min(1, Math.max(0, (value - minValue) / (maxValue - minValue)))
        radius: 6
        color: getColor(value)

        Behavior on color {
            ColorAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    // Текст значения
    Text {
        id: valueText
        anchors.bottom: fillBar.top
        anchors.bottomMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        text: Math.round(value)
        color: "white"
        font.pixelSize: 14
        font.bold: true
    }

    // Единица измерения
    Text {
        anchors.top: valueText.bottom
        anchors.topMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        text: unit
        color: "#bdc3c7"
        font.pixelSize: 8
    }

    // Название
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        text: label
        color: "#ecf0f1"
        font.pixelSize: 10
        font.bold: true
    }

    // Деления (риски)
    Repeater {
        model: 5
        Rectangle {
            x: parent.width - 10
            y: (parent.height - 40) * (1 - index / 4) + 20
            width: 10
            height: 2
            color: "#7f8c8d"

            Text {
                x: -25
                y: -5
                text: Math.round(root.minValue + (root.maxValue - root.minValue) * (index / 4))
                color: "#95a5a6"
                font.pixelSize: 8
            }
        }
    }
}
