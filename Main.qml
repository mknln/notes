pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // Notes are tracked by id rather than row, because editing a note moves
    // it to the top of the list.
    property string currentId: notesModel.idAt(0)

    // The window is sized and shown in Component.onCompleted.
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

    function noteCount(count) {
        return count === 1 ? qsTr("1 note") : qsTr("%1 notes").arg(count)
    }

    function showMessage(title, text) {
        messageDialog.title = title
        messageDialog.text = text
        messageDialog.open()
    }

    function exportNotes() {
        exportDialog.selectedFile = exportDialog.currentFolder + "/"
                + qsTr("Notes %1.zip").arg(Qt.formatDate(new Date(), "yyyy-MM-dd"))
        exportDialog.open()
    }

    function importNotes(file) {
        const result = notesModel.importNotes(file)
        if (result.error) {
            showMessage(qsTr("Import Failed"), result.error)
            return
        }
        if (!currentId)
            currentId = notesModel.idAt(0)
        const lines = []
        if (result.added > 0)
            lines.push(qsTr("%1 added").arg(noteCount(result.added)))
        if (result.updated > 0)
            lines.push(qsTr("%1 updated").arg(noteCount(result.updated)))
        if (result.skipped > 0)
            lines.push(qsTr("%1 already up to date").arg(noteCount(result.skipped)))
        showMessage(qsTr("Import Complete"), lines.join("\n"))
    }

    // Moves to the note `step` rows away from the current one in the list, if
    // there is one, scrolling the list to show it.
    function selectAdjacentNote(step) {
        const row = notesModel.indexOf(currentId) + step
        if (row < 0 || row >= notesModel.count)
            return
        currentId = notesModel.idAt(row)
        noteList.positionViewAtIndex(row, ListView.Contain)
    }

    // Zooms the editor's text by `step`, e.g. 0.1 for 10% bigger, within the
    // zoom slider's range. Rounded to the slider's steps, so repeated steps
    // don't drift.
    function zoomBy(step) {
        const zoom = Math.round((settings.zoom + step) * 10) / 10
        settings.zoom = Math.max(zoomSlider.from, Math.min(zoomSlider.to, zoom))
    }

    function isPinned(id) {
        return notesModel.pinnedIds.includes(id)
    }

    function deleteNote(id) {
        const row = notesModel.indexOf(id)
        notesModel.removeNote(id)
        if (id === currentId)
            currentId = notesModel.idAt(Math.min(row, notesModel.count - 1))
    }

    // Set while a note's text is put into the editor, so that doesn't count
    // as an edit.
    property bool loadingNote: false
    // Set once the window is complete. Notes are only loaded after that: the
    // editor doesn't take its text until it's complete, and reports it as a
    // change when it does.
    property bool ready: false
    // The editor's Markdown when it last changed. The editor also reports a
    // change when it lays the text out again, e.g. when its width changes, and
    // its Markdown can differ from the saved file (e.g. in list spacing), so
    // comparing with the saved text would count that as an edit.
    property string editorMarkdown

    function loadNote() {
        loadingNote = true
        editor.text = notesModel.text(currentId)
        if (formatter.prepareLoadedText()) {
            // Works around the editor not drawing the reformatted line (or
            // the cursor at its new height) after its text was replaced:
            // changing the padding makes it lay everything out again.
            editor.leftPadding += 1
            editor.leftPadding -= 1
        }
        // Otherwise the format at the cursor in the previous note (e.g. a
        // heading) carries over into this one.
        formatter.resetTypingFormat()
        loadingNote = false
        editorMarkdown = formatter.markdown()
    }

    function saveEditorText() {
        if (!ready || !currentId || loadingNote)
            return
        const markdown = formatter.markdown()
        if (markdown === editorMarkdown)
            return
        editorMarkdown = markdown
        notesModel.setText(currentId, markdown)
    }

    onCurrentIdChanged: if (ready) loadNote()
    Component.onCompleted: {
        // A share of the screen, which comes to about 1100x700 on a 1920x1080
        // screen, so the window looks the same size on screens of any
        // resolution. It's set here rather than bound to the screen's size, so
        // moving the window to another screen doesn't resize it.
        width = Math.round(Screen.width * 0.57)
        height = Math.round(Screen.height * 0.65)
        ready = true
        loadNote()
        visible = true
    }

    NotesModel {
        id: notesModel
        onNoteReplaced: id => {
            if (id === window.currentId)
                window.loadNote()
        }
    }

    // Stored in a file of its own: the default location needs an
    // organization name, and setting one would also move the notes.
    Settings {
        id: settings
        location: StandardPaths.writableLocation(StandardPaths.AppConfigLocation) + "/settings.ini"
        property bool use24HourTime: false
        // Of the editor's text, where 1 is the system font's size.
        property real zoom: 1
    }

    TextFormatter {
        id: formatter
        document: editor.textDocument
        selectionStart: editor.selectionStart
        selectionEnd: editor.selectionEnd
        // The first line is the note's title.
        titleHeadingLevel: 2
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
        id: strikeOutAction
        text: qsTr("&Strikethrough")
        shortcut: "Ctrl+Shift+X"
        enabled: editor.enabled
        checkable: true
        checked: formatter.strikeOut
        onTriggered: formatter.strikeOut = checked
    }
    Action {
        id: bulletListAction
        text: qsTr("B&ulleted List")
        shortcut: "Ctrl+Shift+8"
        enabled: editor.enabled && !formatter.onTitleLine
        checkable: true
        checked: formatter.listStyle === TextFormatter.BulletList
        onTriggered: formatter.listStyle = checked ? TextFormatter.BulletList : TextFormatter.NoList
    }
    Action {
        id: numberedListAction
        text: qsTr("&Numbered List")
        shortcut: "Ctrl+Shift+7"
        enabled: editor.enabled && !formatter.onTitleLine
        checkable: true
        checked: formatter.listStyle === TextFormatter.NumberedList
        onTriggered: formatter.listStyle = checked ? TextFormatter.NumberedList : TextFormatter.NoList
    }

    menuBar: MenuBar {
        id: appMenuBar
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&New Note")
                shortcut: StandardKey.New
                onTriggered: window.newNote()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Import Notes…")
                onTriggered: importDialog.open()
            }
            Action {
                text: qsTr("&Export Notes…")
                enabled: notesModel.count > 0
                onTriggered: window.exportNotes()
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
                text: window.isPinned(window.currentId) ? qsTr("Un&pin Note") : qsTr("&Pin Note")
                enabled: window.currentId !== ""
                onTriggered: notesModel.setPinned(window.currentId, !window.isPinned(window.currentId))
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Delete Note")
                enabled: window.currentId !== ""
                onTriggered: window.requestDelete(window.currentId)
            }
            MenuSeparator {}
            // Left to the note list while it has focus.
            Action {
                text: qsTr("Pre&vious Note")
                shortcut: "Ctrl+PgUp"
                enabled: !noteList.activeFocus
                onTriggered: window.selectAdjacentNote(-1)
            }
            Action {
                text: qsTr("Ne&xt Note")
                shortcut: "Ctrl+PgDown"
                enabled: !noteList.activeFocus
                onTriggered: window.selectAdjacentNote(1)
            }
        }
        Menu {
            title: qsTr("&View")
            Action {
                text: qsTr("Zoom &In")
                shortcut: StandardKey.ZoomIn
                onTriggered: window.zoomBy(0.1)
            }
            Action {
                text: qsTr("Zoom &Out")
                shortcut: StandardKey.ZoomOut
                onTriggered: window.zoomBy(-0.1)
            }
            Action {
                text: qsTr("&Actual Size")
                enabled: settings.zoom !== 1
                onTriggered: settings.zoom = 1
            }
        }
        Menu {
            title: qsTr("F&ormat")
            MenuItem { action: boldAction }
            MenuItem { action: italicAction }
            MenuItem { action: strikeOutAction }
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
                        enabled: editor.enabled && !formatter.onTitleLine
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
        Menu {
            title: qsTr("&Insert")
            Action {
                text: qsTr("&Date")
                shortcut: "F2"
                enabled: editor.enabled
                // e.g. [11/22/26 - 11:38:02pm], or [11/22/26 - 23:38:02]
                onTriggered: {
                    const format = settings.use24HourTime ? "M/d/yy - HH:mm:ss" : "M/d/yy - h:mm:ss AP"
                    formatter.insertText("[" + Qt.formatDateTime(new Date(), format) + "]")
                    editor.forceActiveFocus()
                }
            }
            MenuSeparator {}
            Action {
                text: qsTr("Use &24-Hour Time")
                checkable: true
                checked: settings.use24HourTime
                onTriggered: settings.use24HourTime = checked
            }
        }
        Menu {
            title: qsTr("&Help")
            Action {
                text: qsTr("&Keyboard Shortcuts")
                shortcut: "Ctrl+/"
                onTriggered: shortcutsDialog.open()
            }
        }
    }

    // The status bar, below the note list and the editor. Like the formatting
    // toolbar, its controls don't take focus.
    footer: ToolBar {
        position: ToolBar.Footer

        RowLayout {
            anchors.fill: parent
            anchors.rightMargin: 8
            spacing: 2

            Item {
                Layout.fillWidth: true
            }
            ToolButton {
                text: "−"
                focusPolicy: Qt.NoFocus
                autoRepeat: true
                enabled: settings.zoom > zoomSlider.from
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Zoom out")
                onClicked: window.zoomBy(-0.1)
            }
            Slider {
                id: zoomSlider
                Layout.preferredWidth: 140
                from: 0.5
                to: 3
                stepSize: 0.1
                snapMode: Slider.SnapAlways
                focusPolicy: Qt.NoFocus
                value: settings.zoom
                onMoved: settings.zoom = Math.round(value * 10) / 10
            }
            ToolButton {
                text: "+"
                focusPolicy: Qt.NoFocus
                autoRepeat: true
                enabled: settings.zoom < zoomSlider.to
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Zoom in")
                onClicked: window.zoomBy(0.1)
            }
            ToolButton {
                // Wide enough for any zoom, so the slider doesn't
                // move as the text changes.
                Layout.preferredWidth: zoomWidth.width + leftPadding + rightPadding
                text: qsTr("%1%").arg(Math.round(settings.zoom * 100))
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reset to 100%")
                onClicked: settings.zoom = 1

                TextMetrics {
                    id: zoomWidth
                    text: qsTr("%1%").arg(300)
                }
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
                        text: window.noteCount(notesModel.count)
                        opacity: 0.7
                    }
                    ToolButton {
                        text: "+"
                        // Relative to the system font, so it scales with the
                        // rest of the interface.
                        font.pointSize: window.font.pointSize * 1.35
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
                // Clicking a note gives the list focus, so this only applies
                // while the list is being used, not while typing in the editor.
                Keys.onDeletePressed: if (window.currentId) window.requestDelete(window.currentId)

                delegate: NoteListDelegate {
                    selected: noteId === window.currentId
                    onClicked: {
                        window.currentId = noteId
                        noteList.forceActiveFocus()
                    }
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
                        enabled: !formatter.onTitleLine
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
                    ToolButton {
                        action: strikeOutAction
                        text: "S"
                        font.strikeout: true
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Strikethrough (%1)").arg(strikeOutAction.shortcut)
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
                    // Headings are sized relative to this, so they zoom too.
                    font.pointSize: window.font.pointSize * settings.zoom
                    padding: 16
                    // An empty note can still have formatting, such as a list
                    // bullet, which the placeholder would overlap. The title
                    // line is always a heading, so it still shows there.
                    placeholderText: !enabled ? qsTr("No note selected. Press + to create one.")
                                   : formatter.listStyle === TextFormatter.NoList
                                     && (formatter.headingLevel === 0 || formatter.onTitleLine) ? qsTr("Start typing…")
                                   : ""
                    onTextChanged: window.saveEditorText()
                    Keys.onReturnPressed: event => event.accepted = formatter.handleReturn()
                    Keys.onEnterPressed: event => event.accepted = formatter.handleReturn()
                    Keys.onSpacePressed: event => event.accepted = formatter.handleSpace()
                    Keys.onTabPressed: event => event.accepted = formatter.shiftListLevel(1)
                    Keys.onBacktabPressed: event => event.accepted = formatter.shiftListLevel(-1)
                    Keys.onPressed: event => {
                        if (event.key !== Qt.Key_PageUp && event.key !== Qt.Key_PageDown)
                            return
                        editorScroll.page(event.key === Qt.Key_PageUp ? -1 : 1,
                                          event.modifiers & Qt.ShiftModifier)
                        event.accepted = true
                    }

                    // Ctrl+wheel zooms a step per notch of a mouse wheel. A
                    // touchpad scrolls in smaller amounts, which add up.
                    WheelHandler {
                        property real notches: 0
                        target: null
                        acceptedModifiers: Qt.ControlModifier
                        onWheel: event => {
                            notches += event.angleDelta.y / 120
                            const steps = Math.trunc(notches)
                            if (steps !== 0) {
                                notches -= steps
                                window.zoomBy(steps * 0.1)
                            }
                        }
                    }
                }
            }

        }
    }

    Menu {
        id: noteMenu
        property string noteId

        MenuItem {
            text: window.isPinned(noteMenu.noteId) ? qsTr("Unpin Note") : qsTr("Pin Note")
            onTriggered: notesModel.setPinned(noteMenu.noteId, !window.isPinned(noteMenu.noteId))
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete Note")
            onTriggered: window.requestDelete(noteMenu.noteId)
        }
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Export Notes")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Zip files (*.zip)")]
        defaultSuffix: "zip"
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: {
            const error = notesModel.exportNotes(selectedFile)
            if (error)
                window.showMessage(qsTr("Export Failed"), error)
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import Notes")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Zip files (*.zip)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: window.importNotes(selectedFile)
    }

    MessageDialog {
        id: messageDialog
        buttons: MessageDialog.Ok
    }

    // Ctrl++ needs Shift on many keyboards, so Ctrl+= zooms in too, as in
    // most apps.
    Shortcut {
        sequence: "Ctrl+="
        onActivated: window.zoomBy(0.1)
    }

    ShortcutsDialog {
        id: shortcutsDialog
        menuBar: appMenuBar
        anchors.centerIn: Overlay.overlay
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
