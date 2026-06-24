/*
 * Copyright (c) 2026 0000xFFFF.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * --- Integration notes ---
 * The decode logic here is mp3_to_bmp.c's libavformat/libavcodec/
 * libswresample pipeline, ported in as decode_peaks(). The key design
 * point for "resize doesn't freeze": decoding happens ONCE per track, on
 * a background std::thread, into a FIXED-resolution envelope (NUM_BUCKETS
 * columns) that gets cached to disk. paintEvent() and resizeEvent() never
 * touch libav at all -- they just resample the existing in-memory array to
 * whatever the widget's current pixel width happens to be, which is O(width)
 * and effectively instant. Resizing the dock is therefore just a repaint,
 * never a re-decode.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <libintl.h> /* must come before libaudcore/i18n.h: see note below */
#include <math.h>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWidget>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h> /* fine here now that real libintl.h was already parsed above */
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#define NUM_BUCKETS                                                            \
    2000 /* fixed cache/decode resolution, independent of widget width */

/* ================= peak data + decode (ported from mp3_to_bmp.c)
 * ================= */

struct TrackPeaks
{
    std::vector<int16_t> mins, maxs;
    int32_t global_peak = 1;
    double duration_sec = 0;
};

static std::mutex g_mutex;
static std::shared_ptr<TrackPeaks> g_peaks; /* envelope currently displayed */
static std::string g_current_file; /* file the envelope above belongs to */

static std::string cache_path_for(const std::string & filename)
{
    /* simple FNV-1a hash of the path -> cache filename */
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : filename)
    {
        h ^= c;
        h *= 1099511628211ULL;
    }

    char hex[17];
    snprintf(hex, sizeof hex, "%016llx", (unsigned long long)h);

    std::string dir =
        std::string(aud_get_path(AudPath::UserDir)) + "/waveform_cache";
    mkdir(dir.c_str(), 0755);
    return dir + "/" + hex + ".peaks";
}

static bool load_cache(const std::string & path, TrackPeaks & out)
{
    FILE * f = fopen(path.c_str(), "rb");
    if (!f)
        return false;

    int32_t n = 0;
    bool ok = fread(&n, sizeof n, 1, f) == 1 && n == NUM_BUCKETS;
    if (ok)
    {
        out.mins.resize(n);
        out.maxs.resize(n);
        ok = fread(&out.global_peak, sizeof out.global_peak, 1, f) == 1 &&
             fread(&out.duration_sec, sizeof out.duration_sec, 1, f) == 1 &&
             fread(out.mins.data(), sizeof(int16_t), n, f) == (size_t)n &&
             fread(out.maxs.data(), sizeof(int16_t), n, f) == (size_t)n;
    }
    fclose(f);
    return ok;
}

static void save_cache(const std::string & path, const TrackPeaks & in)
{
    FILE * f = fopen(path.c_str(), "wb");
    if (!f)
        return;
    int32_t n = (int32_t)in.mins.size();
    fwrite(&n, sizeof n, 1, f);
    fwrite(&in.global_peak, sizeof in.global_peak, 1, f);
    fwrite(&in.duration_sec, sizeof in.duration_sec, 1, f);
    fwrite(in.mins.data(), sizeof(int16_t), n, f);
    fwrite(in.maxs.data(), sizeof(int16_t), n, f);
    fclose(f);
}

/* Same algorithm as mp3_to_bmp.c's decode loop: open via libavformat, find
 * the audio stream, decode every packet through the matching libavcodec
 * decoder, resample to mono S16 via libswresample, then bucket into
 * NUM_BUCKETS min/max columns. Runs entirely off the GUI thread. */
static bool decode_peaks(const std::string & filename, TrackPeaks & out)
{
    AVFormatContext * fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filename.c_str(), nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    int audio_idx = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_idx = i;
            break;
        }

    if (audio_idx < 0)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecParameters * params = fmt_ctx->streams[audio_idx]->codecpar;
    const AVCodec * codec = avcodec_find_decoder(params->codec_id);
    if (!codec)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecContext * ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, params);
    if (avcodec_open2(ctx, codec, nullptr) < 0)
    {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, 1); /* downmix to mono */

    SwrContext * swr = nullptr;
    if (swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_S16,
                            ctx->sample_rate, &ctx->ch_layout, ctx->sample_fmt,
                            ctx->sample_rate, 0, nullptr) < 0 ||
        !swr || swr_init(swr) < 0)
    {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    std::vector<int16_t> samples;
    samples.reserve((size_t)ctx->sample_rate * 60 *
                    4); /* ~4 min headroom up front */

    AVPacket * pkt = av_packet_alloc();
    AVFrame * frame = av_frame_alloc();
    std::vector<int16_t> conv_buf;

    auto pump_frame = [&](AVFrame * fr) {
        int max_out = swr_get_out_samples(swr, fr->nb_samples);
        if ((int)conv_buf.size() < max_out)
            conv_buf.resize(max_out);
        uint8_t * planes[1] = {(uint8_t *)conv_buf.data()};
        int got = swr_convert(swr, planes, max_out, (const uint8_t **)fr->data,
                              fr->nb_samples);
        if (got > 0)
            samples.insert(samples.end(), conv_buf.begin(),
                           conv_buf.begin() + got);
    };

    while (av_read_frame(fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == audio_idx &&
            avcodec_send_packet(ctx, pkt) == 0)
            while (avcodec_receive_frame(ctx, frame) == 0)
                pump_frame(frame);
        av_packet_unref(pkt);
    }
    avcodec_send_packet(ctx, nullptr);
    while (avcodec_receive_frame(ctx, frame) == 0)
        pump_frame(frame);

    int drain;
    do
    {
        if (conv_buf.empty())
            conv_buf.resize(1024);
        uint8_t * planes[1] = {(uint8_t *)conv_buf.data()};
        drain = swr_convert(swr, planes, (int)conv_buf.size(), nullptr, 0);
        if (drain > 0)
            samples.insert(samples.end(), conv_buf.begin(),
                           conv_buf.begin() + drain);
    } while (drain > 0);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr);
    avcodec_free_context(&ctx);
    int sample_rate = fmt_ctx->streams[audio_idx]->codecpar->sample_rate;
    avformat_close_input(&fmt_ctx);

    if (samples.empty())
        return false;

    out.mins.assign(NUM_BUCKETS, 0);
    out.maxs.assign(NUM_BUCKETS, 0);
    out.global_peak = 1;
    out.duration_sec = (double)samples.size() / sample_rate;

    for (int b = 0; b < NUM_BUCKETS; b++)
    {
        size_t start = (size_t)((double)b / NUM_BUCKETS * samples.size());
        size_t end = (size_t)((double)(b + 1) / NUM_BUCKETS * samples.size());
        if (end > samples.size())
            end = samples.size();
        if (end <= start)
            end = start + 1;
        if (end > samples.size())
            end = samples.size();

        int16_t mn = 0, mx = 0;
        for (size_t i = start; i < end; i++)
        {
            if (samples[i] < mn)
                mn = samples[i];
            if (samples[i] > mx)
                mx = samples[i];
        }
        out.mins[b] = mn;
        out.maxs[b] = mx;
        if (-mn > out.global_peak)
            out.global_peak = -mn;
        if (mx > out.global_peak)
            out.global_peak = mx;
    }

    return true;
}

/* ================= background loading + thread-safe handoff =================
 */

class WaveformWidget; /* fwd decl, used by the loader to dispatch back to the
                         GUI thread */
static WaveformWidget * spect_widget = nullptr;

static void load_track_async(std::string filename);

/* ================= widget ================= */

class WaveformWidget : public QWidget
{
public:
    WaveformWidget(QWidget * parent = nullptr);
    ~WaveformWidget();

    /* called (via QMetaObject::invokeMethod, queued onto the GUI thread)
     * once a background decode/cache-load finishes */
    void apply_peaks(const std::string & filename,
                     std::shared_ptr<TrackPeaks> peaks);

protected:
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

private:
    void paint_background(QPainter &);
    void paint_waveform(QPainter &);
    void paint_playhead(QPainter &);
    void seek_to_x(int x);

    QTimer * m_timer;
};

WaveformWidget::WaveformWidget(QWidget * parent) : QWidget(parent)
{
    spect_widget = this;

    /* redraw periodically so the playhead animates during playback.
     * This is just a repaint -- no decoding happens here. */
    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, this, [this]() { update(); });
    m_timer->start(100); /* 10 Hz */

    setMinimumHeight(80);

    /* if a track is already playing when the dock is created, show it
     * (this also primes the cache for next time even if it was a miss) */
    if (aud_drct_get_ready())
    {
        String fn = aud_drct_get_filename();
        if (fn)
            load_track_async((const char *)fn);
    }
}

WaveformWidget::~WaveformWidget() { spect_widget = nullptr; }

void WaveformWidget::apply_peaks(const std::string & filename,
                                 std::shared_ptr<TrackPeaks> peaks)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_current_file = filename;
        g_peaks = peaks;
    }
    update();
}

void WaveformWidget::paint_background(QPainter & p)
{
    p.fillRect(0, 0, width(), height(), QColor(18, 18, 20));
}

void WaveformWidget::paint_waveform(QPainter & p)
{
    std::shared_ptr<TrackPeaks> peaks;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        peaks = g_peaks;
    }

    int w = width(), h = height();
    int center_y = h / 2;

    /* centerline */
    p.setPen(QColor(90, 90, 95));
    p.drawLine(0, center_y, w, center_y);

    if (!peaks || peaks->mins.empty())
        return;

    /* Resample the fixed-resolution cached envelope to however many
     * pixels wide we are RIGHT NOW. This is the only per-resize cost:
     * O(width) array lookups, no decoding, no I/O. */
    double half = h / 2.0;
    double scale = (half * 0.95) / (double)peaks->global_peak;
    int n = (int)peaks->mins.size();

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(64, 200, 255));

    for (int x = 0; x < w; x++)
    {
        int idx = x * n / w;
        if (idx >= n)
            idx = n - 1;
        double top = center_y - peaks->maxs[idx] * scale;
        double bot = center_y - peaks->mins[idx] * scale;
        if (bot < top)
            std::swap(top, bot);
        p.drawRect(QRectF(x, top, 1.0, bot - top + 1.0));
    }
}

void WaveformWidget::paint_playhead(QPainter & p)
{
    if (!aud_drct_get_playing())
        return;

    int length = aud_drct_get_length();
    if (length <= 0)
        return;

    int time = aud_drct_get_time();
    double frac = (double)time / length;
    if (frac < 0)
        frac = 0;
    if (frac > 1)
        frac = 1;

    double x = frac * width();

    QPen pen(QColor(255, 40, 40));
    pen.setWidth(2);
    p.setPen(pen);
    p.drawLine(QPointF(x, 0), QPointF(x, height()));
}

void WaveformWidget::resizeEvent(QResizeEvent *)
{
    /* intentionally does nothing but trigger a repaint -- paint_waveform()
     * resamples the existing cached peaks array to the new width, so this
     * stays smooth even while you're actively dragging the dock's edge */
    update();
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paint_background(p);
    paint_waveform(p);
    paint_playhead(p);
}

void WaveformWidget::seek_to_x(int x)
{
    int length = aud_drct_get_length();
    if (length <= 0 || width() <= 0)
        return;

    double frac = (double)x / width();
    if (frac < 0)
        frac = 0;
    if (frac > 1)
        frac = 1;

    aud_drct_seek((int)(frac * length));
    update();
}

void WaveformWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
        seek_to_x(event->pos().x());
}

void WaveformWidget::mouseMoveEvent(QMouseEvent * event)
{
    /* Qt only delivers move events while a button is held (no
     * setMouseTracking needed), so this gives click-and-drag scrubbing
     * across the waveform for free. */
    if (event->buttons() & Qt::LeftButton)
        seek_to_x(event->pos().x());
}

/* ================= async load: cache hit is instant, miss decodes on a thread
 * ================= */

static void load_track_async(std::string filename)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (filename == g_current_file)
            return; /* already showing this track */
    }

    std::thread([filename]() {
        auto peaks = std::make_shared<TrackPeaks>();
        std::string cpath = cache_path_for(filename);

        bool ok = load_cache(cpath, *peaks);
        if (!ok)
        {
            StringBuf local = uri_to_filename(filename.c_str());
            std::string decode_path =
                local ? std::string((const char *)local) : filename;

            ok = decode_peaks(decode_path, *peaks);
            if (ok)
                save_cache(cpath, *peaks);
        }
        if (!ok)
            return; /* leave whatever was showing rather than blanking it */

        /* hop back onto the GUI thread before touching the widget */
        QMetaObject::invokeMethod(
            qApp,
            [filename, peaks]() {
                if (spect_widget)
                    spect_widget->apply_peaks(filename, peaks);
                else
                {
                    /* dock isn't open right now -- still cache the result for
                     * next time the widget appears */
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_current_file = filename;
                    g_peaks = peaks;
                }
            },
            Qt::QueuedConnection);
    }).detach();
}

static void on_playback_ready(void *, void *)
{
    String fn = aud_drct_get_filename();
    if (fn)
        load_track_async((const char *)fn);
}

/* ================= plugin glue ================= */

class QtWaveform : public VisPlugin
{
public:
    static constexpr PluginInfo info = {N_("Waveform"), PACKAGE,
                                        nullptr, // about
                                        nullptr, // prefs
                                        PluginQtOnly};

    constexpr QtWaveform() : VisPlugin(info, Visualizer::Freq) {}

    bool init() override;
    void cleanup() override;

    void * get_qt_widget() override;

    void clear() override;
    void render_freq(const float * freq) override;
};

EXPORT QtWaveform aud_plugin_instance;

bool QtWaveform::init()
{
    hook_associate("playback ready", (HookFunction)on_playback_ready, nullptr);
    return true;
}

void QtWaveform::cleanup()
{
    hook_dissociate("playback ready", (HookFunction)on_playback_ready);
}

void QtWaveform::render_freq(const float * freq)
{ /* unused -- we decode independently via libav */ }

void QtWaveform::clear()
{
    if (spect_widget)
        spect_widget->update();
}

void * QtWaveform::get_qt_widget()
{
    if (spect_widget)
        return spect_widget;

    spect_widget = new WaveformWidget();
    return spect_widget;
}