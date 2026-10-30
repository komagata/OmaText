import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import qs.Commons
import qs.Ui as Ui

Window {
    id: root
    required property var document
    required property var treeIo
    property real requestedSidebarWidth: Style.space(240)
    readonly property real minimumSidebarWidth: Style.space(160)
    readonly property real maximumSidebarWidth: Math.max(minimumSidebarWidth, root.width - Style.space(280))
    property bool sidebarVisible: false
    signal sidebarVisibilityChanged(bool value)
    onSidebarVisibleChanged: sidebarVisibilityChanged(sidebarVisible)
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
    title: (document.modified ? "● " : "") + "OmaText"
    readonly property string fileName: document.url.toString() ? decodeURIComponent(document.url.toString().split("/").pop()) : "Untitled"
    readonly property bool modalOpen: document.busy || unsaved.visible || errorDialog.visible || openDialog.visible || saveDialog.visible || settingsDialog.visible || folderDialog.visible
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
    function toggleSidebar() {
        if (modalOpen) return
        commitInput()
        sidebarVisible = !sidebarVisible
        if (sidebarVisible) {
            treeList.forceActiveFocus()
        } else focusEditor()
    }
    function openTreeFile(url) {
        if (modalOpen) return
        commitInput()
        document.requestFile(url)
        Qt.callLater(root.focusEditor)
    }
    function openFolderUrl(url) {
        if (modalOpen) return
        commitInput()
        tree.setRoot(url)
        sidebarVisible = true
        focusEditor()
    }
    function openFolder() {
        if (modalOpen) return
        commitInput()
        folderDialog.open()
    }
    FileTree { id: tree; io: root.treeIo; selectedUrl: root.document.url.toString() }
    Shortcut { sequence: "Ctrl+B"; enabled: !root.modalOpen; onActivated: root.toggleSidebar() }
    Shortcut { sequence: "Ctrl+Shift+O"; enabled: !root.modalOpen; onActivated: root.openFolder() }
    FolderDialog {
        id: folderDialog
        objectName: "folderDialog"
        title: "Open Folder"
        options: FolderDialog.DontUseNativeDialog
        onAccepted: root.openFolderUrl(selectedFolder)
        onRejected: root.focusEditor()
    }
    Item {
        id: sidebar
        objectName: "sidebar"
        visible: root.sidebarVisible
        width: Math.max(root.minimumSidebarWidth, Math.min(root.requestedSidebarWidth, root.maximumSidebarWidth))
        anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.left: parent.left
        enabled: !root.modalOpen
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Color.foreground; opacity: 0.25 }
        Text {
            x: Style.space(12); y: Style.space(14)
            width: parent.width - Style.space(56)
            text: "Files"; textFormat: Text.PlainText; color: Color.foreground
            font.family: Style.font.family; font.pixelSize: Style.font.body
        }
        Ui.Button {
            objectName: "openFolderButton"
            anchors.right: parent.right; anchors.rightMargin: Style.space(8); y: Style.space(8)
            width: Style.space(32); height: Style.space(32)
            iconText: "\uf114"; tooltipText: "Open folder (Ctrl+Shift+O)"
            Accessible.name: "Open folder"; focusable: true
            onClicked: root.openFolder()
        }
        Text {
            id: rootLabel
            x: Style.space(12); y: Style.space(52); width: parent.width - Style.space(24)
            text: tree.rootUrl ? decodeURIComponent(tree.rootUrl.replace(/\/$/, "").split("/").pop()) || "/" : "Open a folder to begin"
            textFormat: Text.PlainText; elide: Text.ElideMiddle; color: Color.foreground
            font.family: Style.font.family; font.pixelSize: Style.font.body
        }
        ListView {
            id: treeList
            objectName: "treeList"
            anchors.top: rootLabel.bottom; anchors.topMargin: Style.space(12)
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: treeStatus.top
            clip: true; model: tree.rows; currentIndex: -1
            activeFocusOnTab: true
            Accessible.name: "File tree"
            onCountChanged: currentIndex = count ? Math.max(-1, Math.min(currentIndex, count - 1)) : -1
            function activate() {
                if (currentIndex < 0 || currentIndex >= tree.rows.length) return
                const row = tree.rows[currentIndex]
                if (row.directory) tree.toggle(row.url)
                else root.openTreeFile(row.url)
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Down) { currentIndex = Math.min(count - 1, currentIndex + 1); event.accepted = true }
                else if (event.key === Qt.Key_Up) { currentIndex = count ? Math.max(0, currentIndex - 1) : -1; event.accepted = true }
                else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { activate(); event.accepted = true }
                else if (event.key === Qt.Key_Right && currentIndex >= 0 && currentIndex < count) {
                    const row = tree.rows[currentIndex]
                    if (row.directory && !row.expanded) tree.toggle(row.url)
                    else currentIndex = Math.min(count - 1, currentIndex + 1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Left && currentIndex >= 0 && currentIndex < count) {
                    const row = tree.rows[currentIndex]
                    if (row.directory && row.expanded) tree.toggle(row.url)
                    else for (let i = currentIndex - 1; i >= 0; i--) {
                        if (tree.rows[i].depth < row.depth) { currentIndex = i; break }
                    }
                    event.accepted = true
                }
                if (event.accepted && currentIndex >= 0 && currentIndex < count) positionViewAtIndex(currentIndex, ListView.Contain)
            }
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: treeList.width - 1; height: Style.space(34)
                objectName: "fileTreeRow" + index
                color: tree.selectedUrl === modelData.url
                    ? Style.selectionFillFor(Color.foreground, Color.accent) : "transparent"
                border.width: treeList.activeFocus && treeList.currentIndex === index ? 1 : 0
                border.color: Color.foreground
                Accessible.name: modelData.name
                Accessible.role: modelData.directory ? Accessible.TreeItem : Accessible.ListItem
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    x: Style.space(12) + modelData.depth * Style.space(16)
                    width: Style.space(16)
                    text: modelData.directory ? (modelData.expanded ? "▾" : "▸") : ""
                    textFormat: Text.PlainText; color: Color.foreground
                    font.family: Style.font.family; font.pixelSize: Style.font.body
                }
                Canvas {
                    id: entryIcon
                    objectName: "treeEntryIcon"
                    anchors.verticalCenter: parent.verticalCenter
                    x: Style.space(28) + modelData.depth * Style.space(16)
                    width: Style.space(16); height: Style.space(16)
                    property color ink: Color.foreground
                    onInkChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset(); ctx.scale(width / 16, height / 16)
                        ctx.strokeStyle = ink; ctx.lineWidth = 1
                        ctx.beginPath()
                        if (modelData.directory) {
                            ctx.moveTo(1.5, 13.5); ctx.lineTo(1.5, 2.5)
                            ctx.lineTo(6, 2.5); ctx.lineTo(8, 4.5)
                            ctx.lineTo(14.5, 4.5); ctx.lineTo(14.5, 13.5)
                        } else {
                            ctx.moveTo(3.5, 14.5); ctx.lineTo(3.5, 1.5)
                            ctx.lineTo(9, 1.5); ctx.lineTo(12.5, 5)
                            ctx.lineTo(12.5, 14.5)
                        }
                        ctx.closePath(); ctx.stroke()
                        if (!modelData.directory) {
                            ctx.beginPath(); ctx.moveTo(9, 1.5); ctx.lineTo(9, 5)
                            ctx.lineTo(12.5, 5); ctx.stroke()
                        }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    x: Style.space(52) + modelData.depth * Style.space(16)
                    width: parent.width - x - Style.space(8)
                    text: modelData.name
                    textFormat: Text.PlainText; elide: Text.ElideRight; color: Color.foreground
                    font.family: Style.font.family; font.pixelSize: Style.font.body
                }
                MouseArea { anchors.fill: parent; onClicked: { treeList.currentIndex = index; treeList.forceActiveFocus(); treeList.activate() } }
            }
        }
        Column {
            id: treeStatus
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.margins: Style.space(12)
            spacing: Style.space(8)
            Text {
                width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
                text: tree.loading ? "Loading…" : tree.error || (tree.truncated ? "Listing limited. Open a smaller folder." : "")
                color: Color.foreground; font.family: Style.font.family; font.pixelSize: Style.font.body
            }
            Ui.Button { visible: !!tree.error; text: "Retry"; focusable: true; Accessible.name: "Retry folder"; onClicked: tree.retry() }
        }
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
        anchors.leftMargin: root.sidebarVisible ? sidebar.width : 0
        anchors.bottomMargin: actions.height + Style.space(20)
        clip: true
        contentWidth: availableWidth
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AlwaysOff
        Controls.TextArea {
            id: editor
            objectName: "editor"
            focus: true
            readOnly: root.document.busy
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
    MouseArea {
        id: sidebarResizeHandle
        objectName: "sidebarResizeHandle"
        x: sidebar.width - 0.5 - width / 2
        width: Style.space(8)
        anchors.top: parent.top; anchors.bottom: parent.bottom
        z: 1
        visible: root.sidebarVisible
        enabled: visible && !root.modalOpen
        cursorShape: Qt.SizeHorCursor
        preventStealing: true
        property real pressX: 0
        property real pressWidth: 0
        onPressed: function(mouse) {
            root.commitInput()
            pressX = mapToItem(root.contentItem, mouse.x, mouse.y).x
            pressWidth = sidebar.width
        }
        onPositionChanged: function(mouse) {
            if (!pressed || !enabled) return
            const pointerX = mapToItem(root.contentItem, mouse.x, mouse.y).x
            root.requestedSidebarWidth = Math.max(root.minimumSidebarWidth,
                Math.min(pressWidth + pointerX - pressX, root.maximumSidebarWidth))
        }
    }
    Item {
        id: actions
        objectName: "actions"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Style.space(12)
        readonly property real buttonSize: Style.space(36)
        readonly property real gap: Style.space(4)
        readonly property bool expanded: actionHover.hovered || settingsButton.activeFocus
            || newButton.activeFocus || openButton.activeFocus || saveButton.activeFocus || sidebarToggle.activeFocus
            || settingsDialog.visible
        width: expanded ? buttonSize * 5 + gap * 4 : buttonSize
        height: buttonSize
        enabled: !root.modalOpen
        HoverHandler { id: actionHover }
        Row {
            anchors.right: settingsButton.left
            anchors.rightMargin: actions.gap
            spacing: actions.gap
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
            Ui.Button {
                id: sidebarToggle
                objectName: "sidebarToggle"
                visible: actions.expanded
                width: actions.buttonSize; height: actions.buttonSize
                iconText: "\uf0db"
                tooltipText: (root.sidebarVisible ? "Hide file tree" : "Show file tree") + " (Ctrl+B)"
                Accessible.name: tooltipText
                focusable: true
                onClicked: root.toggleSidebar()
            }
        }
        Ui.Button {
            id: settingsButton
            objectName: "settingsButton"
            anchors.right: parent.right
            width: actions.buttonSize; height: actions.buttonSize
            iconText: "\uf013"
            tooltipText: "Settings (Ctrl+,)"
            Accessible.name: "Settings"
            focusable: true
            onClicked: { root.commitInput(); settingsDialog.open() }
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
        function treeAction(openFolder) {
            // Leave font drafts unapplied, just as Cancel does.
            close()
            Qt.callLater(function() {
                if (openFolder) root.openFolder()
                else root.toggleSidebar()
            })
        }
        function apply() {
            root.preferredFontFamily = draftFontFamily
            root.preferredFontSize = draftFontSize
            root.preferencesChanged(draftFontFamily, draftFontSize)
            close()
        }
        contentItem: ColumnLayout {
            spacing: Style.space(12)
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
                spacing: Style.space(8)
                Ui.Button {
                    objectName: "settingsTreeButton"
                    text: root.sidebarVisible ? "Hide file tree" : "Show file tree"
                    tooltipText: "Toggle file tree (Ctrl+B)"
                    Accessible.name: text
                    focusable: true
                    onClicked: settingsDialog.treeAction(false)
                }
                Ui.Button {
                    objectName: "settingsFolderButton"
                    text: "Open folder"
                    tooltipText: "Open folder (Ctrl+Shift+O)"
                    Accessible.name: text
                    focusable: true
                    onClicked: settingsDialog.treeAction(true)
                }
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
        // Keep file selection inside Qt; GTK dialogs can abort the shared shell.
        options: FileDialog.DontUseNativeDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Text files (*.txt *.md *.text)", "All files (*)"]
        onAccepted: { document.acceptFile(selectedFile, false); editor.forceActiveFocus() }
        onRejected: { document.cancelFile(); editor.forceActiveFocus() }
    }
    FileDialog {
        id: saveDialog
        title: "Save As"
        options: FileDialog.DontUseNativeDialog
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
        objectName: "errorDialog"
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
