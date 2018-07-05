project(plasma-analyzer)

cmake_minimum_required(VERSION 3.0)

find_package(ECM 5.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

find_package(Qt5 CONFIG REQUIRED Gui Qml)
find_package(KF5Plasma 5.0 REQUIRED)

find_package(PulseAudio REQUIRED)

add_subdirectory(applet)
add_subdirectory(src)
