import QtQuick

QtObject {
    id: root
    required property var io
    property string text: ""
    property url url: ""
    property string error: ""
    readonly property bool modified: text !== savedText
    readonly property bool busy: io.busy
    property string savedText: ""
    property string pending: ""
    property string pendingFile: ""
    property var operation: null
    property bool crlf: false
    property bool bom: false
    signal confirmationRequested()
    signal fileDialogRequested(bool saving)
    signal closeReady()

    function clearError() { error = "" }
    function request(action, fileUrl = "") {
        if (busy || pending) return
        pendingFile = fileUrl
        pending = action
        if (modified) confirmationRequested()
        else proceed()
    }
    function requestNew() { request("new") }
    function requestOpen() { request("open") }
    function requestFile(fileUrl) {
        if (typeof fileUrl !== "string" || !fileUrl.startsWith("file:///")) return
        const normalized = Qt.resolvedUrl(fileUrl).toString()
        if (normalized === url.toString()) return
        request("open", normalized)
    }
    function requestClose() { request("close") }
    function proceed() {
        const action = pending
        const fileUrl = pendingFile
        pending = ""; pendingFile = ""
        if (action === "new") {
            url = ""; savedText = ""; text = ""; crlf = false; bom = false
        } else if (action === "open") {
            if (fileUrl) acceptFile(fileUrl, false)
            else fileDialogRequested(false)
        }
        else if (action === "close") closeReady()
    }
    function resolveUnsaved(choice) {
        if (choice === "save") save()
        else if (choice === "discard") proceed()
        else cancelFile()
    }
    function save() {
        if (busy) return
        if (!url.toString()) fileDialogRequested(true)
        else acceptFile(url, true)
    }
    function saveAs() { if (!busy) fileDialogRequested(true) }
    function cancelFile() { pending = ""; pendingFile = "" }
    function open(fileUrl) { acceptFile(fileUrl, false) }
    function acceptFile(fileUrl, saving) {
        if (busy) return
        operation = {url: fileUrl.toString(), saving: saving, text: text}
        io.request(saving
            ? {action: "write", url: operation.url, text: text, crlf: crlf, bom: bom}
            : {action: "read", url: operation.url})
    }
    property Connections completion: Connections {
        target: root.io
        function onCompleted(result) {
            const op = root.operation
            root.operation = null
            if (!op) return
            if (!result.ok) {
                root.cancelFile()
                root.error = result.error || "The file operation failed."
                return
            }
            root.clearError()
            root.url = op.url
            if (op.saving) {
                root.savedText = op.text
                root.proceed()
            } else {
                root.crlf = result.crlf; root.bom = result.bom
                root.savedText = result.text; root.text = result.text
            }
        }
    }
}
