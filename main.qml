import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtCharts 2.3

Window {
    id: window
    visible: true
    width: 480
    height: 900
    title: qsTr("Smart_lab_Controller")
    property color defaultBgColor: "#f5f7fa"

    color: defaultBgColor
    SequentialAnimation {
        id: backgroundFlashAnimation
        ColorAnimation { target: window; property: "color"; from: window.defaultBgColor; to: "red"; duration: 150 }
        ColorAnimation { target: window; property: "color"; from: "red"; to: window.defaultBgColor; duration: 300 }
    }

    ListModel { id: logModel }
    function addLog(action) {
        var currentTime = Qt.formatDateTime(new Date(), "hh:mm:ss")
        logModel.insert(0, { "time": currentTime, "msg": action })
        if (logModel.count > 10) logModel.remove(10)
    }

    Connections {
        target: cppbackend
        onRemoteCommandReceived: {
            myToast.show(msg)
            successPopup.popupText = msg
            successPopup.open()
        }
        onOverplanpower: {
            warnigPopup.popupText = msg
            warnigPopup.open()
            backgroundFlashAnimation.start()
        }

        onHistoryFetched: {
            historyModel.clear()
            for (var i = 0; i < history.length; i++) {
                historyModel.append(history[i])
            }
        }
        onLightBrightnessChanged: addLog("亮度已更新")
        onLaserPowerChanged: addLog("激光功率已更新")
        onFanActiveChanged: addLog("风扇状态已更新")
        onIsOnlineChanged: addLog(cppbackend.isOnline ? "设备在线" : "设备离线")
        onModbusConnectedChanged: {
            if (cppbackend.modbusConnected) addLog("Modbus 已连接")
        }
        onModbusReconnectingChanged: {
            if (cppbackend.modbusReconnecting) addLog("🟡 Modbus 重连中...")
            else addLog("🔴 Modbus 已断开")
        }
        onModbusRegistersChanged: {
            // QML 绑定自动更新，无需手动处理
        }
        onModbusALarm: {
            // 温度告警：红色弹窗 + 背景闪烁 + 日志
            warnigPopup.popupText = msg
            warnigPopup.open()
            backgroundFlashAnimation.start()
            addLog("🚨 " + msg)
        }
    }

    Popup {
        id: successPopup
        property string popupText: ""
        anchors.centerIn: parent
        width: 300
        height: 150
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "white"; radius: 10; border.color: "#2ecc71"; border.width: 2 }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 20
            Text { text: "解析成功"; font.pixelSize: 20; font.bold: true; color: "#2ecc71"; Layout.alignment: Qt.AlignHCenter }
            Text { text: successPopup.popupText; font.pixelSize: 14; color: "#34495e"; wrapMode: Text.WordWrap; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
            Button { text: "确定"; Layout.alignment: Qt.AlignHCenter; onClicked: successPopup.close() }
        }
    }

    Popup {
        id: warnigPopup
        property string popupText: ""
        anchors.centerIn: parent
        width: 300
        height: 150
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "red"; radius: 10; border.color: "#cc2e2eff"; border.width: 2 }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 20
            Text { text: "超出阈值"; font.pixelSize: 20; font.bold: true; color: "#cc2e2eff"; Layout.alignment: Qt.AlignHCenter }
            Text { text: warnigPopup.popupText; font.pixelSize: 14; color: "white"; wrapMode: Text.WordWrap; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
            Button { text: "确定"; Layout.alignment: Qt.AlignHCenter; onClicked: warnigPopup.close() }
        }
    }

    Rectangle {
        id: myToast
        property alias text: toastTest.text
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 50
        width: 300
        height: 50
        color: "#333333"
        opacity: 0
        radius: 10
        visible: false
        Text { id: toastTest; anchors.centerIn: parent; color: "white" }
        function show(msg) {
            text = msg
            visible = true
            fadeAnimation.start()
        }
        SequentialAnimation on opacity {
            id: fadeAnimation
            NumberAnimation { from: 0; to: 1; duration: 500 }
            PauseAnimation { duration: 2000 }
            NumberAnimation { from: 1; to: 0; duration: 500 }
            onStopped: myToast.visible = false
        }
    }

    Rectangle {
        id: titlearea
        width: parent.width
        height: 60
        color: "#2c3e50"
        z: 2
        Text { text: "智能实验室自动化控制器"; anchors.centerIn: parent; color: "white"; font.pixelSize: 18; font.bold: true }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 15
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Rectangle { width: 12; height: 12; radius: 6; color: cppbackend.isOnline ? "#2ecc71" : "#e74c3c"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: cppbackend.isOnline ? "在线" : "离线"; color: "white"; font.pixelSize: 14 }
        }
    }

    Flickable {
        anchors.top: titlearea.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentHeight: mainLayout.height + 40
        clip: true

        ColumnLayout {
            id: mainLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 15
            spacing: 15

            RowLayout {
                spacing: 15
                Layout.fillWidth: true
                Rectangle { Layout.fillWidth: true; height: 80; radius: 8; color: "white"; Column { anchors.centerIn: parent; Text { text: "激光功率"; color: "#909399" } Text { text: cppbackend.laserPower.toFixed(1) + " W"; color: "#409EFF"; font.bold: true; font.pixelSize: 20 } } }
                Rectangle { Layout.fillWidth: true; height: 80; radius: 8; color: "white"; Column { anchors.centerIn: parent; Text { text: "工作温度"; color: "#909399" } Text { text: "25.5 ℃"; color: "#67C23A"; font.bold: true; font.pixelSize: 20 } } }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 320
                radius: 10
                color: "white"
                Column {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 20
                    Column {
                        width: parent.width
                        spacing: 5
                        Text { text: "功率调节: " + cppbackend.laserPower.toFixed(1) + " W"; font.bold: true }
                        Slider { width: parent.width; from: 0; to: 200; value: cppbackend.laserPower; onMoved: cppbackend.laserPower = value }
                    }
                    RowLayout {
                        width: parent.width
                        Text { text: "排风系统状态"; Layout.fillWidth: true }
                        Image { source: "fan.png"; Layout.preferredWidth: 25; Layout.preferredHeight: 25; RotationAnimation on rotation { running: cppbackend.isFanActive; from: 0; to: 360; duration: 1000; loops: Animation.Infinite } }
                        Switch { checked: cppbackend.isFanActive; onToggled: { cppbackend.isFanActive = checked; addLog(checked ? "开启排风" : "关闭排风") } }
                    }
                    Column {
                        width: parent.width
                        spacing: 5
                        Text { text: "光源亮度: " + Math.floor(lightSlider.value) + "%" }
                        Slider { id: lightSlider; width: parent.width; from: 0; to: 100; value: cppbackend.lightBrightness; onMoved: cppbackend.lightBrightness = value }
                    }
                    Button {
                        width: parent.width
                        height: 40
                        text: "紧急停止"
                        background: Rectangle {
                            color: "#F56C6C"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.bold: true
                        }
                        onClicked: {
                            cppbackend.emergencyStop()
                            addLog("触发紧急停止！")
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 150
                color: "#ebeef5"
                radius: 8
                clip: true
                Text { text: "实时运行日志"; anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 5; font.bold: true; color: "#606266" }
                ListView { anchors.fill: parent; anchors.topMargin: 25; anchors.margins: 10; model: logModel; delegate: Row { spacing: 10; Text { text: time; color: "#999"; font.pixelSize: 11 } Text { text: msg; color: msg.indexOf("警告") !== -1 ? "red" : "#333"; font.pixelSize: 12 } } }
                Row {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 5
                    spacing: 10
                    Button { text: "导出状态"; height: 30; onClicked: { cppbackend.printCurrentJson(); addLog("已在控制台导出数据预览") } }
                    Button { text: "清空"; height: 30; onClicked: { cppbackend.clearLog(); logModel.clear() } }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 250
                color: "white"
                radius: 10
                border.color: "#dcdfe6"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "数据库操作历史 (最近10条)"; font.bold: true; Layout.fillWidth: true }
                        Button { text: "刷新记录"; onClicked: { historyModel.clear(); cppbackend.getHistory() } }
                        Button { text: "导出 CSV"; onClicked: { if (cppbackend.exportToCSV("history_export.csv")) addLog("历史记录已导出至 history_export.csv"); else addLog("导出失败，请检查数据") } }
                    }
                    ListView {
                        id: historyView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: ListModel { id: historyModel }
                        delegate: Rectangle { width: historyView.width; height: 30; color: index % 2 === 0 ? "#b0b5c0ff" : "white"; RowLayout { anchors.fill: parent; anchors.margins: 5; Text { text: time; font.pixelSize: 11; color: "#909399"; Layout.preferredWidth: 120 } Text { text: device; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 60 } Text { text: action; font.pixelSize: 12; Layout.fillWidth: true } Text { text: val; font.pixelSize: 12; color: "#409EFF"; Layout.preferredWidth: 40; horizontalAlignment: Text.AlignRight } } }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 350
                color: "white"
                radius: 10
                border.color: "#dcdfe6"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    RowLayout { Layout.fillWidth: true; Text { text: "实时数值监控 (100ms 采样)"; font.bold: true; Layout.fillWidth: true } ComboBox { id: deviceSelector; model: ["laser", "brightness", "fan"]; currentIndex: 0; onCurrentIndexChanged: { lineSeries.clear(); chartTimer.dataCount = 0 } } }
                    ChartView { id: chartView; Layout.fillWidth: true; Layout.fillHeight: true; antialiasing: true; legend.visible: false; backgroundColor: "transparent"; ValueAxis { id: axisX; min: 0; max: 50; tickCount: 6; labelFormat: "%.0f"; titleText: "采样点" } ValueAxis { id: axisY; min: 0; max: 200; titleText: "数值" } LineSeries { id: lineSeries; name: deviceSelector.currentText; axisX: axisX; axisY: axisY; color: "#409EFF"; width: 2 } }
                    Timer { id: chartTimer; interval: 100; running: true; repeat: true; property int dataCount: 0; onTriggered: { let val = cppbackend.getDeviceValue(deviceSelector.currentText); lineSeries.append(dataCount, val); if (lineSeries.count > 100) lineSeries.remove(0); dataCount++; if (dataCount > axisX.max) { axisX.min = dataCount - 50; axisX.max = dataCount } if (deviceSelector.currentText === "fan") axisY.max = 1.5; else if (deviceSelector.currentText === "brightness") axisY.max = 110; else axisY.max = 210 } }
                }
            }

            // ━━━━━━━━━━━━ 传感器趋势图（最近5分钟）━━━━━━━━━━━━━
            Rectangle {
                Layout.fillWidth: true
                height: 350
                color: "white"
                radius: 10
                border.color: "#dcdfe6"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    Text { text: "传感器趋势（最近5分钟）"; font.bold: true }

                    ChartView {
                        id: sensorTrendChart
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        antialiasing: true
                        legend.visible: true            // 两条线需要图例区分
                        legend.alignment: Qt.AlignBottom
                        backgroundColor: "transparent"

                        // X 轴：真实时间，格式"分:秒"
                        DateTimeAxis {
                            id: trendAxisX
                            format: "mm:ss"
                            min: new Date(Date.now() - 300000)  // 初始：5分钟前
                            max: new Date()                     // 初始：现在
                        }
                        // Y 轴：温度湿度共用 0-100
                        ValueAxis {
                            id: trendAxisY
                            min: 0
                            max: 100
                            titleText: "数值"
                        }
                        LineSeries {
                            id: tempSeries
                            name: "温度(℃)"
                            color: "#E6A23C"
                            width: 2
                            axisX: trendAxisX        // ★ 必须绑定，否则自建坐标轴
                            axisY: trendAxisY
                        }
                        LineSeries {
                            id: humiSeries
                            name: "湿度(%)"
                            color: "#409EFF"
                            width: 2
                            axisX: trendAxisX
                            axisY: trendAxisY
                        }
                    }
                }

                // 采样定时器：每 1 秒取一次数据
                Timer {
                    id: trendTimer
                    interval: 1000
                    running: true
                    repeat: true
                    onTriggered: {
                        // ★ 断线时冻结图表：不再画旧值平线
                        if (!cppbackend.modbusConnected) return

                        var now = new Date().getTime()
                        var temp = cppbackend.modbusRegisters["0"]
                        var humi = cppbackend.modbusRegisters["1"]

                        // ★ 数据可能未到达（undefined），跳过
                        if (temp !== undefined)
                            tempSeries.append(now, temp)
                        if (humi !== undefined)
                            humiSeries.append(now, humi)

                        // 裁剪：只保留最近 5 分钟的点
                        var cutoff = now - 300000
                        while (tempSeries.count > 0 && tempSeries.at(0).x < cutoff)
                            tempSeries.remove(0)
                        while (humiSeries.count > 0 && humiSeries.at(0).x < cutoff)
                            humiSeries.remove(0)

                        // 滚动时间窗口：X 轴跟着时间走
                        trendAxisX.min = new Date(now - 300000)
                        trendAxisX.max = new Date(now)
                    }
                }
            }
            // ━━━━━━━━━━━━ Modbus 传感器面板 ━━━━━━━━━━━━
            Rectangle {
                Layout.fillWidth: true
                height: 300
                color: "white"
                radius: 10
                border.color: "#dcdfe6"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12

                    // 标题行 + 三色状态指示灯
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Modbus 传感器监控"; font.bold: true; Layout.fillWidth: true }
                        // 呼吸灯动画（重连中时闪烁）
                        Rectangle {
                            id: statusDot
                            width: 14; height: 14; radius: 7
                            anchors.verticalCenter: parent.verticalCenter
                            property string state: cppbackend.modbusConnected ? "connected"
                                                : (cppbackend.modbusReconnecting ? "reconnecting" : "disconnected")
                            color: state === "connected" ? "#2ecc71"
                                 : state === "reconnecting" ? "#e6a23c" : "#e74c3c"
                            // 重连中：呼吸闪烁动画
                            SequentialAnimation on opacity {
                                running: statusDot.state === "reconnecting"
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.2; duration: 600 }
                                NumberAnimation { from: 0.2; to: 1.0; duration: 600 }
                            }
                            // 非重连状态：完全不透明
                            opacity: statusDot.state === "reconnecting" ? undefined : 1.0
                        }
                        Text {
                            text: statusDot.state === "connected" ? "🟢 已连接"
                                : (statusDot.state === "reconnecting" ? "🟡 重连中..." : "🔴 已断开")
                            font.pixelSize: 12; color: "#909399"
                        }
                    }

                    // 串口配置行
                    RowLayout {
                        spacing: 10
                        TextField {
                            id: modbusPortInput
                            placeholderText: "串口号"; text: "COM13"
                            Layout.preferredWidth: 80
                        }
                        TextField {
                            id: modbusBaudInput
                            placeholderText: "波特率"; text: "9600"
                            Layout.preferredWidth: 70
                        }
                        Button {
                            text: cppbackend.modbusConnected ? "断开" : "连接"
                            Layout.preferredWidth: 60
                            background: Rectangle {
                                color: cppbackend.modbusConnected ? "#F56C6C" : "#67C23A"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 13
                            }
                            onClicked: {
                                if (cppbackend.modbusConnected) {
                                    cppbackend.disconnectModbus()
                                } else {
                                    cppbackend.connectModbus(modbusPortInput.text, parseInt(modbusBaudInput.text))
                                }
                            }
                        }
                    }

                    // 分隔线
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#ebeef5" }

                    // 寄存器实时值
                    RowLayout {
                        spacing: 15
                        Layout.fillWidth: true
                        Rectangle {
                            Layout.fillWidth: true; height: 70; radius: 8; color: "#f0f9eb"
                            Column {
                                anchors.centerIn: parent
                                Text { text: "寄存器 0 (温度)"; color: "#909399"; font.pixelSize: 12; anchors.horizontalCenter: parent.horizontalCenter }
                                Text {
                                    text: (cppbackend.modbusRegisters["0"] !== undefined ? cppbackend.modbusRegisters["0"].toFixed(1) : "--") + " ℃"
                                    color: "#67C23A"; font.bold: true; font.pixelSize: 22
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; height: 70; radius: 8; color: "#ecf5ff"
                            Column {
                                anchors.centerIn: parent
                                Text { text: "寄存器 1 (湿度)"; color: "#909399"; font.pixelSize: 12; anchors.horizontalCenter: parent.horizontalCenter }
                                Text {
                                    text: (cppbackend.modbusRegisters["1"] !== undefined ? cppbackend.modbusRegisters["1"].toFixed(1) : "--") + " %"
                                    color: "#409EFF"; font.bold: true; font.pixelSize: 22
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }
                        }
                    }

                    // 写入寄存器
                    RowLayout {
                        spacing: 10
                        Layout.fillWidth: true
                        Text { text: "写入寄存器:"; font.pixelSize: 13; color: "#606266" }
                        TextField {
                            id: writeAddrInput; text: "0"; Layout.preferredWidth: 50
                            placeholderText: "地址"
                        }
                        TextField {
                            id: writeValueInput; text: "0"; Layout.preferredWidth: 60
                            placeholderText: "数值"
                        }
                        Button {
                            text: "写入"
                            Layout.preferredWidth: 50
                            onClicked: {
                                cppbackend.writeModbusRegister(1, parseInt(writeAddrInput.text), parseInt(writeValueInput.text))
                                addLog("Modbus 写入: 寄存器[" + writeAddrInput.text + "] ← " + writeValueInput.text)
                            }
                        }
                    }
                    Text { text: "提示：写入后无需等待回复，1秒轮询会自动同步新值到上方显示"; font.pixelSize: 10; color: "#c0c4cc" }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 120
                color: "white"
                radius: 10
                border.color: "#dcdfe6"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    Text { text: "服务器连接配置 (单例模式 + 自动保存)"; font.bold: true }
                    RowLayout {
                        spacing: 20
                        TextField { id: ipInput; placeholderText: "服务器 IP"; text: config.serverIp; Layout.fillWidth: true; onEditingFinished: config.serverIp = text }
                        TextField { id: portInput; placeholderText: "端口"; text: config.serverPort.toString(); Layout.preferredWidth: 80; onEditingFinished: config.serverPort = parseInt(text) }
                        Button { text: "应用并保存"; onClicked: { config.serverIp = ipInput.text; config.serverPort = parseInt(portInput.text); addLog("配置已保存到 config.ini") } }
                    }
                    Text { text: "提示：修改后下次启动程序将自动连接到新地址"; font.pixelSize: 11; color: "#909399" }
                }
            }
        }
    }
}

