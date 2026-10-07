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

    // Set while a note's text is put into the editor, so that doesn't count
    // as an edit. The editor's Markdown can differ from the saved file (e.g.
    // in list spacing) even though nothing changed.
    property bool loadingNote: false

    function loadNote() {
        loadingNote = true
        editor.text = notesModel.text(currentId)
        // Otherwise the format at the cursor in the previous note (e.g. a
        // heading) carries over into this one.
        formatter.resetTypingFormat()
        loadingNote = false
    }

    onCurrentIdChanged: loadNote()
    Component.onCompleted: loadNote()

    NotesModel {
        id: notesModel
    }

    TextFormatter {
        id: formatter
        document: editor.textDocument
        selectionStart: editor.selectionStart
        selectionEnd: editor.selectionEnd
    }

    // Shared by the Format menu and the formatting toolbar.
    Action {
        id: boldAction
        text: qsTr("&Bold")
        shortcut: StandardKey.Bold
        enabled: editor.enabled
        checkable: true
        checked: formatter.bold
        onTriggered: formatter.bold = checked
    }
    Action {
        id: italicAction
        text: qsTr("&Italic")
        shortcut: StandardKey.Italic
        enabled: editor.enabled
        checkable: true
        checked: formatter.italic
        onTriggered: formatter.italic = checked
    }
    Action {
        id: bulletListAction
        text: qsTr("B&ulleted List")
        shortcut: "Ctrl+Shift+8"
        enabled: editor.enabled
        checkable: true
        checked: formatter.listStyle === TextFormatter.BulletList
        onTriggered: formatter.listStyle = checked ? TextFormatter.BulletList : TextFormatter.NoList
    }
    Action {
        id: numberedListAction
        text: qsTr("&Numbered List")
        shortcut: "Ctrl+Shift+7"
        enabled: editor.enabled
        checkable: true
        checked: formatter.listStyle === TextFormatter.NumberedList
        onTriggered: formatter.listStyle = checked ? TextFormatter.NumberedList : TextFormatter.NoList
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
        Menu {
            title: qsTr("F&ormat")
            MenuItem { action: boldAction }
            MenuItem { action: italicAction }
            MenuSeparator {}
            Menu {
                id: headingMenu
                title: qsTr("&Heading")
                Instantiator {
                    model: 7
                    delegate: Action {
                        required property int index
                        text: index === 0 ? qsTr("&Normal Text") : qsTr("Heading &%1").arg(index)
                        shortcut: "Ctrl+" + index
                        enabled: editor.enabled
                        checkable: true
                        checked: formatter.headingLevel === index
                        onTriggered: formatter.headingLevel = index
                    }
                    onObjectAdded: (index, object) => headingMenu.insertAction(index, object)
                    onObjectRemoved: (index, object) => headingMenu.removeAction(object)
                }
            }
            MenuSeparator {}
            MenuItem { action: bulletListAction }
            MenuItem { action: numberedListAction }
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

        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 240
            spacing: 0

            // The controls don't take focus, so the editor keeps its cursor
            // and selection while they are used.
            ToolBar {
                Layout.fillWidth: true
                enabled: editor.enabled

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    spacing: 2

                    ComboBox {
                        focusPolicy: Qt.NoFocus
                        implicitContentWidthPolicy: ComboBox.WidestText
                        model: [qsTr("Normal Text"), qsTr("Heading 1"), qsTr("Heading 2"), qsTr("Heading 3"),
                            qsTr("Heading 4"), qsTr("Heading 5"), qsTr("Heading 6")]
                        currentIndex: formatter.headingLevel
                        onActivated: index => formatter.headingLevel = index
                    }
                    ToolSeparator {}
                    ToolButton {
                        action: boldAction
                        text: "B"
                        font.bold: true
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Bold (%1)").arg(boldAction.shortcut)
                    }
                    ToolButton {
                        action: italicAction
                        text: "I"
                        font.italic: true
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Italic (%1)").arg(italicAction.shortcut)
                    }
                    ToolSeparator {}
                    ToolButton {
                        action: bulletListAction
                        text: "•"
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Bulleted list (%1)").arg(bulletListAction.shortcut)
                    }
                    ToolButton {
                        action: numberedListAction
                        text: "1."
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Numbered list (%1)").arg(numberedListAction.shortcut)
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
            }

            ScrollView {
                id: editorScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                // The style's scroll bar only shows while scrolling, and is
                // too faint to see on white, so this one is always shown when
                // the note overflows and has a visible track.
                ScrollBar.vertical: ScrollBar {
                    id: editorScrollBar
                    parent: editorScroll
                    x: editorScroll.mirrored ? 0 : editorScroll.width - width
                    y: editorScroll.topPadding
                    height: editorScroll.availableHeight
                    policy: editor.height > editorScroll.availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                    padding: 2

                    contentItem: Rectangle {
                        implicitWidth: 8
                        radius: width / 2
                        color: editorScrollBar.pressed ? editorScrollBar.palette.shadow
                             : editorScrollBar.hovered ? Qt.darker(editorScrollBar.palette.dark, 1.2)
                             : editorScrollBar.palette.dark
                    }
                    background: Rectangle {
                        color: editorScrollBar.palette.window
                        border.color: editorScrollBar.palette.mid
                        border.width: 1
                    }
                }

                // TextEdit doesn't handle Page Up/Down, since it doesn't know
                // the size of the view. Scrolls a page and moves the cursor
                // with it, keeping its place on screen.
                function page(direction, select) {
                    const flickable = contentItem
                    const distance = direction * availableHeight
                    const maxY = Math.max(0, flickable.contentHeight - flickable.height)
                    const rect = editor.cursorRectangle
                    const position = editor.positionAt(rect.x, rect.y + rect.height / 2 + distance)
                    flickable.contentY = Math.max(0, Math.min(maxY, flickable.contentY + distance))
                    if (select)
                        editor.moveCursorSelection(position, TextEdit.SelectCharacters)
                    else
                        editor.cursorPosition = position
                }

                TextArea {
                    id: editor
                    enabled: window.currentId !== ""
                    textFormat: TextEdit.MarkdownText
                    wrapMode: TextArea.Wrap
                    font.pixelSize: 15
                    padding: 16
                    placeholderText: enabled ? qsTr("Start typing…") : qsTr("No note selected. Press + to create one.")
                    onTextChanged: if (window.currentId && !window.loadingNote) notesModel.setText(window.currentId, text)
                    Keys.onReturnPressed: event => event.accepted = formatter.handleReturn()
                    Keys.onEnterPressed: event => event.accepted = formatter.handleReturn()
                    Keys.onPressed: event => {
                        if (event.key !== Qt.Key_PageUp && event.key !== Qt.Key_PageDown)
                            return
                        editorScroll.page(event.key === Qt.Key_PageUp ? -1 : 1,
                                          event.modifiers & Qt.ShiftModifier)
                        event.accepted = true
                    }
                }
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
