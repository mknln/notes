pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // Notes are tracked by id rather than row, because editing a note moves
    // it to the top of the list.
    property string currentId: notesModel.idAt(0)

    width: 1100
    height: 700
    visible: true
    title: qsTr("Notes")

    function newNote() {
        currentId = notesModel.createNote()
        editor.forceActiveFocus()
    }

    function requestDelete(id) {
        deleteDialog.noteId = id
        deleteDialog.noteTitle = notesModel.title(id)
        deleteDialog.open()
    }

    function deleteNote(id) {
        const row = notesModel.indexOf(id)
        notesModel.removeNote(id)
        if (id === currentId)
            currentId = notesModel.idAt(Math.min(row, notesModel.count - 1))
    }

    onCurrentIdChanged: editor.text = notesModel.text(currentId)
    Component.onCompleted: editor.text = notesModel.text(currentId)

    NotesModel {
        id: notesModel
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&New Note")
                shortcut: StandardKey.New
                onTriggered: window.newNote()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Quit")
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("&Note")
            Action {
                text: qsTr("&Delete Note")
                enabled: window.currentId !== ""
                onTriggered: window.requestDelete(window.currentId)
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        ColumnLayout {
            SplitView.preferredWidth: window.width * 0.2
            SplitView.minimumWidth: 180
            spacing: 0

            ToolBar {
                Layout.fillWidth: true

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8

                    Label {
                        Layout.fillWidth: true
                        text: notesModel.count === 1 ? qsTr("1 note") : qsTr("%1 notes").arg(notesModel.count)
                        opacity: 0.7
                    }
                    ToolButton {
                        text: "+"
                        font.pixelSize: 20
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("New note")
                        onClicked: window.newNote()
                    }
                }
            }

            ListView {
                id: noteList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: notesModel
                ScrollBar.vertical: ScrollBar {}

                delegate: NoteListDelegate {
                    selected: noteId === window.currentId
                    onClicked: window.currentId = noteId
                    onContextMenuRequested: clickedId => {
                        window.currentId = clickedId
                        noteMenu.noteId = clickedId
                        noteMenu.popup()
                    }
                }
            }
        }

        ScrollView {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 240

            TextArea {
                id: editor
                enabled: window.currentId !== ""
                wrapMode: TextArea.Wrap
                font.pixelSize: 15
                padding: 16
                placeholderText: enabled ? qsTr("Start typing…") : qsTr("No note selected. Press + to create one.")
                // The model ignores unchanged text, so loading a note here
                // doesn't count as an edit.
                onTextChanged: if (window.currentId) notesModel.setText(window.currentId, text)
            }
        }
    }

    Menu {
        id: noteMenu
        property string noteId

        MenuItem {
            text: qsTr("Delete Note")
            onTriggered: window.requestDelete(noteMenu.noteId)
        }
    }

    Dialog {
        id: deleteDialog
        property string noteId
        property string noteTitle

        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("Delete Note?")
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            text: qsTr("“%1” will be permanently deleted.").arg(deleteDialog.noteTitle)
        }

        onAccepted: window.deleteNote(noteId)
    }
}
