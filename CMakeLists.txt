# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

project(plasma-analyzer)

cmake_minimum_required(VERSION 3.22)

find_package(ECM 5.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
LIST(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 CONFIG REQUIRED Gui Qml)
find_package(KF5Plasma 5.0 REQUIRED)
find_package(PulseAudio REQUIRED)

add_subdirectory(applet)
add_subdirectory(src)
