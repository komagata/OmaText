import QtQuick

EditorBase {
    io: FileIo {}
    function open(payloadJson) { showEditor(payloadJson) }
    function close() { hideEditor() }
}
