// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

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
        const frameBufferLength = event.frameBuffer.length // the data we get out of reader is qlist<int> (really qlist<int16>)
        const channels = 1
        const rate = 44100

        // in loadmetadata in example
        const fft = new FFT.FFT(frameBufferLength / channels, rate);
        fft.forward(event.frameBuffer);

        // Quick and dirty merge logic.
        // Our bar property decides how many outputs we generate, each
        // output is the sum of the bins within its range. So with 2 bars
        // the first bar is the sum of spectrum0 to spectrum.length/2 and the
        // second bar is spectrum.length/2 to spectrum.length.
        // Doesn't scale or anything so the results are fairly garbage.
        const mergeCount = fft.spectrum.length / bars / 2 // drop half the bars because they'll not contain anything worthwhile (13khz to 22khz is pretty empty)
        var output = new Array(bars)
        for (var i = 0; i < output.length; ++i) {
            output[i] = 0;
            for (var j = Math.floor(mergeCount * i); j < mergeCount * (i + 1); ++j) {
                // TODO: this way of dropping bins is super stupid, with a very high resolution spectrum we'd have bins that are always 0 becuase we drop their underlying fft spectrum data
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
