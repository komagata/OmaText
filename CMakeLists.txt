cmake_minimum_required(VERSION 3.21)
project(omatext VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
enable_testing()

# Loaded in-process by Omarchy. No standalone executable or copied shell UI.
qt_add_library(omatext-backend SHARED)
qt_add_qml_module(omatext-backend
    URI OmaText.Backend VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/OmaText/Backend"
    SOURCES src/document.h src/document.cpp)
target_include_directories(omatext-backend PRIVATE src)
target_link_libraries(omatext-backend PRIVATE Qt6::Core Qt6::Qml)
# Keep the backing library next to its QML plugin, relocatable on installation.
set_target_properties(omatext-backend PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/OmaText/Backend")
set_target_properties(omatext-backendplugin PROPERTIES BUILD_RPATH "$ORIGIN")

add_library(editor-core STATIC src/document.cpp src/document.h)
target_link_libraries(editor-core PUBLIC Qt6::Core Qt6::Qml)
target_include_directories(editor-core PUBLIC src)
add_executable(document-test tests/document_test.cpp)
target_link_libraries(document-test PRIVATE editor-core Qt6::Test)
add_test(NAME document COMMAND document-test)

# Test-only adapters allow UI tests without starting a second Quickshell process.
add_executable(ui-test tests/ui_test.cpp tests/support/theme.cpp tests/support/theme.h)
target_include_directories(ui-test PRIVATE tests/support)
target_link_libraries(ui-test PRIVATE editor-core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test)
file(GLOB_RECURSE TEST_UI CONFIGURE_DEPENDS tests/support/vendor/qs/*)
qt_add_resources(ui-test test-ui PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ui/EditorView.qml ${TEST_UI})
add_test(NAME ui COMMAND ui-test)
set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QPA_PLATFORMTHEME=")

add_dependencies(ui-test omatext-backendplugin)
target_compile_definitions(ui-test PRIVATE SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
