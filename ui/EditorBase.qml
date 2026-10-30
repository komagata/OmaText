import QtQuick
import QtCore

Item {
    id: plugin
    required property var io
    required property var treeIo
    property string omarchyPath: ""
    property var shell: null
    property var manifest: null
    readonly property bool opened: window.visible
    readonly property string pluginId: "io.github.komagata.omatext"

    // Only local file URLs are accepted; file contents use the normal document IO.
    function showEditor(payloadJson) {
        window.visible = true
        window.raise()
        window.requestActivate()
        let payload = null
        try { payload = JSON.parse(payloadJson) } catch (error) {}
        if (payload && !window.modalOpen && !backendDocument.busy) {
            if (typeof payload.folderUrl === "string") window.openFolderUrl(payload.folderUrl)
            else if (typeof payload.fileUrl === "string") {
                window.commitInput()
                backendDocument.requestFile(payload.fileUrl)
            }
        }
        Qt.callLater(function() { window.focusEditor() })
    }
    function inspectState(unused) { return JSON.stringify({opened: opened, active: window.active, editor: window.editorState(), length: backendDocument.text.length, url: backendDocument.url.toString(), modified: backendDocument.modified, busy: backendDocument.busy}) }
    function hideEditor() { if (window.visible) window.requestClose() }

    Settings {
        id: preferences
        location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) + "/omatext/editor.ini"
        property string fontFamily: ""
        property int fontSize: 0
        property bool sidebarVisible: false
    }
    Document { id: backendDocument; io: plugin.io }
    EditorView {
        id: window
        document: backendDocument
        treeIo: plugin.treeIo
        sidebarVisible: preferences.sidebarVisible
        onSidebarVisibilityChanged: function(value) { preferences.sidebarVisible = value; preferences.sync() }
        preferredFontFamily: preferences.fontFamily
        preferredFontSize: preferences.fontSize >= 8 && preferences.fontSize <= 96 ? preferences.fontSize : 0
        onPreferencesChanged: function(family, pixelSize) {
            preferences.fontFamily = family
            preferences.fontSize = pixelSize
            preferences.sync()
        }
        onFinished: {
            if (plugin.shell && typeof plugin.shell.hide === "function")
                plugin.shell.hide(plugin.pluginId)
        }
    }
}
