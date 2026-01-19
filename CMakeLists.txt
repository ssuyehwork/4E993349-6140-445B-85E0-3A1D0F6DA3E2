cmake_minimum_required(VERSION 3.16)

project(RapidNotes VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network Concurrent)

# 源代码列表
set(SOURCES
    src/main.cpp
    src/core/DatabaseManager.cpp
    src/core/HotkeyManager.cpp
    src/core/ClipboardMonitor.cpp
    src/models/NoteModel.cpp
    src/ui/FloatingBall.cpp
    src/ui/QuickWindow.cpp
    src/ui/MainWindow.cpp
    src/ui/GraphWidget.cpp
    src/ui/Editor.cpp
    src/ui/ScreenshotTool.h
    resources/resources.qrc
)

add_executable(RapidNotes ${SOURCES})

target_link_libraries(RapidNotes PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Concurrent
)

# Windows 平台特定配置
if(WIN32)
    target_link_libraries(RapidNotes PRIVATE user32 shell32)
    set_target_properties(RapidNotes PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
