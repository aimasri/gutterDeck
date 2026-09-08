cmake_minimum_required(VERSION 3.16)
project(GutterDeck VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Find XCB
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb xcb-ewmh xcb-icccm xcb-keysyms)

# Qt Autogen settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Compiler Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Add Source Files
set(SOURCES
    src/main.cpp
    src/infrastructure/XcbConnection.cpp
    src/infrastructure/XcbEngine.cpp
    src/infrastructure/WindowWatcher.cpp
    src/infrastructure/ConfigManager.cpp
    src/domain/StateMachine.cpp
    src/domain/AppLauncher.cpp
    src/domain/DeckController.cpp
    src/presentation/GutterWidget.cpp
    src/presentation/CurtainWidget.cpp
    src/presentation/OverlayWindow.cpp
    src/presentation/SleekDialogs.cpp
    src/presentation/AppIcon.cpp
    src/presentation/ProfilePickerWindow.cpp
    src/infrastructure/TrayClient.cpp
    src/presentation/TrayDaemonWindow.cpp
)

add_executable(gutterdeck ${SOURCES})

# Include Directories
target_include_directories(gutterdeck PRIVATE 
    src
    ${XCB_INCLUDE_DIRS}
)

# Link Libraries
target_link_libraries(gutterdeck PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets Qt6::Network 
    ${XCB_LIBRARIES}
)

# Unit Testing Option
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
