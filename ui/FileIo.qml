import QtQuick
import Quickshell.Io

QtObject {
    id: root
    property bool busy: false
    property string input: ""
    property bool timedOut: false
    signal completed(var result)
    function finish(result) {
        if (!busy) return
        timeout.stop()
        busy = false
        input = ""
        completed(result)
    }
    function request(message) {
        if (busy) return
        timedOut = false
        input = JSON.stringify(message) + "\n"
        busy = true
        timeout.start()
        process.running = true
    }
    property Process process: Process {
        command: ["python3", decodeURIComponent(Qt.resolvedUrl("../scripts/file_io.py").toString().replace(/^file:\/\//, ""))]
        stdinEnabled: true
        onStarted: write(root.input)
        stdout: StdioCollector { id: output; waitForEnd: true }
        stderr: StdioCollector { waitForEnd: true }
        onExited: function(code) {
            if (root.timedOut) {
                root.finish({ok: false, error: "The file operation timed out. Check the file before retrying."})
                return
            }
            try {
                if (code !== 0) throw new Error("helper failed")
                root.finish(JSON.parse(output.text))
            } catch (_) { root.finish({ok: false, error: "The file helper failed. Check that Python 3 is installed."}) }
        }
    }
    property Timer timeout: Timer {
        interval: 15000
        onTriggered: {
            // Keep the request occupied until exit; otherwise a late exit
            // could complete a newer request queued by a tree root change.
            root.timedOut = true
            process.running = false
        }
    }
}
