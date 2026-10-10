pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Lists the keyboard shortcuts: those of the actions in menuBar, read from it
// each time the dialog opens so the list can't fall out of date, followed by
// the keys that aren't in a menu.
Dialog {
    id: dialog

    required property MenuBar menuBar

    title: qsTr("Keyboard Shortcuts")
    modal: true
    standardButtons: Dialog.Close
    width: 440
    height: Math.min(implicitHeight, parent.height * 0.85)

    // The menu text without the & that marks its mnemonic.
    function plainText(text) {
        return text.replace(/&(.)/g, "$1")
    }

    function addMenu(menu, section, rows) {
        for (let i = 0; i < menu.count; ++i) {
            const subMenu = menu.menuAt(i)
            if (subMenu) {
                addMenu(subMenu, section, rows)
                continue
            }
            const action = menu.actionAt(i)
            if (action && action.shortcut)
                rows.push({ section: section, name: plainText(action.text), sequence: action.shortcut, keys: "" })
        }
    }

    onAboutToShow: {
        const rows = []
        for (let i = 0; i < menuBar.count; ++i) {
            const menu = menuBar.menuAt(i)
            addMenu(menu, plainText(menu.title), rows)
        }

        // Keys handled by the editor and note list themselves. Each has a key
        // sequence, which is shown in the platform's style, or text describing
        // what to type.
        const editing = qsTr("Editing")
        rows.push({ section: editing, name: qsTr("Start a bulleted list"), sequence: "", keys: qsTr("* or - then Space") })
        rows.push({ section: editing, name: qsTr("Start a numbered list"), sequence: "", keys: qsTr("1. then Space") })
        rows.push({ section: editing, name: qsTr("Indent list item"), sequence: "Tab", keys: "" })
        rows.push({ section: editing, name: qsTr("Outdent list item"), sequence: "Shift+Tab", keys: "" })
        rows.push({ section: editing, name: qsTr("Outdent or end the list, on an empty item"), sequence: "Return", keys: "" })
        const noteList = qsTr("Note List")
        rows.push({ section: noteList, name: qsTr("Delete the selected note"), sequence: "Delete", keys: "" })

        list.model = rows
    }

    contentItem: ListView {
        id: list
        implicitHeight: contentHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        section.property: "section"
        section.delegate: Label {
            required property string section
            width: ListView.view.width
            topPadding: 12
            bottomPadding: 4
            text: section
            font.bold: true
        }

        delegate: RowLayout {
            id: row
            required property var modelData
            width: ListView.view.width
            spacing: 16

            // Only used to get the platform's text for the key sequence, e.g.
            // ⌘B on macOS rather than Ctrl+B. A standard key can have several
            // sequences, which sequence doesn't accept; this shows the first.
            Shortcut {
                id: shortcut
                sequences: row.modelData.sequence ? [row.modelData.sequence] : []
                enabled: false
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 3
                Layout.bottomMargin: 3
                text: row.modelData.name
                wrapMode: Text.Wrap
            }
            Label {
                text: row.modelData.keys || shortcut.nativeText
                opacity: 0.7
            }
        }
    }
}
