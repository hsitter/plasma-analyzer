plasma_install_bundled_package(. org.kde.plasma.analyzer)

file(GLOB_RECURSE QML_SRCS *)
add_custom_target(QmlFiles ALL echo SOURCES ${QML_SRCS})
