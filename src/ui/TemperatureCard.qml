import QtQuick

Rectangle {
    id: tempCard

    property string label: ""
    property int value: 0
    property int threshold: 80
    property color barColor: "#0A84FF"
    property var appTheme: null
    property bool embedded: false

    radius: embedded ? 0 : 16
    color: embedded ? "transparent" : (appTheme ? appTheme.card : "#1C1C1E")

    Column {
        anchors.fill: parent
        anchors.margins: tempCard.embedded ? 0 : 14
        spacing: 8

        Item {
            width: parent.width
            height: 22

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: tempCard.label
                font.pixelSize: 13
                font.weight: Font.Medium
                color: tempCard.appTheme ? tempCard.appTheme.textMuted : "#8E8E93"
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: tempCard.value + "°"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: tempCard.value >= tempCard.threshold
                       ? "#FF453A"
                       : (tempCard.appTheme ? tempCard.appTheme.textLabel : "#FFFFFF")
            }
        }

        Rectangle {
            width: parent.width
            height: 4
            radius: 2
            color: tempCard.appTheme ? tempCard.appTheme.barTrack : "#2C2C2E"

            Rectangle {
                width: Math.min(parent.width, parent.width * Math.max(0, tempCard.value) / 100)
                height: parent.height
                radius: 2
                color: tempCard.value >= tempCard.threshold ? "#FF453A" : tempCard.barColor
            }
        }
    }
}
