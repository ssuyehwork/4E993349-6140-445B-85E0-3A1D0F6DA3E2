cmake_minimum_required(VERSION 3.16)

project(RapidNotes VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network Concurrent Svg)

# 源代码列表 - 确保 NoteEditWindow 相关的两个文件都在这里
set(SOURCES
    src/main.cpp
    src/core/DatabaseManager.cpp
    src/core/HotkeyManager.cpp
    src/core/ClipboardMonitor.cpp
    src/core/KeyboardHook.cpp
    src/core/OCRManager.cpp
    src/models/NoteModel.cpp
    src/models/CategoryModel.cpp
    src/ui/FloatingBall.cpp
    src/ui/QuickWindow.cpp
    src/ui/QuickToolbar.cpp
    src/ui/DropTreeView.cpp
    src/ui/MainWindow.cpp
    src/ui/QuickPreview.h
    src/ui/HeaderBar.cpp
    src/ui/MetadataPanel.cpp
    src/ui/Toolbox.cpp
    src/ui/SystemTray.cpp
    src/ui/Editor.cpp
    src/ui/NoteDelegate.h
    src/ui/QuickNoteDelegate.h
    src/ui/NoteEditWindow.h    # <--- 必须有
    src/ui/NoteEditWindow.cpp  # <--- 必须有
    src/ui/ScreenshotTool.h
    src/ui/SearchLineEdit.h
    src/ui/SearchLineEdit.cpp
    src/ui/WritingAnimation.h
    src/ui/WritingAnimation.cpp
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
    Qt6::Svg
)

if(WIN32)
    target_link_libraries(RapidNotes PRIVATE user32 shell32 psapi)
    set_target_properties(RapidNotes PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()