import QtQuick
import QtQuick.Controls

Rectangle {
    id: slider

    property int value: 80
    property int minValue: 30
    property int maxValue: 100
    property string label: ""
    property var appTheme: null

    signal thresholdChanged(int newValue)

    radius: 16
    color: appTheme ? appTheme.card : "#1C1C1E"

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            text: slider.label + ": " + slider.value + "°C"
            font.pixelSize: 15
            font.weight: Font.Medium
            color: appTheme ? appTheme.textLabel : "#FFFFFF"
        }

        Slider {
            id: control
            width: parent.width
            from: slider.minValue
            to: slider.maxValue
            value: slider.value
            onValueChanged: {
                slider.value = value
                slider.thresholdChanged(value)
            }

            background: Rectangle {
                width: control.width
                height: 6
                radius: 3
                color: appTheme ? appTheme.barTrack : "#2C2C2E"
                Rectangle {
                    width: control.visualPosition * parent.width
                    height: parent.height
                    radius: 3
                    color: appTheme ? appTheme.barFill : "#0A84FF"
                }
            }

            handle: Rectangle {
                x: control.leftPadding + control.visualPosition * (control.width - width)
                y: control.topPadding + (control.height - height) / 2
                width: 18
                height: 18
                radius: 9
                color: "#FFFFFF"
                border.width: 1
                border.color: Qt.rgba(0, 0, 0, 0.16)
            }
        }
    }
}
