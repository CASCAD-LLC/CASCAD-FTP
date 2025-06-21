cmake_minimum_required(VERSION 3.16)
project(CASCAD-FTP VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(CURL REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

qt_add_executable(CASCAD-FTP
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(CASCAD-FTP PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${CURL_LIBRARIES}
)

qt_finalize_executable(CASCAD-FTP)
