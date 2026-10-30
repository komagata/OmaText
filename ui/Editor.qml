import QtQuick

EditorBase {
    io: FileIo {}
    treeIo: FileIo {}
    function open(payloadJson) { showEditor(payloadJson) }
    function close() { hideEditor() }
}
