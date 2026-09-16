import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import qs.Commons
import qs.Ui as Ui

Window {
    id: root
    required property var document
    signal finished()
    signal preferencesChanged(string family, int pixelSize)
    property string preferredFontFamily: ""
    property int preferredFontSize: 0
    readonly property string effectiveFontFamily: (settingsDialog.visible ? settingsDialog.draftFontFamily : preferredFontFamily) || Style.font.family
    readonly property int effectiveFontSize: (settingsDialog.visible ? settingsDialog.draftFontSize : preferredFontSize) || Style.font.heading
    width: 960; height: 720
    minimumWidth: 520; minimumHeight: 360
    transientParent: null
    visible: false
    color: Color.background
    title: (document.modified ? "● " : "") + fileName + " — OmaText"
    readonly property string fileName: document.url.toString() ? decodeURIComponent(document.url.toString().split("/").pop()) : "Untitled"
    readonly property bool modalOpen: unsaved.visible || errorDialog.visible || openDialog.visible || saveDialog.visible || settingsDialog.visible
    onClosing: function(close) {
        close.accepted = false
        requestClose()
    }
    onActiveChanged: if (active) Qt.callLater(root.focusEditor)
    function editorState() { return {focus: editor.focus, activeFocus: editor.activeFocus, enabled: editor.enabled, visible: editor.visible, modalOpen: modalOpen, actionsExpanded: actions.expanded, settingsOpen: settingsDialog.visible, fontFamily: editor.font.family, fontSize: editor.font.pixelSize} }
    function focusEditor() {
        if (unsaved.visible) unsaved.forceActiveFocus()
        else if (errorDialog.visible) errorDialog.forceActiveFocus()
        else if (settingsDialog.visible) settingsDialog.forceActiveFocus()
        else if (!modalOpen) editor.forceActiveFocus()
    }
    function requestClose() { if (!modalOpen) { commitInput(); document.requestClose() } }
    function commitInput() { Qt.inputMethod.commit() }
    function act(action) {
        commitInput()
        if (action === "new") document.requestNew()
        else if (action === "open") document.requestOpen()
        else if (action === "save") document.save()
        else if (action === "saveAs") document.saveAs()
        Qt.callLater(root.focusEditor)
    }
    Shortcut { sequence: "Ctrl+,"; enabled: !root.modalOpen; onActivated: { root.commitInput(); settingsDialog.open() } }
    Shortcut { sequence: "Ctrl+N"; enabled: !root.modalOpen; onActivated: root.act("new") }
    Shortcut { sequence: "Ctrl+O"; enabled: !root.modalOpen; onActivated: root.act("open") }
    Shortcut { sequence: "Ctrl+S"; enabled: !root.modalOpen; onActivated: root.act("save") }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !root.modalOpen; onActivated: root.act("saveAs") }
    Shortcut { sequence: "Ctrl+Q"; enabled: !root.modalOpen; onActivated: { root.commitInput(); document.requestClose() } }

    Controls.ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.bottomMargin: actions.height + Style.space(20)
        clip: true
        contentWidth: availableWidth
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AlwaysOff
        Controls.TextArea {
            id: editor
            objectName: "editor"
            focus: true
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            persistentSelection: true
            color: Color.foreground
            selectionColor: Style.selectionFillFor(Color.foreground, Color.accent)
            selectedTextColor: Color.foreground
            font.family: root.effectiveFontFamily
            font.pixelSize: root.effectiveFontSize
            leftPadding: Style.space(28); rightPadding: Style.space(28)
            topPadding: Style.space(24); bottomPadding: Style.space(24)
            background: null
            onTextChanged: if (root.document.text !== text) root.document.text = text
            Component.onCompleted: text = root.document.text
            Accessible.name: "Editor"
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Tab && event.modifiers === Qt.NoModifier) {
                    insert(cursorPosition, "\t"); event.accepted = true
                }
            }
        }
    }
    Item {
        id: actions
        objectName: "actions"
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: Style.space(12)
        readonly property real buttonSize: Style.space(36)
        readonly property real gap: Style.space(4)
        readonly property bool expanded: actionHover.hovered || settingsButton.activeFocus
            || newButton.activeFocus || openButton.activeFocus || saveButton.activeFocus
            || settingsDialog.visible
        width: expanded ? buttonSize * 4 + gap * 3 : buttonSize
        height: buttonSize
        enabled: !root.modalOpen
        HoverHandler { id: actionHover }
        Row {
            spacing: actions.gap
            Ui.Button {
                id: settingsButton
                objectName: "settingsButton"
                width: actions.buttonSize; height: actions.buttonSize
                iconText: "\uf013"
                tooltipText: "Settings (Ctrl+,)"
                Accessible.name: "Settings"
                focusable: true
                onClicked: { root.commitInput(); settingsDialog.open() }
            }
            Ui.Button {
                id: newButton
                objectName: "newButton"
                visible: actions.expanded
                width: actions.buttonSize; height: actions.buttonSize
                iconText: "\uf016"
                tooltipText: "New (Ctrl+N)"
                Accessible.name: "New"
                focusable: true
                onClicked: root.act("new")
            }
            Ui.Button {
                id: openButton
                objectName: "openButton"
                visible: actions.expanded
                width: actions.buttonSize; height: actions.buttonSize
                iconText: "\uf07c"
                tooltipText: "Open (Ctrl+O)"
                Accessible.name: "Open"
                focusable: true
                onClicked: root.act("open")
            }
            Ui.Button {
                id: saveButton
                objectName: "saveButton"
                visible: actions.expanded
                width: actions.buttonSize; height: actions.buttonSize
                iconText: "\uf0c7"
                tooltipText: "Save (Ctrl+S)"
                Accessible.name: "Save"
                focusable: true
                onClicked: root.act("save")
            }
        }
    }
    Controls.Popup {
        id: settingsDialog
        objectName: "settingsDialog"
        anchors.centerIn: parent
        width: Math.min(root.width - Style.space(40), Style.space(440))
        padding: Style.space(24)
        modal: true; focus: true
        closePolicy: Controls.Popup.CloseOnEscape
        property string draftFontFamily: ""
        property int draftFontSize: 0
        background: Ui.BorderSurface { color: Color.background; borderSpec: Border.flat(Color.foreground, 1); radius: 0 }
        onOpened: {
            draftFontFamily = root.preferredFontFamily
            draftFontSize = root.preferredFontSize
            fontPicker.options = [{value: "", label: "Omarchy default"}].concat(Qt.fontFamilies().map(function(family) { return {value: family, label: family} }))
            fontPicker.value = draftFontFamily
            sizePicker.value = String(draftFontSize)
        }
        onClosed: { fontPicker.close(); sizePicker.close(); root.focusEditor() }
        function apply() {
            root.preferredFontFamily = draftFontFamily
            root.preferredFontSize = draftFontSize
            root.preferencesChanged(draftFontFamily, draftFontSize)
            close()
        }
        contentItem: ColumnLayout {
            spacing: Style.space(20)
            Text {
                textFormat: Text.PlainText
                text: "Settings"
                color: Color.foreground
                font.family: Style.font.family; font.pixelSize: Style.font.heading
            }
            Ui.SearchableDropdown {
                id: fontPicker
                objectName: "fontPicker"
                Layout.fillWidth: true
                label: "Font"
                placeholderText: "Search fonts"
                emptyText: "No fonts found"
                popupMinHeight: Style.space(180)
                onChanged: function(value) { settingsDialog.draftFontFamily = value }
            }
            Ui.Dropdown {
                id: sizePicker
                objectName: "sizePicker"
                Layout.fillWidth: true
                label: "Font size"
                options: [{value: "0", label: "Omarchy default"}].concat([12,14,16,18,20,22,24,28,32,36,48,64].map(function(size) { return {value: String(size), label: size + " px"} }))
                onChanged: function(value) { settingsDialog.draftFontSize = Number(value) }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Ui.Button { objectName: "cancelSettings"; iconText: "\uf00d"; tooltipText: "Cancel"; Accessible.name: "Cancel"; width: Style.space(36); height: Style.space(36); focusable: true; onClicked: settingsDialog.close() }
                Ui.Button { objectName: "applySettings"; iconText: "\uf00c"; tooltipText: "Apply"; Accessible.name: "Apply"; width: Style.space(36); height: Style.space(36); focusable: true; bordered: true; onClicked: settingsDialog.apply() }
            }
        }
    }
    Connections {
        target: document
        function onTextChanged() { if (editor.text !== document.text) editor.text = document.text }
        function onConfirmationRequested() { unsaved.open() }
        function onFileDialogRequested(saving) {
            if (saving) { saveDialog.selectedFile = document.url; saveDialog.open() }
            else openDialog.open()
        }
        function onCloseReady() { root.visible = false; root.finished() }
        function onErrorChanged() { if (document.error) errorDialog.open() }
    }
    FileDialog {
        id: openDialog
        title: "Open File"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Text files (*.txt *.md *.text)", "All files (*)"]
        onAccepted: { document.acceptFile(selectedFile, false); editor.forceActiveFocus() }
        onRejected: { document.cancelFile(); editor.forceActiveFocus() }
    }
    FileDialog {
        id: saveDialog
        title: "Save As"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Text files (*.txt)", "All files (*)"]
        onAccepted: { document.acceptFile(selectedFile, true); editor.forceActiveFocus() }
        onRejected: { document.cancelFile(); editor.forceActiveFocus() }
    }
    Controls.Popup {
        id: unsaved
        objectName: "unsavedDialog"
        anchors.centerIn: parent
        width: Math.min(root.width - 40, 480)
        padding: Style.space(24)
        modal: true; focus: true
        closePolicy: Controls.Popup.CloseOnEscape
        background: Ui.BorderSurface { color: Color.background; borderSpec: Border.flat(Color.foreground, 1); radius: 0 }
        property bool chosen: false
        onOpened: chosen = false
        onClosed: { if (!chosen) document.resolveUnsaved("cancel"); editor.forceActiveFocus() }
        function choose(choice) { chosen = true; close(); document.resolveUnsaved(choice) }
        contentItem: ColumnLayout {
            spacing: Style.space(20)
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: "Save changes?"; color: Color.foreground
                font.family: Style.font.family; font.pixelSize: Style.font.heading
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: root.fileName + " has unsaved changes."
                wrapMode: Text.Wrap
                color: Color.foreground; font.family: Style.font.family; font.pixelSize: Style.font.body
            }
            RowLayout {
                spacing: Style.space(8)
                Ui.Button { text: "Cancel"; focusable: true; onClicked: unsaved.choose("cancel") }
                Ui.Button { text: "Don't Save"; focusable: true; onClicked: unsaved.choose("discard") }
                Ui.Button { text: "Save"; focusable: true; bordered: true; onClicked: unsaved.choose("save") }
            }
        }
    }
    Controls.Popup {
        id: errorDialog
        anchors.centerIn: parent; width: Math.min(root.width - 40, 520)
        padding: Style.space(24); modal: true; focus: true
        background: Ui.BorderSurface { color: Color.background; borderSpec: Border.flat(Color.urgent, 1); radius: 0 }
        onClosed: { document.clearError(); editor.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: Style.space(20)
            Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: document.error; wrapMode: Text.Wrap; color: Color.foreground; font.family: Style.font.family; font.pixelSize: Style.font.body }
            Ui.Button { text: "Close"; focusable: true; onClicked: errorDialog.close() }
        }
    }
}
