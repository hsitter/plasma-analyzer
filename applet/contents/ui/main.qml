// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

import QtQuick 2.15
import QtQuick.Layouts 1.0

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.plasmoid 2.0
import QtQuick.Window 2.2
import Qt.labs.animation 1.0

import org.kde.plasma.private.analyzer 1.0 as Analyzer

import 'fft.js' as FFT

Item {
    id: main

    Layout.preferredWidth: units.gridUnit * 10
    Layout.preferredHeight: units.gridUnit * 10
    Plasmoid.backgroundHints: Plasmoid.configuration.Background ? "StandardBackground" : "NoBackground";
    Plasmoid.preferredRepresentation: Plasmoid.fullRepresentation

    property var barWidth: plasmoid.configuration.Width
    property var barSpacing: plasmoid.configuration.Spacing

    ListView {
        id: spectrum

        onWidthChanged: spectrumModel.updateBarCount()
        onHeightChanged: spectrumModel.updateBarCount()
        Component.onCompleted: spectrumModel.updateBarCount()

        anchors.fill: parent
        orientation: ListView.Horizontal
        interactive: false

        Connections {
            target: Analyzer.StreamReader
            function onReadyRead() {
                spectrumModel.audioAvailable({frameBuffer: Analyzer.StreamReader.samplesVector(), time: new Date()});
            }
        }

        model: SpectrumModel {
            id: spectrumModel
            function updateBarCount() {
                bars =  Math.floor(width / (barWidth + barSpacing))
            }
        }

        spacing: barSpacing

        delegate: Item {
            width: Math.max(rect.width, shadow.width)
            height: parent.height

            Rectangle {
                id: shadow
                x: rect.x
                y: rect.y
                width: rect.width + 2
                height: rect.height + 2
                color: theme.backgroundColor
                visible: !plasmoid.configuration.Background
            }

            Rectangle {
                id: rect
                height: Math.max(2, Math.min(display * parent.height, parent.height))

                Behavior on height {
                    // only animate on decrease, not increases. this keeps things snappy
                    enabled: targetValue < targetProperty.object[targetProperty.name]
                    NumberAnimation {
                        easing.type: Easing.OutInQuint
                        alwaysRunToEnd: false
                        duration: 250 /* random value that looks good and is snappy enought */
                    }
                }

                width: barWidth
                property real normalizedIndex: index / parent.ListView.view.count
                property color prideColor: Qt.hsla(normalizedIndex * 0.8, 0.95, 0.5, 1.0)
                color: plasmoid.configuration.Pride ? Qt.hsla(normalizedIndex * 0.8, 0.95, 0.5, 1.0) : theme.textColor
                anchors.top: plasmoid.configuration.Anchor === 1 ? parent.top: undefined
                anchors.bottom: plasmoid.configuration.Anchor === 0 ? parent.bottom : undefined
            }
        }
    }
}
