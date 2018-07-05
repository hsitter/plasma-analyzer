set(analyzer_SRCS
    StreamReader.cpp
)

include_directories(${PULSEAUDIO_INCLUDE_DIR})

qt5_add_resources(analyzer_SRCS resources.qrc)
add_executable(spectrump main.cpp ${analyzer_SRCS})
target_link_libraries(spectrump
    Qt5::Qml
    Qt5::Gui
    ${PULSEAUDIO_LIBRARY})

set(qml_SRCS
    libqml/qmldir
)

add_library(plasma-analyzer-private SHARED libqml/plugin.cpp ${analyzer_SRCS} ${qml_SRCS})
target_link_libraries(plasma-analyzer-private
    Qt5::Qml
    ${PULSEAUDIO_LIBRARY}
)
set(PRIVATE_QML_INSTALL_DIR ${QML_INSTALL_DIR}/org/kde/plasma/private/analyzer)
install(TARGETS plasma-analyzer-private DESTINATION ${PRIVATE_QML_INSTALL_DIR})
install(FILES ${qml_SRCS} DESTINATION ${PRIVATE_QML_INSTALL_DIR})
