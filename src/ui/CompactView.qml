import QtQuick

Item {
    id: compactView

    property var appTheme: null
    property string activeMode: "AUTO"
    property int cpuTemp: 0
    property int gpuTemp: 0
    property bool coolerBoostOn: false
    property bool isDark: true
    property real f7Progress: 0.0
    property bool f7Holding: false

    signal expandRequested()
    signal dismissRequested()
    signal userInteracted()

    readonly property color muted: appTheme ? appTheme.textMuted : "#8E8E93"
    readonly property color label: appTheme ? appTheme.textLabel : "#FFFFFF"
    readonly property color chip: appTheme ? appTheme.cardHover : "#2C2C2E"

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Item {
            width: parent.width
            height: 28

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: "CPU " + compactView.cpuTemp + "°"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: compactView.label
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "GPU " + compactView.gpuTemp + "°"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: compactView.muted
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: expandArea.containsMouse ? compactView.chip : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "↗"
                        font.pixelSize: 14
                        color: compactView.muted
                    }
                    MouseArea {
                        id: expandArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            compactView.userInteracted()
                            compactView.expandRequested()
                        }
                    }
                }

                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: closeArea.containsMouse ? compactView.chip : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: compactView.muted
                    }
                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: compactView.dismissRequested()
                    }
                }
            }
        }

        Row {
            width: parent.width
            height: parent.height - 40
            spacing: 10

            ModeCard {
                width: (parent.width - 10) / 2
                height: parent.height
                label: qsTr("Auto")
                isActive: compactView.activeMode === "AUTO"
                iconSource: "qrc:/MSI/CoolerManager/resources/svg/balance-light.svg"
                appTheme: compactView.appTheme
                onActivated: {
                    compactView.userInteracted()
                    controller.setMode("AUTO")
                }
            }

            ModeCard {
                width: (parent.width - 10) / 2
                height: parent.height
                label: qsTr("Manuel")
                isActive: compactView.activeMode === "MANUEL"
                iconOpacity: compactView.activeMode === "MANUEL" && !compactView.coolerBoostOn ? 0.28 : 1
                detail: compactView.activeMode === "MANUEL"
                       ? (compactView.coolerBoostOn ? qsTr("Actif") : qsTr("Inactif"))
                       : ""
                iconSource: "qrc:/MSI/CoolerManager/resources/svg/fire-light.svg"
                appTheme: compactView.appTheme
                onActivated: {
                    compactView.userInteracted()
                    if (compactView.activeMode === "MANUEL")
                        controller.toggleCoolerBoost()
                    else
                        controller.setMode("MANUEL")
                }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16
        height: 3
        radius: 2
        width: Math.max(0, (parent.width - 32) * compactView.f7Progress)
        color: compactView.appTheme ? compactView.appTheme.barFill : "#0A84FF"
        visible: compactView.f7Holding
    }
}
