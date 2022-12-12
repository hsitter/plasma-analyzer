// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

#pragma once

#include <array>

#include <pulse/pulseaudio.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QSharedPointer>
#include <QTimer>

namespace std
{
template<>
struct default_delete<pa_threaded_mainloop> {
    void operator()(pa_threaded_mainloop *ptr) const
    {
        pa_threaded_mainloop_stop(ptr);
        pa_threaded_mainloop_free(ptr);
    }
};

template<>
struct default_delete<pa_context> {
    void operator()(pa_context *ptr) const
    {
        pa_context_disconnect(ptr);
        pa_context_unref(ptr);
    }
};

template<>
struct default_delete<pa_stream> {
    void operator()(pa_stream *ptr) const
    {
        pa_stream_disconnect(ptr);
        pa_stream_unref(ptr);
    }
};

template<>
struct default_delete<pa_operation> {
    void operator()(pa_operation *ptr) const
    {
        if (ptr) { // implicitly doesn't like to be called with nullptr for some reason
            pa_operation_unref(ptr);
        }
    }
};
} // namespace std

class StreamReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int samplingFrequency MEMBER s_sampleRate CONSTANT)
    Q_PROPERTY(int samplingChannels MEMBER s_sampleChannels CONSTANT)
    Q_PROPERTY(int sampleSize MEMBER SAMPLES CONSTANT)
public:
    static StreamReader *instance();

    Q_INVOKABLE QList<int> samplesVector();

    // The amount of samples in a set depends on the bytes per sample.
    // Since we have a 16bit PCM spec 2 bytes make one sample.
    // Meaning we have 2048 int16 *samples* in 4096 *bytes*
    static constexpr auto SAMPLE_BYTES = sizeof(int16_t);
    static constexpr auto SAMPLES = 2048;
    static constexpr auto SAMPLES_BYTES = SAMPLES * SAMPLE_BYTES;

    static constexpr auto s_sampleRate = 44100;
    static constexpr auto s_sampleChannels = 1;
    static constexpr pa_sample_spec s_sampleSpec = {.format = PA_SAMPLE_S16LE,
                                                    .rate = s_sampleRate,
                                                    .channels = s_sampleChannels};

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
    static void cb_subscription(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata);

    std::unique_ptr<pa_threaded_mainloop> m_mainloop;
    pa_mainloop_api *m_mainloopAPI = nullptr; // not unique because we don't own this one
    std::unique_ptr<pa_context> m_context;
    std::unique_ptr<pa_stream> m_stream;

    /** The current source being recorded (-1 if none) */
    qint64 m_sourceIndex = -1;
    /** This is the index of the sink of which we want to record the source */
    qint64 m_sinkIndex = -1;

private:
    std::array<int16_t, SAMPLES> m_buffer;
    size_t m_bufferIndex = 0;

    QMap<uint32_t, uint> m_musicsPerSink;

    QList<int> m_samples;
    QReadWriteLock m_samplesRWLock;
    // Used to discard read requests exceeding our desired read limit per second
    QElapsedTimer m_readTime;

    StreamReader();
};
