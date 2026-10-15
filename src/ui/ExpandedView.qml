import QtQuick

Item {
    id: expandedView

    property var appTheme: null
    property string activeMode: "AUTO"
    property int cpuTemp: 0
    property int gpuTemp: 0
    property bool coolerBoostOn: false
    property bool isDark: true

    signal collapseRequested()
    signal dismissRequested()

    readonly property color muted: appTheme ? appTheme.textMuted : "#8E8E93"
    readonly property color label: appTheme ? appTheme.textLabel : "#FFFFFF"
    readonly property color card: appTheme ? appTheme.card : "#1C1C1E"
    readonly property color chip: appTheme ? appTheme.cardHover : "#2C2C2E"
    readonly property color accent: appTheme ? appTheme.barFill : "#0A84FF"
    readonly property string autoIcon: (!isDark && activeMode !== "AUTO")
        ? "qrc:/MSI/CoolerManager/resources/svg/balance-dark.svg"
        : "qrc:/MSI/CoolerManager/resources/svg/balance-light.svg"
    readonly property string manuelIcon: (!isDark && activeMode !== "MANUEL")
        ? "qrc:/MSI/CoolerManager/resources/svg/fire-dark.svg"
        : "qrc:/MSI/CoolerManager/resources/svg/fire-light.svg"

    component RoundButton : Rectangle {
        property string glyph: ""
        signal clicked()
        width: 30
        height: 30
        radius: 15
        color: area.containsMouse ? expandedView.chip : "transparent"
        Text {
            anchors.centerIn: parent
            text: glyph
            font.pixelSize: 13
            font.weight: Font.Medium
            color: expandedView.muted
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    component Stepper : Rectangle {
        property string glyph: ""
        signal clicked()
        width: 28
        height: 28
        radius: 14
        color: area.containsMouse
              ? (expandedView.appTheme ? expandedView.appTheme.cardPressed : Qt.rgba(1, 1, 1, 0.24))
              : expandedView.chip
        Text {
            anchors.centerIn: parent
            text: glyph
            font.pixelSize: 16
            font.weight: Font.Medium
            color: expandedView.label
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Item {
            width: parent.width
            height: 32

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Cooler")
                font.pixelSize: 20
                font.weight: Font.DemiBold
                font.letterSpacing: -0.4
                color: expandedView.label
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                RoundButton {
                    glyph: "↙"
                    onClicked: expandedView.collapseRequested()
                }
                RoundButton {
                    glyph: "✕"
                    onClicked: expandedView.dismissRequested()
                }
            }
        }

        Row {
            width: parent.width
            height: 156
            spacing: 10

            ModeCard {
                width: (parent.width - 10) / 2
                height: parent.height
                large: true
                label: qsTr("Auto")
                isActive: expandedView.activeMode === "AUTO"
                iconSource: expandedView.autoIcon
                appTheme: expandedView.appTheme
                onActivated: controller.setMode("AUTO")
            }

            ModeCard {
                width: (parent.width - 10) / 2
                height: parent.height
                large: true
                label: qsTr("Manuel")
                isActive: expandedView.activeMode === "MANUEL"
                iconOpacity: expandedView.activeMode === "MANUEL" && !expandedView.coolerBoostOn ? 0.28 : 1
                detail: expandedView.activeMode === "MANUEL"
                       ? (expandedView.coolerBoostOn ? qsTr("Actif") : qsTr("Inactif"))
                       : ""
                iconSource: expandedView.manuelIcon
                appTheme: expandedView.appTheme
                onActivated: {
                    if (expandedView.activeMode === "MANUEL")
                        controller.toggleCoolerBoost()
                    else
                        controller.setMode("MANUEL")
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 116
            radius: 18
            color: expandedView.card

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                TemperatureCard {
                    width: parent.width
                    height: 36
                    embedded: true
                    label: qsTr("CPU")
                    value: expandedView.cpuTemp
                    threshold: controller.cpuThreshold
                    barColor: expandedView.accent
                    appTheme: expandedView.appTheme
                }

                TemperatureCard {
                    width: parent.width
                    height: 36
                    embedded: true
                    label: qsTr("GPU")
                    value: expandedView.gpuTemp
                    threshold: controller.gpuThreshold
                    barColor: expandedView.accent
                    appTheme: expandedView.appTheme
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 108
            radius: 18
            color: expandedView.card

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Item {
                    width: parent.width
                    height: 32

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Seuil CPU")
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        color: expandedView.label
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        Stepper {
                            glyph: "−"
                            onClicked: controller.setCpuThreshold(controller.cpuThreshold - 5)
                        }
                        Text {
                            width: 42
                            text: controller.cpuThreshold + "°"
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            color: expandedView.label
                            horizontalAlignment: Text.AlignHCenter
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Stepper {
                            glyph: "+"
                            onClicked: controller.setCpuThreshold(controller.cpuThreshold + 5)
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 32

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Seuil GPU")
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        color: expandedView.label
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        Stepper {
                            glyph: "−"
                            onClicked: controller.setGpuThreshold(controller.gpuThreshold - 5)
                        }
                        Text {
                            width: 42
                            text: controller.gpuThreshold + "°"
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            color: expandedView.label
                            horizontalAlignment: Text.AlignHCenter
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Stepper {
                            glyph: "+"
                            onClicked: controller.setGpuThreshold(controller.gpuThreshold + 5)
                        }
                    }
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("F7 change le mode")
            font.pixelSize: 12
            font.weight: Font.Medium
            color: expandedView.muted
        }
    }
}
