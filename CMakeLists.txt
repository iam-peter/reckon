cmake_minimum_required(VERSION 3.21)

project(reckon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
# An installed reckon finds the Qt it was built against.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

# Executables, the tests' too, go to build/bin on every platform rather
# than next to their CMakeLists.txt (qt_standard_project_setup() would
# move them on Windows only).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Lay QML modules out by URI so qmllint and qmlls find them.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)
    add_subdirectory(tests)
endif()
