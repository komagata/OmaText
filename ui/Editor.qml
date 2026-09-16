import QtQuick
import QtCore
import "../lib/OmaText/Backend" as Backend

Item {
    id: plugin
    property string omarchyPath: ""
    property var shell: null
    property var manifest: null
    readonly property bool opened: window.visible
    readonly property string pluginId: "io.github.komagata.omatext"

    // No payload-controlled paths or code. Repeated summons preserve the buffer.
    function open(payloadJson) {
        window.visible = true
        window.raise()
        window.requestActivate()
        Qt.callLater(function() { window.focusEditor() })
    }
    function inspectState(unused) { return JSON.stringify({opened: opened, active: window.active, editor: window.editorState(), length: backendDocument.text.length}) }
    function close() { if (window.visible) window.requestClose() }

    Settings {
        id: preferences
        location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) + "/omatext/editor.ini"
        property string fontFamily: ""
        property int fontSize: 0
    }
    Backend.Document { id: backendDocument }
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
