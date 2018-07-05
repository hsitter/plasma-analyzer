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

import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQml.Models 2.2
import QtQuick.Dialogs 1.2

import privateanalyzer 1.0 as Analyzer
//import org.kde.plasma.private.analyzer 1.0 as Analyzer

import "ui"

ApplicationWindow {
    id: window

    width: 1290
    height: 400
    visible: true

    ColumnLayout {
        anchors.fill: parent

        ListView {
            id: spectrum0

            height: 100
            Layout.fillWidth: true
            orientation: ListView.Horizontal
            spacing: 1

            model: SpectrumModel {
                id: spectrumModel

                Component.onCompleted: {
                    Analyzer.StreamReader.readyRead.connect(onData)
                }

                function onData() {
                    var e = {}
                    e['frameBuffer'] = Analyzer.StreamReader.samplesVector()
                    e['time'] = new Date()
                    spectrumModel.audioAvailable(e);
                }
            }

            delegate: Rectangle {
//                height: Math.max(2, ((Math.floor((display * 100)/10) * 10) / 100) * parent.height)
                height: Math.max(2, display * parent.height)
                width: 5
                property real normalizedIndex: index / ListView.view.count
                color: Qt.hsla(normalizedIndex * 0.8, 0.95, 0.5, 1.0)
                anchors.bottom: parent.bottom
            }
        }

        ListView {
            id: fadeSpectrum

            property var intensities: []
            property var magnitudes: []

            model: magnitudes

            height: 100
            Layout.fillWidth: true
            orientation: ListView.Horizontal

            delegate: Rectangle {
                height: modelData * 100
                width: 5
                color: "lightgrey"
                opacity: fadeSpectrum.intensities[index]
                anchors.bottom: parent.bottom
            }

            Timer {
                interval: 80
                running: true
                repeat: true
                onTriggered: {
                    for (var i = 0; i < fadeSpectrum.intensities.length; ++i) {
                        fadeSpectrum.intensities[i] = Math.max(0, fadeSpectrum.intensities[i]-0.05)
                        fadeSpectrum.magnitudes[i] -= 0.01
                    }

                        fadeSpectrum.model = []
                    fadeSpectrum.model = fadeSpectrum.magnitudes
                }
            }


            ListView {
                id: spectrum

                property bool active: false
                    property variant prevm

                height: 100
//                Layout.fillWidth: true
                anchors.fill: parent
                orientation: ListView.Horizontal
                spacing: 1

                model: Analyzer.Context.list
                //                onModelChanged: {
                //                    if (fadeSpectrum.magnitudes.length != model.length) {
                //                        for (var i = 0; i < model.length; ++i) {
                //                            fadeSpectrum.intensities.push(0)
                //                            fadeSpectrum.magnitudes.push(0)
                //                            fadeSpectrum.model = fadeSpectrum.magnitudes
                //                        }
                //                    }


                //                    for (var i = 0; i < model.length; ++i) {
                //                        var value = model[i]

                //                        if (value >= fadeSpectrum.magnitudes[i] || fadeSpectrum.intensities[i] <= 0) {
                //                            fadeSpectrum.intensities[i] = 1.0
                ////                            fadeSpectrum.magnitudes[i] = value
                //                            fadeSpectrum.magnitudes[i] = (Math.floor((value * 100)/10) * 10) / 100
                //                        }
                //                    }
                //                    fadeSpectrum.model = fadeSpectrum.magnitudes
                //                    active = true
                //                }


                delegate: Rectangle {
//                    height: Math.floor((modelData * 100)/20) * 20
//                    height: Math.floor((modelData * 100)/10) * 10
                    height: Math.max(2, ((Math.floor((modelData * 100)/10) * 10) / 100) * parent.height)
                    width: 5
                    color: "blue"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: "black" }
                    }

                    anchors.bottom: parent.bottom
                }
            }
        }
    }
}

