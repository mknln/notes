import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: root

    required property string noteId
    required property string title
    required property string preview
    required property date modified
    required property bool pinned
    required property bool selected

    signal contextMenuRequested(string noteId, string title)

    width: ListView.view.width
    highlighted: selected

    function formatModified(when) {
        const now = new Date()
        if (when.toDateString() === now.toDateString())
            return Qt.formatTime(when, Locale.ShortFormat)
        return Qt.formatDate(when, Locale.ShortFormat)
    }

    contentItem: ColumnLayout {
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: root.title
                elide: Text.ElideRight
            }
            Label {
                visible: root.pinned
                text: "📌"
                Accessible.name: qsTr("Pinned")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: root.formatModified(root.modified)
                opacity: 0.7
            }
            Label {
                Layout.fillWidth: true
                text: root.preview
                elide: Text.ElideRight
                opacity: 0.7
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root.contextMenuRequested(root.noteId, root.title)
    }
}
