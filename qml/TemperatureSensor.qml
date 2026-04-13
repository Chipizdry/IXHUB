import QtQuick 2.12
import QtQuick.Controls 2.12

Rectangle {
    id: root
    width: 200
    height: 280
    color: "#2c3e50"
    radius: 15
    border.color: "#34495e"
    border.width: 2
    
    // Свойства компонента
    property int slaveId: 0
    property string sensorName: "Датчик"
    property real temperature: 0
    property string status: "ok"
    property real minTemp: -50
    property real maxTemp: 150
    
    // Функция для получения цвета в зависимости от температуры
    function getTempColor(temp) {
        if (temp > 80) return "#e74c3c"      // Красный - критично
        if (temp > 60) return "#e67e22"      // Оранжевый - высоко
        if (temp > 40) return "#f1c40f"      // Желтый - нормально выше среднего
        if (temp < 0) return "#3498db"       // Синий - холодно
        return "#2ecc71"                     // Зеленый - нормально
    }
    
    // Функция для получения иконки статуса
    function getStatusIcon() {
        if (status === "ok") return "✓"
        if (status === "no_sensor_or_error") return "⚠"
        return "✗"
    }
    
    function getStatusColor() {
        if (status === "ok") return "#2ecc71"
        return "#e74c3c"
    }
    
    // Фоновый эффект
    Rectangle {
        anchors.fill: parent
        radius: 15
        color: "#34495e"
        opacity: 0.3
    }
    
    // Заголовок с ID датчика
    Rectangle {
        id: headerRect
        width: parent.width
        height: 50
        radius: 15
        color: "#3498db"
        
        Text {
            anchors.centerIn: parent
            text: sensorName
            color: "white"
            font.pixelSize: 16
            font.bold: true
        }
        
        // Индикатор статуса в углу
        Rectangle {
            width: 20
            height: 20
            radius: 10
            color: getStatusColor()
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            
            Text {
                anchors.centerIn: parent
                text: getStatusIcon()
                color: "white"
                font.pixelSize: 12
                font.bold: true
            }
        }
    }
    
    // Круговая индикация температуры
    Item {
        id: gaugeContainer
        width: parent.width - 40
        height: width
        anchors.top: headerRect.bottom
        anchors.topMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        
        // Фоновый круг
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#1a252f"
            border.color: "#34495e"
            border.width: 3
        }
        
        // Температурная шкала (дуга)
        Canvas {
            id: tempArc
            anchors.fill: parent
            antialiasing: true
            
            property real currentTemp: root.temperature
            
            onCurrentTempChanged: requestPaint()
            
            function degToRad(deg) {
                return deg * Math.PI / 180
            }
            
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.clearRect(0, 0, width, height);
                
                var cx = width / 2;
                var cy = height / 2;
                var radius = width / 2 - 15;
                
                // Нормализуем температуру в угол (от -50 до 150 -> от 140 до 400 градусов)
                var angleRange = 260; // 260 градусов дуги
                var startAngle = 140; // Начинаем с 140 градусов
                
                var tempNormalized = (currentTemp - minTemp) / (maxTemp - minTemp);
                var tempAngle = startAngle + (tempNormalized * angleRange);
                
                // Рисуем фоновую дугу
                ctx.beginPath();
                ctx.arc(cx, cy, radius, degToRad(startAngle), degToRad(startAngle + angleRange));
                ctx.strokeStyle = "#2c3e50";
                ctx.lineWidth = 12;
                ctx.stroke();
                
                // Рисуем температурную дугу
                ctx.beginPath();
                ctx.arc(cx, cy, radius, degToRad(startAngle), degToRad(tempAngle));
                ctx.strokeStyle = getTempColor(currentTemp);
                ctx.lineWidth = 12;
                ctx.stroke();
                
                // Рисуем деления
                ctx.save();
                ctx.strokeStyle = "#7f8c8d";
                ctx.lineWidth = 2;
                
                for (var i = 0; i <= 10; i++) {
                    var tempValue = minTemp + (maxTemp - minTemp) * (i / 10);
                    var angle = startAngle + (i / 10) * angleRange;
                    var rad = degToRad(angle);
                    
                    var x1 = cx + (radius - 5) * Math.cos(rad);
                    var y1 = cy + (radius - 5) * Math.sin(rad);
                    var x2 = cx + (radius + 5) * Math.cos(rad);
                    var y2 = cy + (radius + 5) * Math.sin(rad);
                    
                    ctx.beginPath();
                    ctx.moveTo(x1, y1);
                    ctx.lineTo(x2, y2);
                    ctx.stroke();
                }
                
                ctx.restore();
            }
        }
        
        // Центральная температура
        Column {
            anchors.centerIn: parent
            spacing: 5
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: temperature.toFixed(1)
                color: getTempColor(root.temperature)
                font.pixelSize: 42
                font.bold: true
            }
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "°C"
                color: "#bdc3c7"
                font.pixelSize: 16
            }
        }
    }
    
    // Информация о датчике
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 5
        
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Slave ID: " + slaveId
            color: "#95a5a6"
            font.pixelSize: 11
        }
        
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: status === "ok" ? "Работает" : 
                  (status === "no_sensor_or_error" ? "Нет датчика" : "Ошибка")
            color: getStatusColor()
            font.pixelSize: 11
            font.bold: true
        }
    }
    
    // Анимация при изменении температуры
    Behavior on temperature {
        NumberAnimation {
            duration: 300
            easing.type: Easing.OutCubic
        }
    }
}

