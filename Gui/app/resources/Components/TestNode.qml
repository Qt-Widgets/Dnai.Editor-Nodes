import QtQuick 2.9
import QtQuick.Controls 2.2
import DNAI.Views 1.0
import DNAI 1.0

import "../Style"
import "../Nodes"
import "../Nodes/Operator"
import "../Nodes/Operator/BinaryOperator"
import "../Nodes/Operator/BinaryOperator/Logical"
import "../Nodes/Operator/UnaryOperator"

DropArea {
    id: dragTarget
    property int margin: 30
    property alias dropProxy: dragTarget
    CanvasNode {
        id: canvas
        anchors.fill: parent
        backgroundColor: AppSettings.style.canvas.background
        gridStep: AppSettings.style.canvas.grid.step
        gridColor: AppSettings.style.canvas.grid.color
        accentGridStep: AppSettings.style.canvas.grid.accentStep
        accentGridColor: AppSettings.style.canvas.grid.accentColor
        states: [
            State {
                when: dragTarget.containsDrag
                PropertyChanges {
                    target: canvas
                    backgroundColor: AppSettings.style.canvas.grid.selectColor
                }
            }
        ]
        Item {
            id: content_item
            x: canvas.width/2
            y: canvas.height/2
            Node {
               x: 0
               y: 0
               model: SubstractModel {}
            }

            Node {
               x: 100
               y: 100
               model: SubstractModel {}
            }
            onScaleChanged: {
                zoomText.text = Math.round(content_item.scale * 100) + "%"
            }
        }
        content: content_item

        MLabel {
            id: zoomText
            text: (content_item.scale * 100) + "%"
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.top: parent.top
            anchors.topMargin: 20
        }
    }
}