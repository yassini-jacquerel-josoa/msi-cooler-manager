import QtQuick

Rectangle {
    id: card

    property string label: ""
    property bool isActive: false
    property string iconSource: ""
    property real iconOpacity: 1
    property string detail: ""
    property color iconColor: "#ffffff"
    property bool large: false
    property var appTheme: null

    signal activated()

    radius: 22
    color: {
        var theme = appTheme
        if (!theme)
            return isActive ? Qt.rgba(0.04, 0.52, 1, 0.78) : Qt.rgba(1, 1, 1, 0.10)
        if (isActive)
            return mouseArea.containsMouse ? theme.cardActiveHover : theme.cardActive
        return mouseArea.containsMouse ? theme.cardHover : theme.card
    }
    border.width: 0

    Behavior on color { ColorAnimation { duration: 160 } }

    Column {
        anchors.centerIn: parent
        width: parent.width
        spacing: large ? 12 : 8

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: large ? 52 : 40
            height: width
            source: card.iconSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: false
            sourceSize.width: Math.round(width * 2)
            sourceSize.height: Math.round(height * 2)
            opacity: card.iconOpacity
            Behavior on opacity { NumberAnimation { duration: 180 } }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: card.label
            font.pixelSize: large ? 17 : 15
            font.weight: Font.DemiBold
            color: "#FFFFFF"
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.activated()
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: card.large ? 14 : 10
        text: card.detail
        visible: text.length > 0
        font.pixelSize: card.large ? 13 : 12
        font.weight: Font.Medium
        color: card.appTheme ? card.appTheme.textMuted : Qt.rgba(1, 1, 1, 0.55)
    }
}
