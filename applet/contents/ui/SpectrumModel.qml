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
import org.kde.plasma.private.analyzer 1.0 as Analyzer

import "fft.js" as FFT

ListModel {
    id: listModel

    // This can't be a nested child item, ListModel gets confused.
    property Component listElementComponent: Component {
        ListElement {
            property real display: 0.0
        }
    }

    property int bars: 32
    property int global_peak: 0

    function audioAvailable(event) {
        // https://wiki.mozilla.org/Audio_Data_API
        // FIXME: hardcoded crap
        var frameBufferLength = event.frameBuffer.length // the data we get out of reader is qlist<int> (really qlist<int16>)
        var channels = 1
        var rate = 44100

        // in loadmetadata in example
        var fft = new FFT.FFT(frameBufferLength / channels, rate);


        var fb = event.frameBuffer
        var t  = event.time /* unused, but it's there */
        var signal = new Float32Array(fb.length / channels)
        var magnitude;

        for (var i = 0, fbl = frameBufferLength / 2; i < fbl; i++ ) {
            // Assuming interlaced stereo channels,
            // need to split and merge into a stero-mix mono signal
            signal[i] = fb[i];
        }

        fft.forward(signal);

        // Quick and dirty merge logic.
        // Our bar property decides how many outputs we generate, each
        // output is the sum of the bins within its range. So with 2 bars
        // the first bar is the sum of spectrum0 to spectrum.length/2 and the
        // second bar is spectrum.length/2 to spectrum.length.
        // Doesn't scale or anything so the results are fairly garbage.
        var mergeCount = fft.spectrum.length / bars
        var output = new Array(bars)
        for (var i = 0; i < output.length; ++i) {
            output[i] = 0;
            for (var j = Math.floor(mergeCount * i); j < mergeCount * (i + 1); ++j) {
                // TODO: this way of dropping bins is super stupid, with a very high resolution spectrum we'd have bins that are always 0 becuase we drop their underlying fft spectrum data
                var frequency = j * (rate / 2.0) / fft.spectrum.length
                if (frequency < 100 || frequency > 20000) {
                    continue; // skip inaudible stuff
                }
                output[i] += fft.spectrum[j]
            }
        }

        var peak = 0
        var bottom = 0
        for (var i = 0; i < output.length; ++i) {
            if (output[i] > peak) {
                peak = output[i]
            }
        }

        // scale by maximum peak
        for (var i = 0; i < output.length; ++i) {
            output[i] = output[i] / peak * 1

        }

        // Find new peak after scaling.
        peak = 0
        bottom = 0
        for (var i = 0; i < output.length; ++i) {
            if (output[i] > peak) {
                peak = output[i]
            }
            if (output[i] > global_peak) {
                global_peak = output[i]
            }
        }

        // FIXME: this is broken when the bands shrink (can it ever?)
        var add = output.length - listModel.count
        var remove = listModel.count - output.length
        if (add > 0) {
            for (var i = 0; i < add; ++i) {
                var obj = listElementComponent.createObject(listModel, { "display": 1.0 })
                listModel.append(obj)
            }
        }
        if (remove > 0) {
            listModel.remove(0, remove)
        }

        var peak_range = peak - bottom;
//        var peak_range = global_peak - bottom;
        var norm_range = 1 - 0;
        for (var i = 0; i < output.length; ++i) {
            listModel.get(i).display = ((output[i] - bottom) / peak_range) * norm_range
        }
    }
}
