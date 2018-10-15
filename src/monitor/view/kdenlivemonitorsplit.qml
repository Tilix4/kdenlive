import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Item {
    id: root
    objectName: "rootsplit"
    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property double timeScale: 1
    property double frameSize: 10
    property int duration: 300
    property int mouseRulerPos: 0
    property int splitterPos
    property point center
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    // percentage holds splitter pos relative to the scene percentage
    property double percentage
    // realpercent holds splitter pos relative to the frame width percentage
    property double realpercent

    signal qmlMoveSplit()

    FontMetrics {
        id: fontMetrics
        font.family: "Arial"
    }

    percentage: 0.5
    realpercent: 0.5
    splitterPos: this.width / 2

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }

    MouseArea {
        width: root.width; height: root.height
        anchors.centerIn: parent
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        acceptedButtons: Qt.LeftButton
        onPressed: {
            root.percentage = mouseX / width
            root.splitterPos = mouseX
            root.qmlMoveSplit()
        }
        onPositionChanged: {
            if (pressed) {
                root.percentage = mouseX / width
                root.splitterPos = mouseX
                root.qmlMoveSplit()
            }
            timer.restart()
            splitter.visible = true
        }
        //onEntered: { splitter.visible = true }
        onExited: { splitter.visible = false }
    }

    Rectangle {
        id: splitter
        x: root.splitterPos
        y: 0
        width: 1
        height: root.height
        color: "red"
        visible: false
        Text {
            text: 'Effect'
            color: "red"
            anchors {
                right: parent.left
                top: parent.top
                topMargin: 10
                rightMargin: 10
            }
        }
    }
    MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        height: controller.rulerHeight
    }

    Timer {
        id: timer

        interval: 1000; running: false; repeat: false
        onTriggered:  {
            splitter.visible = false
        }
    }
}
