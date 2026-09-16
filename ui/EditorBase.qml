import QtQuick
import QtCore

Item {
    id: plugin
    required property var io
    property string omarchyPath: ""
    property var shell: null
    property var manifest: null
    readonly property bool opened: window.visible
    readonly property string pluginId: "io.github.komagata.omatext"

    // No payload-controlled paths or code. Repeated summons preserve the buffer.
    function showEditor(payloadJson) {
        window.visible = true
        window.raise()
        window.requestActivate()
        Qt.callLater(function() { window.focusEditor() })
    }
    function inspectState(unused) { return JSON.stringify({opened: opened, active: window.active, editor: window.editorState(), length: backendDocument.text.length}) }
    function hideEditor() { if (window.visible) window.requestClose() }

    Settings {
        id: preferences
        location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) + "/omatext/editor.ini"
        property string fontFamily: ""
        property int fontSize: 0
    }
    Document { id: backendDocument; io: plugin.io }
    EditorView {
        id: window
        document: backendDocument
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
