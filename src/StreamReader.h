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

#ifndef STREAMREADER_H
#define STREAMREADER_H

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

// The amount of samples in a set depends on the bytes per sample.
// Since we have a 16bit pcm spec 2 bytes make one sample.
// Meaning we have 512 int16 *samples* in 1024 *bytes*
#define SAMPLE_BYTES 2048

class StreamReader : public QObject
{
    Q_OBJECT
public:
    static StreamReader *instance();
    ~StreamReader();

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

    // This fits 512 16bit samples. i.e. 1024 bytes!
    // This is the transfer buffer. The buffer is filled with data, once
    // a full sample set has been gathered the buffer is transferred into the
    // sample array and transferred to the Analyzer.
    int16_t m_buffer[SAMPLE_BYTES / 2];
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

#endif // STREAMREADER_H
