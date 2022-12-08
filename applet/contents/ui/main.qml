/*
    Copyright 2018 Harald Sitter <sitter@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License or (at your option) version 3 or any later version
    accepted by the membership of KDE e.V. (or its successor approved
    by the membership of KDE e.V.), which shall act as a proxy
    defined in Section 14 of version 3 of the license.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.2
import QtQuick.Layouts 1.0

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.plasmoid 2.0

import org.kde.plasma.private.analyzer 1.0 as Analyzer

import QtGraphicalEffects 1.0

import 'fft.js' as FFT

Item {
    id: main

    Layout.preferredWidth: units.gridUnit * 10
    Layout.preferredHeight: units.gridUnit * 20
    Plasmoid.backgroundHints: Plasmoid.configuration.Background ? "StandardBackground" : "NoBackground";
    Plasmoid.preferredRepresentation: Plasmoid.fullRepresentation

    ListView {
        id: spectrum

        onWidthChanged: spectrumModel.updateBarCount()
        onHeightChanged: spectrumModel.updateBarCount()
        Component.onCompleted: spectrumModel.updateBarCount()

        anchors.fill: parent
        orientation: ListView.Horizontal
        interactive: false

        model: SpectrumModel {
            id: spectrumModel

            Component.onCompleted: {
                Analyzer.StreamReader.readyRead.connect(onData)
            }

            function onData() {
                var e = {}
                e['frameBuffer'] = Analyzer.StreamReader.samplesVector()
                e['time'] = new Date()
                audioAvailable(e);
            }

            function updateBarCount() {
                // TODO dynamically scale the bands depending on how many bars
                // we can fight into the current width
                // the data is stereo dupe and the other half is garbage after fft.
                // FIXME: this is garbage. 8 is the rect width, 4 is the spacing
                // FIXME: this is actually off.. the longer the widget the larger the offness. not quite sure why... ooh delegate is actually max(shadow,rect) wide, so that may screw it up?
                bars = Math.floor(width / (8.0 + 6))
            }
        }

        spacing: 4

        delegate: Item {
            width: Math.max(rect.width, shadow.width)
            height: parent.height

            Rectangle {
                id: shadow
                x: rect.x
                y: rect.y
                width: rect.width + 2
                height: rect.height + 2
                color: "red"
                visible: !plasmoid.configuration.Background
            }

            Rectangle {
                id: rect
                height: Math.max(2, display * parent.height)
                width: 8
                property real normalizedIndex: index / parent.ListView.view.count
                property color prideColor: Qt.hsla(normalizedIndex * 0.8, 0.95, 0.5, 1.0)
                color: plasmoid.configuration.Pride ? Qt.hsla(normalizedIndex * 0.8, 0.95, 0.5, 1.0) : theme.textColor
                anchors.top: plasmoid.configuration.Anchor === 1 ? parent.top: undefined
                anchors.bottom: plasmoid.configuration.Anchor === 0 ? parent.bottom : undefined
            }

            Component.onCompleted: console.log("new delegate")
        }
    }
}
