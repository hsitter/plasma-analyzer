# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

include_directories(${PULSEAUDIO_INCLUDE_DIR})

set(qml_SRCS
    libqml/qmldir
)

add_library(plasma-analyzer-private SHARED libqml/plugin.cpp StreamReader.cpp ${qml_SRCS})
target_link_libraries(plasma-analyzer-private
    Qt5::Qml
    ${PULSEAUDIO_LIBRARY}
)
set(PRIVATE_QML_INSTALL_DIR ${QML_INSTALL_DIR}/org/kde/plasma/private/analyzer)
install(TARGETS plasma-analyzer-private DESTINATION ${PRIVATE_QML_INSTALL_DIR})
install(FILES ${qml_SRCS} DESTINATION ${PRIVATE_QML_INSTALL_DIR})
