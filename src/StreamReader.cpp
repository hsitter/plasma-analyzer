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

#include "StreamReader.h"

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>

#include <QTime>
#include <QThread>

#define THAT(x) Q_ASSERT(x); auto that = static_cast<StreamReader *>(x);

#warning FIXME: still used lots of static crap
static pa_sample_spec s_sampleSpec = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 1
};

Q_DECLARE_METATYPE(QSharedPointer<qint16>)
Q_DECLARE_METATYPE(QList<qint16>)

StreamReader::StreamReader()
    : m_mainloop(nullptr)
    , m_thread(new QThread(this))
{
    qDebug();
    // Register so we can queue it across threads
    qRegisterMetaType<QSharedPointer<qint16>>();
    qRegisterMetaType<QList<qint16>>();

    // This starts a context, Context must be singleton within an app! calling start twice will most certainly crash
    if (!(m_mainloop = pa_threaded_mainloop_new())) {
        qFatal("pa_mainloop_new failed");
        return;
    }

    if (!(m_mainloopAPI = pa_threaded_mainloop_get_api(m_mainloop))) {
        qFatal("pa_threaded_mainloop_get_api failed");
        return;
    }

    if (pa_signal_init(m_mainloopAPI) != 0) {
        qFatal("pa_signal_init failed");
        return;
    }

    if (!(m_context = pa_context_new(m_mainloopAPI, "plasma-analyzer"))) {
        qFatal("pa_context_new failed");
        return;
    }

    if (pa_context_connect(m_context, nullptr, PA_CONTEXT_NOFAIL, nullptr ) != 0) {
        qFatal("pa_context_connect failed");
        return;
    }
    pa_context_set_state_callback(m_context, cb_context_state, this);

    pa_threaded_mainloop_start(m_mainloop);

#warning fixme should probably movetothread and then queue init() where we setup the entire shebang?

    m_pollTimer.setInterval(2500);
    connect(&m_pollTimer, &QTimer::timeout, this, &StreamReader::poll);

#warning fimxe qml thinks all items are in the gui thread and direct connects them. this extends to singletons as a result we can't thread if we want to fft in qml
//    m_pollTimer.moveToThread(m_thread);

    // The reader is a singleton and not parented by anyone, so we are free to
    // thread it.
    // And we do so because
    // a) it makes sure signals are auto-queued (when we emit from a pulse
    //    callback as well, which ordinarily woudln't be detected I think)
    // b) everything doing work up to the Model should be threaded so as to
    //    avoid contending GUI thread resources for our maths magic.
//    moveToThread(m_thread);
//    m_thread->start();
}

StreamReader::~StreamReader()
{
    qDebug() << "dtor";
    m_shuttingDown = 1;

    if (m_stream) {
        pa_stream_disconnect(m_stream);
        pa_stream_unref(m_stream);
        m_stream = nullptr;
    }
    if (m_context) { // Context also stops the mainloop, nee
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        m_context = nullptr;
    }
    if (m_mainloop) {
        pa_threaded_mainloop_stop(m_mainloop);
        pa_threaded_mainloop_free(m_mainloop);
        m_mainloop = nullptr;
        m_mainloopAPI = nullptr; // Doesn't need unreffing or anything it seems.
    }

    m_thread->quit();
    m_thread->wait();
}

QList<int> StreamReader::samplesVector()
{
    QReadLocker l(&m_samplesRWLock);
    QList<int> ret;
    if (!m_sharedSamples) {
        qDebug() << "no data";
        return ret;
    }
    qDebug() << "returning data";
    for (int i = 0; i < SAMPLE_BYTES / 2; ++i) { // 2 is the sample size
        qint16 s = m_sharedSamples.data()[i];
        ret.append(s);
    }
    return ret;
}

StreamReader *StreamReader::instance()
{
    // NB: this is on the heap because QML will free us. Somewhat fishy, but
    //   whatevs.
    // might be better to make this a global static which I think won't be
    // causing trouble with qml but also get delted when not used from qml
    static StreamReader *self = new StreamReader;
    return self;
}

void StreamReader::cb_get_source_info(pa_context *c, const pa_source_info *i, int eol, void *userdata)
{
    if (eol != 0) {
        qDebug() << "source eol";
        return;
    }

    Q_ASSERT(c);
    Q_ASSERT(i);
    THAT(userdata);

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

    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = -1;
    buffer_attr.tlength = -1;
    buffer_attr.prebuf = -1;
    buffer_attr.minreq = -1;
    buffer_attr.fragsize = pa_usec_to_bytes(1000, &s_sampleSpec);

    qDebug() << "fragsize" << buffer_attr.fragsize;
    qDebug() << "bytes per frame" << pa_frame_size(&s_sampleSpec);
    qDebug() << "sample size" << pa_sample_size(&s_sampleSpec);

    if ((pa_stream_connect_record(that->m_stream, i->name, &buffer_attr, PA_STREAM_ADJUST_LATENCY)) < 0) {
        qFatal("pa_stream_connect_record failed: %s\n", pa_strerror(pa_context_errno(that->m_context)));
        return;
    }
}

void StreamReader::cb_stream_read(pa_stream *s, size_t length, void *userdata)
{
    Q_ASSERT(s);
    Q_ASSERT(length > 0);
    THAT(userdata);
    that->read(s, length);
}

// This is called from PA thread.
void StreamReader::read(pa_stream *s, size_t length)
{
    // We read into an array here because we need to feed this into
    // the FFT later, so using a higher level container such as QVector or QBA
    // would only get in the way and have no use up unti after the FFT, where
    // we get new output data anyway.

#warning fixme the math here is a bit garbage ... everywhere '2' is pa_sample_size(=2 bytes per sample) we could just use int8 and then cast that to int16. avoids us having to multiply/divide by the sample size all the time
#warning fixme the snapshot crap needs to reset if we switch source
    const void *data;
    if (pa_stream_peek(s, &data, &length) < 0) {
        qFatal("pa_stream_peek failed: %s", pa_strerror(pa_context_errno(m_context)));
        return;
    }

    int overflow = m_bufferIndex * 2 + length - (SAMPLE_BYTES);

    if (overflow < 0) {
        overflow = 0;
    }

    if (!m_samples) {
        m_samples = (qint16 *)malloc(sizeof(m_samples) * (SAMPLE_BYTES / 2));
    }
    memcpy(m_buffer + m_bufferIndex, data, length - overflow);
    m_bufferIndex += (length - overflow) / 2;
    pa_stream_drop(s);

//    qDebug() << "read: " << "length" << length << "m_bufferIndex" << m_bufferIndex << "overflow"  << overflow  << "copied deleta" << (length - overflow);

    if (!overflow) {
        return;
    }

    memcpy(m_samples, m_buffer, m_bufferIndex * 2);
    m_bufferIndex = 0;

    // 20 updates per second should be plenty. Discard data sets arriving
    // in excess of that. This prevents too many updates, which makes the bars
    // look like on drugs as well as putting severe strain on everything,
    // while still being regular enough to look fluid. Could possibly become
    // a user setting at some point?
    if (m_readTime.elapsed() < 80) {
        return;
    }
    m_readTime.restart();


    // Put the array into a shared pointer so we can continue reading into
    // m_samples. This is a shared pointer because it is sent to 1:N Analyzers.
#warning fixme a short ringbuffer may be better guarded against contention
    QWriteLocker l(&m_samplesRWLock);
    m_sharedSamples = QSharedPointer<int16_t>(m_samples);
    m_samples = nullptr;
    qDebug() << "pumpgin read";
    emit readyRead();
}

void StreamReader::cb_stream_state(pa_stream *s, void *userdata)
{
    Q_ASSERT(s);
    THAT(userdata);

    if (that->m_shuttingDown) {
        return; // Already shutting down reader. Nothing to do.
    }

    switch (pa_stream_get_state(s)) {
    case PA_STREAM_TERMINATED:
        qDebug() << "++++ STREAM TERMINATED" << s << that->m_stream;
        if (that->m_stream == s) {
            pa_stream_unref(that->m_stream);
            that->m_stream = nullptr;
            that->m_sourceIndex = -1;
            that->m_sinkIndex = -1;
        }
        // Start a new recorder stream. Possibly on a different sink.
        that->startRecorder();
    case PA_STREAM_UNCONNECTED:
        qDebug() << "++++ STREAM UNCONNECTED" << s << that->m_stream;
    case PA_STREAM_CREATING:
        qDebug() << "++++ STREAM CREATING" << s << that->m_stream;
    case PA_STREAM_READY:
        qDebug() << "++++ STREAM READY" << s << that->m_stream;
    case PA_STREAM_FAILED:
        qDebug() << "++++ STREAM FAILED" << s << that->m_stream;
        break; // Don't care about any of those
    }
}

void StreamReader::startRecorder()
{
    // FIXME: this may need a lock as it can be called from callbacks and
    // our qtimer.
    Q_ASSERT(!m_stream);
    m_bufferIndex = 0;

    if (!(m_stream = pa_stream_new(m_context, "plasma-analyzer", &s_sampleSpec, nullptr))) {
        qFatal("pa_stream_new failed %s", pa_strerror(pa_context_errno(m_context)));
        return;
    }

    pa_stream_set_read_callback(m_stream, cb_stream_read, this);
    pa_stream_set_state_callback(m_stream, cb_stream_state, this);
    pa_operation_unref(pa_context_get_source_info_list(m_context, cb_get_source_info, this));

#warning fixme this should run in context ready this also needs fallback logic though. if no useful sink input is present the auto-connect should run (-1 -1)
    // run sink input poll on first start
    if (m_sourceIndex < 0) {
        pa_operation_unref(pa_context_get_sink_input_info_list(m_context, cb_sink_input_info, this));
    }
}

void StreamReader::cb_context_state(pa_context *c, void *userdata)
{
    Q_ASSERT(c);
    THAT(userdata);

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
    case PA_CONTEXT_UNCONNECTED:
        break;

    case PA_CONTEXT_READY:
        qDebug() << "context ready";
        that->metaObject()->invokeMethod(that, [that]() {
            // Instantly run a poll (in the right thread) and then start
            // the timer. This means we get onto a good device ASAP because of
            // the manual poll. After that we'll keep our scores up to date
            // via the timed polling.
            that->poll();
            that->m_pollTimer.start();
        });
        break;

    case PA_CONTEXT_FAILED: // TODO
        qFatal("Context failure: %s", pa_strerror(pa_context_errno(c)));
        // TODO: Should reset itself to be started again

    case PA_CONTEXT_TERMINATED: // TODO
        qDebug("Context terminated");
        if (!that->m_shuttingDown) {
            // If we get this mid-flight it indicates a major problem.
            qFatal("Context terminated");
            // TODO: Should reset itself to be started again
        }
        break;
    }
}

void StreamReader::cb_sink_input_info(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata)
{
#warning FIXME: this should re-schedule itself WHEN eol. static poll timer popping every 5s might flood with polls if the poll takes more than 5s
#warning fixme this should use a map during pool and then set it as the current list on eol. otherwise the data may change as the other thread reads the map
    THAT(userdata);

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
        auto stream = m_stream;
        // reset this first, concurrently this triggers a terminate signal.
        // We need to be null by the time that runs!
        m_stream = nullptr;
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);
    } else {
        startRecorder();
    }
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
        moveToSink(highestSink);
    }
    m_musicsPerSinkMutex.unlock();
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


// FIXME: it'd be better if we simply got events to look at it instead of poll for no good reason
void StreamReader::poll()
{
    // NB: this is run in the QThread, the actual callback will be in a PA
    //   thread, since the two might run at the same time (depending on timing)
    //   we need proper locking of our resource!
    // We'll lock the mutex here, then initiate the query, its callback will
    // then call newMusicScores where the mutex is released again.
    // Until the mutex is free we'll skip all polls.
    if (!m_musicsPerSinkMutex.tryLock()) {
        qDebug() << "poll skip";
        // Couldn't lock. Skip polll to prevent contention and raise poll
        // interval to reduce load. On successful locks this gets reduced again.
        m_pollTimer.setInterval(qMax(m_pollTimer.interval() * 1.25, 20 * 1000.0));
        return;
    }
    // TODO: could possibly stop the timer and restart on return. that way
    //  we'd not get overwhelmbed by calls if locking the mutex takes too long
    qDebug () << "poll";
    m_pollTimer.setInterval(qMax(m_pollTimer.interval() * 0.75, 5 * 1000.0));
    m_musicsPerSink.clear();
#warning fixme for some reason the cb stop triggering at some point (may be load related?) then we are stuck since we still have the lock but the callback isn't running so it's never getting unlocked
    // may be necessary to have a second time which reclaims the lock if the callback isn't firing within a minute?
    // maybe the inovke method on newmusicscores is garbage. in the log I am seeing it bugged out after newmusicscores
    // so it could be that newmusicscores releases the lock but the callback hasn't returned yet,
    // meanwhile we claim the lock here and want to initiate the callback but pa
    // refuses to do so because a callback is already in progress?
    // supposedly the pa operation should tell us more?
    // could attach a notfy callback
    auto o = pa_context_get_sink_input_info_list(m_context, cb_sink_input_info, this);
    qDebug() << "starting operation" << o;
    pa_operation_set_state_callback(o, pa_operation_callback, this);
    pa_operation_unref(o);

    // FIXME: instaed of locking we could just listen to the operation state changes.
    //   we stop the timer when the poll starts and start it when the operation
    //   goes to done or cancelled. that way we'd need no lock since the timer
    //   prevents extra runs?
    //   context ready also calls poll directly though which may still need
    //   the lock. the timer adjustments we could drop though.
    //   (as the operation is async we cannot rely on the eventloop for
    //   syncronization of a manual poll call and a timed poll call)
}
