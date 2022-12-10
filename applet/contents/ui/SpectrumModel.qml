// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2015 Darby A Payne https://github.com/dpayne/cli-visualizer
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

/*
The MIT License (MIT)

Copyright (c) 2015 Darby A Payne

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

import QtQuick 2.2
import org.kde.plasma.private.analyzer 1.0 as Analyzer

import "fft.js" as FFT

ListModel {
    id: listModel

    // The component itself can't be a nested child item, ListModel gets confused.
    property Component listElementComponent: Component {
        ListElement {
            property real display: 0.0
        }
    }

    property int bars: 32

    property var cutoffBars: -1
    property var cutoff: {}

    readonly property var samplingFrequency: Analyzer.StreamReader.samplingFrequency
    readonly property var samplingChannels: Analyzer.StreamReader.samplingChannels
    readonly property var sampleSize: Analyzer.StreamReader.sampleSize

    property var previousMaxHeights: []

    function calculateMovingAverageAndStandardDeviation(newValue, maxNumberOfElements) {
        if (previousMaxHeights.length > maxNumberOfElements) {
            previousMaxHeights.shift()
        }
        previousMaxHeights.unshift(newValue)

        const sum = previousMaxHeights.reduce((accumulator, currentValue) => accumulator + currentValue)
        const movingAverage = sum / previousMaxHeights.length

        const innerProduct = (a, b) => a.map((x, i) => a[i] * b[i]).reduce((m, n) => m + n);
        const squaredSummation = innerProduct(previousMaxHeights, previousMaxHeights)
        const standardDeviation = Math.sqrt((squaredSummation / previousMaxHeights.length) - Math.pow(movingAverage, 2))
        return {movingAverage: movingAverage, standardDeviation: standardDeviation}
    }

    function scaleBars(height, output) {
        const maxElement = (output) => {
            let maxHeight = 0
            let maxHeightIndex = -1;
            for (var i = 0; i < output.length; ++i) {
                const value = output[i]
                if (value > maxHeight) {
                    maxHeight = value
                    maxHeightIndex = i
                }
            }
            return {maxHeightIndex: maxHeightIndex, maxHeight: maxHeight}
        }

        // max number of elements to calculate for moving average
        const secondsForAutoScaling = 1
        const maxNumberOfElements = ((secondsForAutoScaling * samplingFrequency) / sampleSize) * 2

        const { movingAverage, standardDeviation } = calculateMovingAverageAndStandardDeviation(maxElement(output).maxHeight, maxNumberOfElements)

        const maxHeight = Math.max(movingAverage + (2 * standardDeviation), 1.0 /* don't divide by 0 */)
        for (var i = 0; i < output.length; ++i) {
            output[i] = Math.min(height, ((output[i] / maxHeight) * height) - 1)
        }
        return output
    }

    function calculateCutoff(fftSize) {
        const lowestFrequency = 30
        const highestFrequency = 22050

        const frequencyConst = Math.log10(lowestFrequency / highestFrequency) / ((1.0 / (bars + 1.0)) - 1.0)

        let lowCutoffFrequencies = new Array(bars + 1).fill(-1)
        let highCutoffFrequencies = new Array(bars + 1).fill(-1)
        let frequencyConstPerBin = new Array(bars + 1).fill(-1)

        for (var i = 0; i <= bars; ++i) {
            frequencyConstPerBin[i] = highestFrequency *
                 Math.pow(10.0,  (frequencyConst * -1) + (((i + 1.0) / (bars + 1.0)) * frequencyConst))

            const frequency = frequencyConstPerBin[i] / (samplingFrequency / 2)
            lowCutoffFrequencies[i] = Math.floor(frequency * (sampleSize / 4))
            if (i > 0) {
                if (lowCutoffFrequencies[i] <= lowCutoffFrequencies[i - 1]) {
                    lowCutoffFrequencies[i] = lowCutoffFrequencies[i - 1] + 1
                }
                highCutoffFrequencies[i - 1] = lowCutoffFrequencies[i - 1]
            }
        }

        return {frequencyConstPerBin: frequencyConstPerBin, lowCutoffFrequencies: lowCutoffFrequencies, highCutoffFrequencies: highCutoffFrequencies}
    }

    function audioAvailable(event) {
        // https://wiki.mozilla.org/Audio_Data_API
        const fft = new FFT.FFT(event.frameBuffer.length / samplingChannels, samplingFrequency);
        fft.forward(event.frameBuffer);

        if (cutoffBars !== bars) {
            cutoff = calculateCutoff(fft.spectrum.length)
            cutoffBars = bars
        }
        let {frequencyConstPerBin: frequencyConstPerBin, lowCutoffFrequencies: lowCutoffFrequencies, highCutoffFrequencies: highCutoffFrequencies} = cutoff

        var output = new Array(bars).fill(-1)
        for (let i = 0; i < output.length; ++i) {
            let frequencyMagnitude = 0
            for (let cutoffFrequency = lowCutoffFrequencies[i]; cutoffFrequency <= highCutoffFrequencies[i]; ++cutoffFrequency) {
                frequencyMagnitude += fft.spectrum[cutoffFrequency]
            }

            output[i] = frequencyMagnitude / (highCutoffFrequencies[i] - lowCutoffFrequencies[i] + 1)

            // boost high frequencies
            output[i] *= Math.log2(2 + i) * (100 / output.length)
            output[i] = Math.pow(output[i], 0.5)
        }

        const heightScale = 100
        output = scaleBars(heightScale, output)

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

        var maxHeight = 0
        for (var i = 0; i < output.length; ++i) {
            listModel.get(i).display = output[i] / heightScale
        }
    }
}
