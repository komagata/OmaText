import QtQuick

QtObject {
    id: root
    objectName: "fileTree"
    required property var io
    property string rootUrl: ""
    property string selectedUrl: ""
    property var rows: []
    property var directories: ({})
    property int generation: 0
    property var operation: null
    property string queuedRoot: ""
    readonly property bool loading: io.busy
    property string error: ""
    property bool truncated: false
    property int cachedEntries: 0

    function setRoot(url) {
        generation++
        rootUrl = url.toString()
        directories = ({})
        rows = []; error = ""; truncated = false; cachedEntries = 0
        queuedRoot = rootUrl
        pump()
    }
    function pump() {
        if (!io.busy && queuedRoot) {
            const url = queuedRoot; queuedRoot = ""
            load(url)
        }
    }
    function load(url) {
        if (io.busy) return
        if (cachedEntries >= 4096) { error = "Tree limit reached. Open a smaller folder."; return }
        operation = {url: url, generation: generation}
        io.request({action: "list", url: url})
    }
    function toggle(url) {
        const node = directories[url]
        if (!node || node.error) { load(url); return }
        node.expanded = !node.expanded
        rebuild()
    }
    function rebuild() {
        const result = []
        function append(url, depth) {
            const node = directories[url]
            if (!node || !node.expanded) return
            for (const entry of node.entries) {
                const child = directories[entry.url]
                result.push({name: entry.name, url: entry.url, directory: entry.directory,
                    depth: depth, expanded: !!child && child.expanded, error: child ? child.error : ""})
                if (entry.directory && child && child.expanded) append(entry.url, depth + 1)
            }
        }
        append(rootUrl, 0)
        rows = result
    }
    property Connections completion: Connections {
        target: root.io
        function onCompleted(result) {
            const op = root.operation; root.operation = null
            if (op && op.generation === root.generation) {
                if (!result.ok) {
                    root.error = result.error || "Could not open the folder."
                    root.directories[op.url] = {entries: [], expanded: false, error: root.error}
                } else {
                    root.error = ""
                    const entries = result.entries.slice(0, 4096 - root.cachedEntries)
                    root.cachedEntries += entries.length
                    root.truncated = root.truncated || result.truncated || entries.length < result.entries.length
                    for (const entry of entries) entry.url = Qt.resolvedUrl(entry.url).toString()
                    root.directories[op.url] = {entries: entries, expanded: true, error: ""}
                }
                root.rebuild()
            }
            root.pump()
        }
    }
    function retry() {
        if (operation || io.busy) return
        for (const url in directories) {
            if (directories[url].error) { load(url); return }
        }
        if (rootUrl) load(rootUrl)
    }
}
