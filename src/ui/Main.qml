import QtQuick
import QtQuick.Window

Window {
    id: root

    property bool isExpanded: false
    property real expand: 0

    readonly property int compactWidth: 360
    readonly property int compactHeight: 280
    readonly property int expandedWidth: 400
    readonly property int expandedHeight: 540

    width: expandedWidth
    height: expandedHeight
    visible: false
    opacity: presence
    title: qsTr("MSI Cooler Manager")
    flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowStaysOnTopHint
    color: "transparent"

    property bool shown: false
    property real presence: 0
    property bool suppressExpandAnim: false

    function smoothstep(edge0, edge1, value) {
        var t = Math.max(0, Math.min(1, (value - edge0) / (edge1 - edge0)))
        return t * t * (3 - 2 * t)
    }

    readonly property real compactOpacity: 1 - smoothstep(0.0, 0.4, expand)
    readonly property real expandedOpacity: smoothstep(0.25, 0.8, expand)

    onIsExpandedChanged: {
        if (suppressExpandAnim) {
            expand = isExpanded ? 1 : 0
            return
        }
        expandAnim.to = isExpanded ? 1 : 0
        expandAnim.start()
        if (isExpanded)
            idleTimer.stop()
        else if (shown)
            idleTimer.restart()
    }

    NumberAnimation {
        id: expandAnim
        target: root
        property: "expand"
        duration: 460
        easing.type: Easing.OutCubic
        onStarted: {
            if (typeof inputMask !== "undefined")
                inputMask.clear(root)
        }
        onFinished: {
            if (typeof inputMask !== "undefined")
                inputMask.update(panel)
        }
    }

    // La fenêtre reste fixe. Seul le panneau intérieur change de taille.
    x: (Screen.width - width) / 2
    y: Screen.height - height - 36

    function ensureRunning() {
        if (typeof controller !== "undefined")
            controller.start()
    }

    function reveal() {
        ensureRunning()
        shown = true
        presenceAnim.stop()
        presence = 0
        visible = true
        raise()
        fadeInTimer.restart()
    }

    function showMini() {
        if (isExpanded)
            isExpanded = false
        reveal()
    }

    function hidePanel() {
        idleTimer.stop()
        fadeInTimer.stop()
        shown = false
        presenceAnim.easing.type = Easing.OutCubic
        presenceAnim.from = presence
        presenceAnim.to = 0
        presenceAnim.start()
    }

    function noteActivity() {
        if (shown && !isExpanded)
            idleTimer.restart()
    }

    function handleF7Down() {
        idleTimer.stop()
    }

    function handleShortF7() {
        var wasShown = shown
        if (!wasShown)
            showMini()
        if (typeof controller === "undefined")
            return
        // Panneau caché et pas en Manuel : F7 ne fait qu'afficher.
        if (!wasShown && controller.mode !== "MANUEL")
            return
        if (controller.mode === "MANUEL") {
            if (controller.coolerBoostActive)
                controller.toggleCoolerBoost()
            else
                controller.setMode("AUTO")
        } else {
            controller.setMode("MANUEL")
        }
        noteActivity()
    }

    function expandPanel() {
        idleTimer.stop()
        ensureRunning()
        if (!shown) {
            fadeInTimer.stop()
            presenceAnim.stop()
            shown = true
            presence = 1
            visible = true
            raise()
        }
        if (!isExpanded)
            isExpanded = true
    }

    function handleLongF7() {
        expandPanel()
    }

    Timer {
        id: fadeInTimer
        interval: 32
        repeat: false
        onTriggered: {
            if (!root.shown)
                return
            presenceAnim.easing.type = Easing.OutCubic
            presenceAnim.from = 0
            presenceAnim.to = 1
            presenceAnim.start()
        }
    }

    NumberAnimation {
        id: presenceAnim
        target: root
        property: "presence"
        duration: 560
        easing.type: Easing.OutCubic
        onStarted: {
            if (root.shown && typeof inputMask !== "undefined")
                inputMask.update(panel)
        }
        onFinished: {
            if (!root.shown) {
                root.visible = false
                root.suppressExpandAnim = true
                root.isExpanded = false
                root.expand = 0
                root.suppressExpandAnim = false
                return
            }
            if (typeof inputMask !== "undefined")
                inputMask.update(panel)
            if (!root.isExpanded && !idleTimer.running)
                idleTimer.restart()
        }
    }

    Timer {
        id: idleTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (root.shown && !root.isExpanded)
                root.hidePanel()
        }
    }

    readonly property bool isDark: true
    readonly property var theme: ({
        bg: Qt.rgba(0, 0, 0, 0.62),
        card: Qt.rgba(1, 1, 1, 0.10),
        cardHover: Qt.rgba(1, 1, 1, 0.16),
        cardActive: Qt.rgba(0.04, 0.52, 1, 0.78),
        cardActiveHover: Qt.rgba(0.25, 0.61, 1, 0.88),
        border: Qt.rgba(1, 1, 1, 0.20),
        borderSel: Qt.rgba(0.04, 0.52, 1, 0.9),
        textLabel: "#FFFFFF",
        textMuted: Qt.rgba(1, 1, 1, 0.55),
        barTrack: Qt.rgba(1, 1, 1, 0.16),
        barFill: Qt.rgba(0.04, 0.52, 1, 0.95),
        iconAuto: "#FFFFFF",
        iconManuel: "#FFFFFF",
        danger: "#FF453A"
    })

    // Données
    readonly property string activeMode: (typeof controller !== "undefined") ? controller.mode : "AUTO"
    readonly property int cpuTemp: (typeof controller !== "undefined") ? controller.cpuTemperature : 0
    readonly property int gpuTemp: (typeof controller !== "undefined") ? controller.gpuTemperature : 0
    readonly property bool coolerBoostOn: (typeof controller !== "undefined") ? controller.coolerBoostActive : false

    // Gestion F7
    property bool f7Holding: false
    property bool f7LongTriggered: false
    property real f7Progress: 0.0
    property real f7StartTime: 0

    function isPlainF7(event) {
        if (typeof globalHotkey !== "undefined" && globalHotkey)
            return false
        return event.key === Qt.Key_F7
            && !event.isAutoRepeat
            && (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.ShiftModifier | Qt.MetaModifier)) === 0
    }

    function handleF7Press() {
        root.handleF7Down()
        f7Holding = true
        f7LongTriggered = false
        f7Progress = 0.0
        f7StartTime = Date.now()
        f7HoldTimer.restart()
        f7ProgressTimer.restart()
    }

    function handleF7Release() {
        f7HoldTimer.stop()
        f7ProgressTimer.stop()
        f7Holding = false
        f7Progress = 0.0
        if (!f7LongTriggered)
            root.handleShortF7()
        f7LongTriggered = false
    }

    Timer {
        id: f7HoldTimer
        interval: 2000
        repeat: false
        onTriggered: {
            f7LongTriggered = true
            f7Progress = 1.0
            root.handleLongF7()
        }
    }
    Timer {
        id: f7ProgressTimer
        interval: 25
        repeat: true
        onTriggered: f7Progress = Math.min(1.0, (Date.now() - f7StartTime) / 2000.0)
    }

    // Capture clavier
    Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true
        onActiveFocusChanged: {
            if (!activeFocus)
                forceActiveFocus()
        }
        Keys.onPressed: (event) => {
            if (root.isPlainF7(event)) {
                root.handleF7Press()
                event.accepted = true
            }             else if (event.key === Qt.Key_Escape) {
                if (root.isExpanded) root.isExpanded = false
                else if (root.shown) root.hidePanel()
                event.accepted = true
            }
        }
        Keys.onReleased: (event) => {
            if (root.isPlainF7(event)) {
                root.handleF7Release()
                event.accepted = true
            }
        }
    }

    Item {
        id: panel
        width: root.compactWidth + (root.expandedWidth - root.compactWidth) * root.expand
        height: root.compactHeight + (root.expandedHeight - root.compactHeight) * root.expand
        x: (root.width - width) / 2
        y: root.height - height

        Rectangle {
            anchors.fill: parent
            radius: 32
            color: root.theme.bg
            border.color: root.theme.border
            border.width: 1
            clip: true

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    root.noteActivity()
                    root.startSystemMove()
                }
            }

            CompactView {
                width: root.compactWidth
                height: root.compactHeight
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                opacity: root.isExpanded ? 0 : root.compactOpacity
                visible: !root.isExpanded && opacity > 0.02
                enabled: visible
                appTheme: root.theme
                activeMode: root.activeMode
                cpuTemp: root.cpuTemp
                gpuTemp: root.gpuTemp
                coolerBoostOn: root.coolerBoostOn
                isDark: root.isDark
                f7Progress: root.f7Progress
                f7Holding: root.f7Holding
                onExpandRequested: root.expandPanel()
                onDismissRequested: root.hidePanel()
                onUserInteracted: root.noteActivity()
            }

            ExpandedView {
                id: expandedContent
                width: root.expandedWidth
                height: root.expandedHeight
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                transformOrigin: Item.Bottom
                scale: root.isExpanded ? (panel.height / root.expandedHeight) : 1
                opacity: root.isExpanded ? 1 : root.expandedOpacity
                visible: root.isExpanded || opacity > 0.02
                enabled: visible && scale > 0.9
                appTheme: root.theme
                activeMode: root.activeMode
                cpuTemp: root.cpuTemp
                gpuTemp: root.gpuTemp
                coolerBoostOn: root.coolerBoostOn
                isDark: root.isDark
                onCollapseRequested: root.isExpanded = false
                onDismissRequested: root.hidePanel()
            }
        }
    }
}
