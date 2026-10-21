# Core: the expression language, the calculator state, scripting and the
# models exposed to QML. No Qt Quick, so the tests link it directly.
qt_add_library(reckon_core STATIC)
qt_add_qml_module(reckon_core
    URI Reckon.Core
    VERSION 1.0
    SOURCES
        core/Number.h
        core/Lexer.h core/Lexer.cpp
        core/Expression.h core/Parser.cpp
        core/Environment.h core/Environment.cpp
        core/Functions.h core/Functions.cpp
        core/Constants.h core/Constants.cpp
        core/Formatter.h core/Formatter.cpp
        core/Models.h core/Models.cpp
        core/Calculator.h core/Calculator.cpp
        core/ScriptEngine.h core/ScriptEngine.cpp
        core/ScriptLibrary.h core/ScriptLibrary.cpp
)
# The generated QML type registrations include headers by file name only.
target_include_directories(reckon_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core
)
target_link_libraries(reckon_core PUBLIC Qt6::Core Qt6::Qml)

# UI: QML and the custom items, as a library so the UI smoke test can load it.
set_source_files_properties(qml/AppSettings.qml qml/Icons.qml qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(reckon_ui STATIC)
qt_add_qml_module(reckon_ui
    URI Reckon
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppSettings.qml
        qml/Theme.qml
        qml/Icons.qml
        qml/Icon.qml
        qml/IconButton.qml
        qml/Display.qml
        qml/Editor.qml
        qml/Keypad.qml
        qml/KeypadButton.qml
        qml/SidePanel.qml
        qml/NameList.qml
        qml/HistoryPanel.qml
        qml/GraphPane.qml
        qml/ScriptPane.qml
        qml/SettingsDialog.qml
        qml/AboutDialog.qml
        qml/ModalDialog.qml
        qml/SplitterHandle.qml
        qml/PanelHeader.qml
    SOURCES
        ui/GraphView.h ui/GraphView.cpp
        ui/ExpressionHighlighter.h ui/ExpressionHighlighter.cpp
        ui/Clipboard.h
        ui/Fonts.h ui/Fonts.cpp
    RESOURCES
        icons/book-open.svg
        icons/braces.svg
        icons/calculator.svg
        icons/chart-line.svg
        icons/check.svg
        icons/chevron-down.svg
        icons/circle-alert.svg
        icons/code.svg
        icons/copy.svg
        icons/download.svg
        icons/equal.svg
        icons/eye.svg
        icons/eye-off.svg
        icons/file.svg
        icons/folder-open.svg
        icons/hash.svg
        icons/history.svg
        icons/keyboard.svg
        icons/maximize.svg
        icons/pencil.svg
        icons/play.svg
        icons/plus.svg
        icons/refresh-cw.svg
        icons/save.svg
        icons/search.svg
        icons/settings.svg
        icons/sigma.svg
        icons/square-function.svg
        icons/trash-2.svg
        icons/variable.svg
        icons/x.svg
        icons/zoom-in.svg
        icons/zoom-out.svg
        logo/reckon.svg
        fonts/Inter-Bold.ttf
        fonts/Inter-Italic.ttf
        fonts/Inter-Medium.ttf
        fonts/Inter-Regular.ttf
        fonts/Inter-SemiBold.ttf
        fonts/JetBrainsMono-Bold.ttf
        fonts/JetBrainsMono-Italic.ttf
        fonts/JetBrainsMono-Regular.ttf
)
target_include_directories(reckon_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ui)
target_link_libraries(reckon_ui PUBLIC
    reckon_core
    reckon_coreplugin
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

qt_add_executable(reckon main.cpp)
target_compile_definitions(reckon PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")
target_link_libraries(reckon PRIVATE reckon_ui reckon_uiplugin)
# A window of its own on Windows, an app bundle on macOS, each with the
# icon file manager, Dock and taskbar show.
set_target_properties(reckon PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE reckon.icns
)
if(WIN32)
    target_sources(reckon PRIVATE logo/reckon.rc)
elseif(APPLE)
    target_sources(reckon PRIVATE logo/reckon.icns)
    set_source_files_properties(logo/reckon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

install(TARGETS reckon
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
# Desktop environments find the icon by the desktop file's Icon= name.
install(FILES logo/reckon.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES logo/reckon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
