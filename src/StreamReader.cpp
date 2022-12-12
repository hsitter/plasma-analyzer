// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

#include "StreamReader.h"

#include <chrono>
#include <cmath>
#include <span>

#include <QMutexLocker>
#include <QQmlEngine>

using namespace std::chrono_literals;

inline StreamReader *THAT(void *x)
{
    Q_ASSERT(x);
    return static_cast<StreamReader *>(x);
}

template<typename T>
struct Expected {
    const int ret; // return value of call
    const int error; // errno immediately after the call
    std::unique_ptr<T> value; // the newly owned object (may be null)
};

template<typename T, typename Func, typename... Args>
Expected<T> owning_ptr_call(Func func, Args &&...args)
{
    T *raw = nullptr;
    const int ret = func(&raw, std::forward<Args>(args)...);
    return {ret, errno, std::unique_ptr<T>(raw)};
}

StreamReader::StreamReader()
    : m_buffer()
{
    // This starts a context, Context must be singleton within an app! calling start twice will most certainly crash
    if (m_mainloop.reset(pa_threaded_mainloop_new()); !m_mainloop) {
        qWarning("pa_mainloop_new failed");
        return;
    }

    if (m_mainloopAPI = pa_threaded_mainloop_get_api(m_mainloop.get()); !m_mainloopAPI) {
        qWarning("pa_threaded_mainloop_get_api failed");
        return;
    }

    if (pa_signal_init(m_mainloopAPI) != 0) {
        qWarning("pa_signal_init failed");
        return;
    }

    if (m_context.reset(pa_context_new(m_mainloopAPI, "plasma-analyzer")); !m_context) {
        qWarning("pa_context_new failed");
        return;
    }

    if (pa_context_connect(m_context.get(), nullptr, PA_CONTEXT_NOFAIL, nullptr ) != 0) {
        qWarning("pa_context_connect failed");
        return;
    }
    pa_context_set_state_callback(m_context.get(), cb_context_state, this);

    pa_threaded_mainloop_start(m_mainloop.get());
}

QList<int> StreamReader::samplesVector()
{
    QReadLocker l(&m_samplesRWLock);
    return m_samples;
}

StreamReader *StreamReader::instance()
{
    static StreamReader self;
    QQmlEngine::setObjectOwnership(&self, QQmlEngine::CppOwnership);
    return &self;
}

void StreamReader::cb_get_source_info(pa_context *c, const pa_source_info *i, int eol, void *userdata)
{
    if (eol != 0) {
        qDebug() << "source eol";
        return;
    }

    Q_ASSERT(c);
    Q_ASSERT(i);
    auto that = THAT(userdata);

    qDebug() << "-------------------";
    qDebug() << i->name;
    qDebug() << i->monitor_of_sink_name;
    qDebug() << i->monitor_of_sink;

    if (that->m_sourceIndex >= 0) {
        qDebug() << "already connected";
        return; // Already connected
    }

    if (that->m_sinkIndex >= 0) {
        if (i->monitor_of_sink != that->m_sinkIndex) {
            qDebug() << "not the wanted sink";
            return; // Not the monitor we want.
        }
    }

    if (i->state == PA_SOURCE_SUSPENDED) {
        qDebug() << "suspended";
        return; // Suspended.
    }

    // Make sure the values are what we are on now.
    that->m_sourceIndex = i->index;
    that->m_sinkIndex = i->monitor_of_sink;
    // Also reset read counter.
    that->m_bufferIndex = 0;
    that->m_readTime.start();

    qDebug() << "Connecting to" << i->name << ", the monitor of sink" << i->monitor_of_sink << i->state;

    // TODO: I am basically guessing what is sane here. I really don't
    //   understand the PA docs on latency control.

    pa_buffer_attr buffer_attr{};
    buffer_attr.maxlength = -1;
    buffer_attr.tlength = -1;
    buffer_attr.prebuf = -1;
    buffer_attr.minreq = -1;
    buffer_attr.fragsize = 2048;

    qDebug() << "fragsize" << buffer_attr.fragsize;
    qDebug() << "bytes per frame" << pa_frame_size(&s_sampleSpec);
    qDebug() << "sample size" << pa_sample_size(&s_sampleSpec);

    if ((pa_stream_connect_record(that->m_stream.get(), i->name, &buffer_attr, PA_STREAM_ADJUST_LATENCY)) < 0) {
        qWarning("pa_stream_connect_record failed: %s\n", pa_strerror(pa_context_errno(that->m_context.get())));
        return;
    }
}

void StreamReader::cb_stream_read(pa_stream *s, size_t length, void *userdata)
{
    Q_ASSERT(s);
    Q_ASSERT(length > 0);
    auto that = THAT(userdata);
    that->read(s, length);
}

// This is called from PA thread.
void StreamReader::read(pa_stream *s, size_t length)
{
    // We read into an array here because we need to feed this into
    // the FFT later, so using a higher level container such as QVector or QBA
    // would only get in the way and have no use up until after the FFT, where
    // we get new output data anyway.

    const void *data = nullptr;
    if (pa_stream_peek(s, &data, &length) < 0) {
        qWarning("pa_stream_peek failed: %s", pa_strerror(pa_context_errno(m_context.get())));
        return;
    }
    // length can be less **OR** more than a complete fragment (16bit sample). i.e.

    const auto bufferEmpty = data == nullptr && length == 0;
    const auto bufferHole = data == nullptr && length > 0;

    // clamp the length to whatever fits in our buffer, discard the rest. we don't need to fully represent
    // all samples, so tracking all samples (even when they exceed our current requirement) would needlessly
    // complicate things.
    const auto clampLength = qMin(m_buffer.size() - m_bufferIndex, length / SAMPLE_BYTES);
    const std::span dataSpan(static_cast<const int16_t *>(data), clampLength);
    // qDebug() << "peeked" << length << "bufferEmpty" << bufferEmpty << "bufferHole" << bufferHole << "clamLength" << dataSpan.size();

    if (bufferEmpty) {
        // No samples to be had, nothing for us to do.
        return;
    }

    // qDebug() << "copying" << dataSpan.size_bytes();
    memcpy(&m_buffer.at(m_bufferIndex), dataSpan.data(), dataSpan.size_bytes());
    m_bufferIndex += clampLength;
    pa_stream_drop(s);

    if (m_bufferIndex < m_buffer.size()) {
        // No complete sample set yet, waiting for more samples.
        return;
    }
    m_bufferIndex = 0;

    // 20 updates per second should be plenty. Discard data sets arriving
    // in excess of that. This prevents too many updates, which makes the bars
    // look like on drugs as well as putting severe strain on everything,
    // while still being regular enough to look fluid. Could possibly become
    // a user setting at some point?
    if (m_readTime.elapsed() < (40ms).count()) {
        return;
    }
    m_readTime.restart();

    // Put the array into a shared pointer so we can continue reading into
    // m_samples. This is a shared pointer because it is sent to 1:N Analyzers.
    QWriteLocker l(&m_samplesRWLock);
    m_samples.clear();
    m_samples.reserve(int(m_buffer.size()));
    for (const auto &sample : m_buffer) {
        m_samples.append(sample);
    }
    emit readyRead();
}

void StreamReader::cb_stream_state(pa_stream *s, void *userdata)
{
    Q_ASSERT(s);
    auto that = THAT(userdata);

    if (that->m_stream.get() != s) {
        // no longer interested, bugger off. This prevents race conditions on shutdown where we have already
        // thrown away the stream but still get its termination signal. We don't care about it at that point!
        return;
    }

    switch (pa_stream_get_state(s)) {
    case PA_STREAM_TERMINATED:
        qDebug() << "++++ STREAM TERMINATED" << s << that->m_stream.get();
        if (that->m_stream.get() == s) {
            that->m_stream = nullptr;
            that->m_sourceIndex = -1;
            that->m_sinkIndex = -1;
        }
        // Start a new recorder stream. Possibly on a different sink.
        that->startRecorder();
        break;
    case PA_STREAM_UNCONNECTED:
        qDebug() << "++++ STREAM UNCONNECTED" << s << that->m_stream.get();
        break;
    case PA_STREAM_CREATING:
        qDebug() << "++++ STREAM CREATING" << s << that->m_stream.get();
        break;
    case PA_STREAM_READY:
        qDebug() << "++++ STREAM READY" << s << that->m_stream.get();
        break;
    case PA_STREAM_FAILED:
        qDebug() << "++++ STREAM FAILED" << s << that->m_stream.get();
        break; // Don't care about any of those
    }
}

void StreamReader::startRecorder()
{
    Q_ASSERT(!m_stream);
    m_bufferIndex = 0;

    if (m_stream.reset(pa_stream_new(m_context.get(), "plasma-analyzer", &s_sampleSpec, nullptr)); !m_stream) {
        qWarning("pa_stream_new failed %s", pa_strerror(pa_context_errno(m_context.get())));
        return;
    }

    pa_stream_set_read_callback(m_stream.get(), cb_stream_read, this);
    pa_stream_set_state_callback(m_stream.get(), cb_stream_state, this);
    std::unique_ptr<pa_operation>(pa_context_get_source_info_list(m_context.get(), cb_get_source_info, this));

    if (m_sourceIndex < 0) {
        std::unique_ptr<pa_operation>(pa_context_get_sink_input_info_list(m_context.get(), cb_sink_input_info, this));
    }
}

void StreamReader::cb_context_state(pa_context *c, void *userdata)
{
    auto that = THAT(userdata);
    Q_ASSERT(c);

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
    case PA_CONTEXT_UNCONNECTED:
        break;

    case PA_CONTEXT_READY:
        qDebug() << "context ready";
        that->metaObject()->invokeMethod(that, [that]() {
            pa_context_set_subscribe_callback(that->m_context.get(), cb_subscription, that);
            pa_context_subscribe(that->m_context.get(), PA_SUBSCRIPTION_MASK_ALL, nullptr, that);
            that->poll();
        });
        break;

    case PA_CONTEXT_FAILED: // TODO
        qWarning("Context failure: %s", pa_strerror(pa_context_errno(c)));
        // TODO: Should reset itself to be started again

    case PA_CONTEXT_TERMINATED: // TODO
        qDebug("Context terminated");
        break;
    }
}

void StreamReader::cb_sink_input_info(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata)
{
#warning FIXME: this should re-schedule itself WHEN eol. static poll timer popping every 5s might flood with polls if the poll takes more than 5s
#warning fixme this should use a map during pool and then set it as the current list on eol. otherwise the data may change as the other thread reads the map
    auto that = THAT(userdata);

    qDebug() << Q_FUNC_INFO;

    if (eol) {
        that->metaObject()->invokeMethod(that, &StreamReader::newMusicScores);
        return;
    }

    qDebug() << i->name << pa_proplist_gets(i->proplist, "media.role");

    auto role = QString::fromLatin1(pa_proplist_gets(i->proplist, "media.role"));
    if (role != "music") {
        return;
    }

    auto sounds = that->m_musicsPerSink.value(i->sink, 0);
    that->m_musicsPerSink[i->sink] = sounds + 1;

    qDebug() << that->m_musicsPerSink;
}

void StreamReader::moveToSink(uint32_t sinkId)
{
    Q_ASSERT(sinkId != m_sinkIndex);

    qDebug() << "moving from" << m_sinkIndex << "to" << sinkId;

    m_sourceIndex = -1;
    m_sinkIndex = sinkId;

    // Disconnecting will cause a terminate signal which we'll act upon
    // by initializing a new stream.
    if (m_stream) {
        m_stream = nullptr;
    }
    startRecorder();
}

void StreamReader::newMusicScores()
{
    qDebug() << Q_FUNC_INFO;
    qint64 highestScore = -1;
    qint64 highestSink = -1;
    for (auto it = m_musicsPerSink.constBegin(); it != m_musicsPerSink.constEnd(); ++it) {
        if (highestScore < it.value()) {
            highestSink = it.key();
        }
    }
    if (highestSink >= 0 && highestSink != m_sinkIndex) {
        qDebug() << "!!!! MOVING TO SINK!";
        moveToSink(highestSink);
    }
}

static void pa_operation_callback(pa_operation *o, void *userdata)
{
    Q_ASSERT(o);
    Q_ASSERT(userdata);
    auto state = pa_operation_get_state(o);
    QString stateString;
    switch (state) {
    case PA_OPERATION_RUNNING: stateString = "running"; break;
    case PA_OPERATION_DONE: stateString = "done"; break;
    case PA_OPERATION_CANCELLED: stateString = "cancelled"; break;
    }
    qDebug() << o << stateString;
}

void StreamReader::poll()
{
    m_musicsPerSink.clear();

    std::unique_ptr<pa_operation> operation(pa_context_get_sink_input_info_list(m_context.get(), cb_sink_input_info, this));
    pa_operation_set_state_callback(operation.get(), pa_operation_callback, this);
}

void StreamReader::cb_subscription(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata)
{
    auto that = THAT(userdata);
    Q_UNUSED(idx);
    Q_ASSERT(c == that->m_context.get());

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
        QMetaObject::invokeMethod(that, &StreamReader::poll);
    }
}
