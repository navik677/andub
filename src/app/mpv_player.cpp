#include "mpv_player.hpp"
#include "screen_inhibitor.hpp"
#include "../services/upscaler.hpp"
#include "../utils/str_utils.hpp"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <atomic>
#include <clocale>
#include <mutex>

#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace anime::app {

// Lives as long as either the item (GUI thread) or its renderer (render
// thread) does; whichever goes last tears mpv down.
struct MpvShared {
    mpv_handle* mpv = nullptr;
    mpv_render_context* gl = nullptr;
    std::mutex mutex;
    MpvPlayer* item = nullptr;            // guarded by mutex; cleared when the item dies
    std::atomic<bool> events_queued{false};

    ~MpvShared() {
        if (mpv) mpv_terminate_destroy(mpv);
    }
};

namespace {

void* get_proc_address(void*, const char* name) {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    return ctx ? reinterpret_cast<void*>(ctx->getProcAddress(QByteArray(name))) : nullptr;
}

void on_render_update(void* ctx) {
    auto* s = static_cast<MpvShared*>(ctx);
    std::lock_guard lock(s->mutex);
    if (MpvPlayer* item = s->item) {
        QMetaObject::invokeMethod(item, [item] { item->update(); }, Qt::QueuedConnection);
    }
}

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(std::shared_ptr<MpvShared> s) : s_(std::move(s)) {}

    ~MpvRenderer() override {
        // Runs on the render thread with the GL context current
        if (s_->gl) {
            mpv_render_context_free(s_->gl);
            s_->gl = nullptr;
        }
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        if (!s_->gl && s_->mpv) {
            mpv_opengl_init_params gl_init{get_proc_address, nullptr};
            int advanced = 0;
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
                {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
                {MPV_RENDER_PARAM_INVALID, nullptr},
            };
            if (mpv_render_context_create(&s_->gl, s_->mpv, params) < 0) {
                qWarning("[MpvPlayer] failed to create mpv render context");
                s_->gl = nullptr;
            } else {
                mpv_render_context_set_update_callback(s_->gl, on_render_update, s_.get());
            }
        }
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override {
        if (!s_->gl) return;
        QOpenGLFramebufferObject* fbo = framebufferObject();
        mpv_opengl_fbo mfbo{static_cast<int>(fbo->handle()), fbo->width(), fbo->height(), 0};
        int flip_y = 0;
        int block = 0; // never stall the render thread waiting for frame timing
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(s_->gl, params);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    std::shared_ptr<MpvShared> s_;
};

void on_wakeup(void* ctx) {
    auto* s = static_cast<MpvShared*>(ctx);
    if (s->events_queued.exchange(true)) return;
    std::lock_guard lock(s->mutex);
    if (MpvPlayer* item = s->item) {
        QMetaObject::invokeMethod(item, "processEvents", Qt::QueuedConnection);
    }
}

} // namespace

MpvPlayer::MpvPlayer(QQuickItem* parent) : QQuickFramebufferObject(parent), shared_(std::make_shared<MpvShared>()) {
    inhibitor_ = new ScreenInhibitor(this);

    // mpv parses numbers with the C locale
    std::setlocale(LC_NUMERIC, "C");

    mpv_handle* mpv = mpv_create();
    if (!mpv) {
        qWarning("[MpvPlayer] mpv_create failed");
        return;
    }
    shared_->mpv = mpv;
    shared_->item = this;

    const char* opts[][2] = {
        {"terminal", "no"},
        {"msg-level", "all=warn"},
        {"vo", "libmpv"},
        // auto-copy keeps hardware decoding but avoids zero-copy GL interop,
        // which corrupts frames on some drivers when rendering into a foreign context
        {"hwdec", "auto-copy"},
        {"vd-lavc-threads", "4"},
        {"ytdl", "no"},          // must be set before init, avoids a ~20s yt-dlp probe
        {"load-scripts", "no"},
        {"keep-open", "yes"},
        {"cache", "yes"},
        {"cache-pause", "no"},
        {"cache-pause-wait", "1"},
        {"demuxer-readahead-secs", "60"},
        {"demuxer-max-bytes", "128MiB"},
        {"demuxer-max-back-bytes", "128MiB"},
        {"force-seekable", "yes"},
        {"hr-seek", "default"},
        {"hr-seek-framedrop", "yes"},
        {"stream-lavf-o", "reconnect=1,reconnect_streamed=1,reconnect_delay_max=5"},
    };
    for (auto& o : opts) mpv_set_option_string(mpv, o[0], o[1]);

    if (mpv_initialize(mpv) < 0) {
        qWarning("[MpvPlayer] mpv_initialize failed");
        return;
    }

    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "demuxer-cache-time", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "seeking", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "eof-reached", MPV_FORMAT_FLAG);

    mpv_set_wakeup_callback(mpv, on_wakeup, shared_.get());
}

MpvPlayer::~MpvPlayer() {
    {
        std::lock_guard lock(shared_->mutex);
        shared_->item = nullptr;
    }
    if (shared_->mpv) {
        mpv_set_wakeup_callback(shared_->mpv, nullptr, nullptr);
        const char* cmd[] = {"stop", nullptr};
        mpv_command(shared_->mpv, cmd);
    }
}

QQuickFramebufferObject::Renderer* MpvPlayer::createRenderer() const {
    return new MpvRenderer(shared_);
}

void MpvPlayer::load(const QString& url, const QVariantMap& headers) {
    mpv_handle* mpv = shared_->mpv;
    if (!mpv) return;

    QString user_agent, referrer;
    QStringList fields;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        const QString key = it.key().toLower();
        if (key == "user-agent") user_agent = it.value().toString();
        else if (key == "referer") referrer = it.value().toString();
        else fields << it.key() + ": " + it.value().toString();
    }
    mpv_set_property_string(mpv, "user-agent", user_agent.isEmpty() ? "libmpv" : user_agent.toUtf8().constData());
    mpv_set_property_string(mpv, "referrer", referrer.toUtf8().constData());
    // http-header-fields is a string list; set it as a node so commas inside values survive
    QList<QByteArray> raw;
    for (const auto& f : fields) raw << f.toUtf8();
    std::vector<mpv_node> nodes(static_cast<size_t>(raw.size()));
    for (int i = 0; i < raw.size(); ++i) {
        nodes[static_cast<size_t>(i)].format = MPV_FORMAT_STRING;
        nodes[static_cast<size_t>(i)].u.string = raw[i].data();
    }
    mpv_node_list list{static_cast<int>(nodes.size()), nodes.data(), nullptr};
    mpv_node node;
    node.format = MPV_FORMAT_NODE_ARRAY;
    node.u.list = &list;
    mpv_set_property(mpv, "http-header-fields", MPV_FORMAT_NODE, &node);

    const QByteArray u = url.toUtf8();
    const char* cmd[] = {"loadfile", u.constData(), nullptr};
    mpv_command_async(mpv, 0, cmd);
    setPaused(false);

    loading_ = true;
    eof_ = false;
    position_ = 0;
    duration_ = 0;
    buffered_ = 0;
    emit positionChanged();
    emit durationChanged();
    emit bufferedChanged();
    if (!has_media_) {
        has_media_ = true;
        emit hasMediaChanged();
    }
    updateBuffering();
    updateInhibit();
}

void MpvPlayer::stop() {
    if (!shared_->mpv) return;
    const char* cmd[] = {"stop", nullptr};
    mpv_command_async(shared_->mpv, 0, cmd);
    loading_ = false;
    if (has_media_) {
        has_media_ = false;
        emit hasMediaChanged();
    }
    updateBuffering();
    updateInhibit();
}

void MpvPlayer::setPaused(bool p) {
    if (!shared_->mpv) return;
    int flag = p ? 1 : 0;
    mpv_set_property_async(shared_->mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
    if (p != paused_) {
        paused_ = p;
        emit pausedChanged();
        updateInhibit();
    }
}

void MpvPlayer::setVolume(double v) {
    if (!shared_->mpv) return;
    v = std::clamp(v, 0.0, 100.0);
    mpv_set_property_async(shared_->mpv, 0, "volume", MPV_FORMAT_DOUBLE, &v);
    if (v != volume_) {
        volume_ = v;
        emit volumeChanged();
    }
}

void MpvPlayer::setMuted(bool m) {
    if (!shared_->mpv) return;
    int flag = m ? 1 : 0;
    mpv_set_property_async(shared_->mpv, 0, "mute", MPV_FORMAT_FLAG, &flag);
    if (m != muted_) {
        muted_ = m;
        emit mutedChanged();
    }
}

void MpvPlayer::seek(double seconds) {
    if (!shared_->mpv) return;
    if (duration_ > 0) seconds = std::clamp(seconds, 0.0, duration_);
    else seconds = std::max(0.0, seconds);
    const bool backward = seconds < position_;
    const QByteArray t = QByteArray::number(seconds, 'f', 3);
    const char* cmd[] = {"seek", t.constData(), "absolute", nullptr};
    mpv_command_async(shared_->mpv, 0, cmd);
    if (backward) {
        // Re-selecting the video track works around decoders (libopenh264)
        // that keep stale frames across a backward flush, desyncing A/V
        const char* off[] = {"set", "vid", "no", nullptr};
        const char* on[] = {"set", "vid", "auto", nullptr};
        mpv_command_async(shared_->mpv, 0, off);
        mpv_command_async(shared_->mpv, 0, on);
    }
    seek_target_ = seconds;
    position_ = seconds;
    eof_ = false;
    emit positionChanged();
}

void MpvPlayer::seekRelative(double offset) {
    const double base = seek_target_ >= 0 ? seek_target_ : position_;
    seek(base + offset);
}

void MpvPlayer::setUpscaler(const QString& id) {
    if (id == upscaler_) return;
    upscaler_ = id;
    emit upscalerChanged();
    applyUpscaler();
}

bool MpvPlayer::applyUpscaler() {
    if (!shared_->mpv) return false;
    const auto& profiles = Upscaler::profiles();
    return Upscaler::apply(shared_->mpv, profiles[Upscaler::index_of(upscaler_.toStdString())]);
}

void MpvPlayer::processEvents() {
    shared_->events_queued.store(false);
    mpv_handle* mpv = shared_->mpv;
    if (!mpv) return;
    while (true) {
        mpv_event* ev = mpv_wait_event(mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE) break;
        switch (ev->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto* p = static_cast<mpv_event_property*>(ev->data);
            handlePropertyChange(p->name, p->format, p->data);
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            loading_ = false;
            updateBuffering();
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            loading_ = false;
            seek_target_ = -1;
            updateBuffering();
            break;
        case MPV_EVENT_END_FILE: {
            auto* e = static_cast<mpv_event_end_file*>(ev->data);
            if (e->reason == MPV_END_FILE_REASON_ERROR) {
                loading_ = false;
                updateBuffering();
                emit loadFailed();
            }
            break;
        }
        default:
            break;
        }
    }
}

void MpvPlayer::handlePropertyChange(const char* name, int format, void* data) {
    const std::string n = name;
    const bool has = data && format != MPV_FORMAT_NONE;
    auto dbl = [&] { return has && format == MPV_FORMAT_DOUBLE ? *static_cast<double*>(data) : 0.0; };
    auto flag = [&] { return has && format == MPV_FORMAT_FLAG && *static_cast<int*>(data) != 0; };

    if (n == "time-pos") {
        if (seek_target_ >= 0) return; // keep the optimistic position until the seek lands
        position_ = dbl();
        emit positionChanged();
    } else if (n == "duration") {
        duration_ = dbl();
        emit durationChanged();
    } else if (n == "demuxer-cache-time") {
        buffered_ = dbl();
        emit bufferedChanged();
    } else if (n == "pause") {
        if (flag() != paused_) {
            paused_ = flag();
            emit pausedChanged();
            updateInhibit();
        }
    } else if (n == "volume") {
        if (has) {
            volume_ = dbl();
            emit volumeChanged();
        }
    } else if (n == "mute") {
        muted_ = flag();
        emit mutedChanged();
    } else if (n == "paused-for-cache") {
        cache_wait_ = flag();
        updateBuffering();
    } else if (n == "seeking") {
        seeking_ = flag();
        updateBuffering();
    } else if (n == "eof-reached") {
        const bool eof = flag();
        if (eof && !eof_ && duration_ > 0) emit endReached();
        eof_ = eof;
    }
}

void MpvPlayer::updateBuffering() {
    const bool b = has_media_ && (loading_ || cache_wait_ || seeking_);
    if (b != buffering_) {
        buffering_ = b;
        emit bufferingChanged();
    }
}

void MpvPlayer::updateInhibit() {
    inhibitor_->set(has_media_ && !paused_);
}

} // namespace anime::app
