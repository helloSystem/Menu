import QtQuick 2.2
import QtQuick.Controls 2.15
import QtQuick.Window 2.0
import QtGraphicalEffects 1.0
import QtMultimedia 5.15
import QtQml 2.15

ApplicationWindow {

    id: window
    visible: true
    visibility: "FullScreen"
    color: Qt.rgba(0, 0, 0, 0)
    modality: Qt.WindowStaysOnTopHint

    SequentialAnimation on color {
        PropertyAnimation { to: Qt.rgba(0, 0, 0, 1); duration: 2000 ; easing.type: Easing.InOutCubic }
    }

    Component.onDestruction: {
        console.log("Exiting")
    }

}
