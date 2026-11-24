cmake_minimum_required(VERSION 3.21)
project(atk-control VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)

qt_add_executable(atk-control
    src/main.cpp
    src/types.h
    src/protocol.h
    src/protocol.cpp
    src/models.h
    src/models.cpp
    src/hidraw.h
    src/hidraw.cpp
    src/device.h
    src/device.cpp
    src/worker.h
    src/worker.cpp
    src/batteryservice.h
    src/batteryservice.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    resources.qrc
)

target_link_libraries(atk-control PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
)

target_compile_options(atk-control PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS atk-control RUNTIME DESTINATION bin)
install(FILES packaging/atk-control.desktop DESTINATION share/applications)
install(FILES packaging/icons/atk-control.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES packaging/icons/atk-control.png DESTINATION share/icons/hicolor/256x256/apps)
install(FILES packaging/71-atk-control.rules DESTINATION share/atk-control)
install(FILES packaging/atk-control.appdata.xml DESTINATION share/metainfo)
install(FILES LICENSE DESTINATION share/licenses/atk-control)
install(DIRECTORY packaging/plasmoid/org.atk.battery DESTINATION share/plasma/plasmoids)
