// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

#pragma once

#include <array>

#include <pulse/pulseaudio.h>
#include <QList>
#include <QObject>
#include <QMap>
#include <QMutex>
#include <QReadWriteLock>
#include <QSharedPointer>
#include <QTime>
#include <QTimer>

#warning FIXME sample bytes hardcoded

class StreamReader : public QObject
{
    Q_OBJECT
public:
    static StreamReader *instance();
    ~StreamReader() override;
    Q_DISABLE_COPY_MOVE(StreamReader);

    Q_INVOKABLE QList<int> samplesVector();

signals:
    void readyRead();

private slots:
    void newMusicScores();
    void poll();

private:
    void startRecorder();
    void read(pa_stream *s, size_t length);
    void moveToSink(uint32_t sinkId);

    static void cb_stream_read(pa_stream *s, size_t length, void *userdata);
    static void cb_stream_state(pa_stream *s, void *userdata);

    static void cb_context_state(pa_context *c, void *userdata);
    static void cb_get_source_info(pa_context *c, const pa_source_info *i, int eol, void *userdata);
    static void cb_sink_input_info(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata);

    pa_threaded_mainloop *m_mainloop = nullptr;
    pa_mainloop_api *m_mainloopAPI = nullptr;
    pa_context *m_context = nullptr;

    pa_stream *m_stream = nullptr;

    /** The current source being recorded (-1 if none) */
    int m_sourceIndex = -1;
    /** This is the index of the sink of which we want to reecord the source */
    qint64 m_sinkIndex = -1;


    // The amount of samples in a set depends on the bytes per sample.
    // Since we have a 16bit pcm spec 2 bytes make one sample.
    // Meaning we have 512 int16 *samples* in 1024 *bytes*
    static constexpr auto SAMPLE_BYTES = 2048;


    // This fits 512 16bit samples. i.e. 1024 bytes!
    // This is the transfer buffer. The buffer is filled with data, once
    // a full sample set has been gathered the buffer is transferred into the
    // sample array and transferred to the Analyzer.
    std::array<int16_t, SAMPLE_BYTES / 2> m_buffer;
    // Allocated on the heap in read(). We transfer ownership of this array
    // to the Analyzer once a full snapshot was taken.
    int16_t *m_samples = nullptr;
    size_t m_bufferIndex = 0;

    // Int bool to sync shutdown behavior into callbacks.
    QAtomicInt m_shuttingDown = 0;

    QMutex m_musicsPerSinkMutex;
    QMap<uint32_t, uint> m_musicsPerSink;

    QSharedPointer<qint16> m_sharedSamples;
    QReadWriteLock m_samplesRWLock;
    // Polls sink inputs for music types.
    QTimer m_pollTimer;
    // Used to discard read requests exceeding our desired read limit per second
    QTime m_readTime;

    QThread *m_thread;

    StreamReader();
};
