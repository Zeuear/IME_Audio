#include "AudioFileDecoder.h"

#ifdef IMEAUDIO_HAS_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <memory>
#include "utils/Logger.h"

namespace {

QString avError(int code)
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

struct FormatCloser { void operator()(AVFormatContext* p) const { avformat_close_input(&p); } };
struct CodecFreer   { void operator()(AVCodecContext* p) const { avcodec_free_context(&p); } };
struct SwrFreer     { void operator()(SwrContext* p) const { swr_free(&p); } };
struct PacketFreer  { void operator()(AVPacket* p) const { av_packet_free(&p); } };
struct FrameFreer   { void operator()(AVFrame* p) const { av_frame_free(&p); } };

AudioFileDecoder::Result failed(const QString& cause, int code = 0)
{
    if (code < 0) LOG_WARN(QString("FFmpeg decode failed: %1 (%2)").arg(cause, avError(code)));
    return {{}, cause};
}

struct OpenedAudio {
    std::unique_ptr<AVFormatContext, FormatCloser> fmt;
    std::unique_ptr<AVCodecContext, CodecFreer> dec;
    int streamIndex = -1;
    QString error;
};

// 打开文件并准备好音频流解码器；其它流（m4a 的封面图、视频画面）直接丢弃，省去无用的解复用
OpenedAudio openAudio(const QString& path)
{
    OpenedAudio r;
    // FFmpeg 在 Windows 上按 UTF-8 解释路径，中文文件名可直接打开
    AVFormatContext* rawFmt = nullptr;
    int ret = avformat_open_input(&rawFmt, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) { r.error = failed(QStringLiteral("无法打开文件"), ret).error; return r; }
    r.fmt.reset(rawFmt);

    if ((ret = avformat_find_stream_info(r.fmt.get(), nullptr)) < 0) {
        r.error = failed(QStringLiteral("无法识别文件格式"), ret).error;
        return r;
    }

    const AVCodec* codec = nullptr;
    r.streamIndex = av_find_best_stream(r.fmt.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (r.streamIndex < 0) { r.error = failed(QStringLiteral("文件中没有音频"), r.streamIndex).error; return r; }

    r.dec.reset(avcodec_alloc_context3(codec));
    avcodec_parameters_to_context(r.dec.get(), r.fmt->streams[r.streamIndex]->codecpar);
    if ((ret = avcodec_open2(r.dec.get(), codec, nullptr)) < 0) {
        r.error = failed(QStringLiteral("不支持该音频编码"), ret).error;
        return r;
    }

    for (unsigned i = 0; i < r.fmt->nb_streams; ++i)
        if (int(i) != r.streamIndex) r.fmt->streams[i]->discard = AVDISCARD_ALL;
    return r;
}

}

bool AudioFileDecoder::isAvailable()
{
    return true;
}

AudioFileDecoder::Result AudioFileDecoder::decode(const QString& path, int targetSampleRate,
                                                  const std::atomic_bool* cancel)
{
    OpenedAudio in = openAudio(path);
    if (!in.error.isEmpty()) return {{}, in.error};
    int streamIndex = in.streamIndex;
    auto& dec = in.dec;
    auto& fmt = in.fmt;
    int ret = 0;

    AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
    SwrContext* rawSwr = nullptr;
    ret = swr_alloc_set_opts2(&rawSwr, &mono, AV_SAMPLE_FMT_S16, targetSampleRate,
                              &dec->ch_layout, dec->sample_fmt, dec->sample_rate, 0, nullptr);
    std::unique_ptr<SwrContext, SwrFreer> swr(rawSwr);
    if (ret < 0 || (ret = swr_init(swr.get())) < 0)
        return failed(QStringLiteral("音频重采样初始化失败"), ret);

    Result r;
    // 按时长预留输出，避免几十万次追加反复扩容（1 小时约 110 MB）
    if (fmt->duration > 0)
        r.pcm.reserve(int(double(fmt->duration) / AV_TIME_BASE * targetSampleRate * 2) + targetSampleRate * 2);

    std::unique_ptr<AVPacket, PacketFreer> pkt(av_packet_alloc());
    std::unique_ptr<AVFrame, FrameFreer> frame(av_frame_alloc());

    auto convert = [&](const uint8_t** in, int inCount) {
        const int maxOut = swr_get_out_samples(swr.get(), inCount);
        if (maxOut <= 0) return;
        const qsizetype old = r.pcm.size();
        r.pcm.resize(old + maxOut * 2);
        auto* out = reinterpret_cast<uint8_t*>(r.pcm.data() + old);
        const int got = swr_convert(swr.get(), &out, maxOut, in, inCount);
        r.pcm.resize(old + qMax(0, got) * 2);
    };
    auto drainFrames = [&]() {
        while (avcodec_receive_frame(dec.get(), frame.get()) == 0) {
            convert(const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
            av_frame_unref(frame.get());
        }
    };

    while (av_read_frame(fmt.get(), pkt.get()) >= 0) {
        if (cancel && cancel->load()) return failed(QStringLiteral("已取消"));
        // 个别损坏的包解不出来就跳过，不让整个文件失败
        if (pkt->stream_index == streamIndex && avcodec_send_packet(dec.get(), pkt.get()) == 0)
            drainFrames();
        av_packet_unref(pkt.get());
    }
    avcodec_send_packet(dec.get(), nullptr);   // 冲出解码器缓存的最后几帧
    drainFrames();
    convert(nullptr, 0);                       // 冲出重采样器里剩余的样本

    if (r.pcm.isEmpty()) return failed(QStringLiteral("没有解码出音频数据"));
    return r;
}

AudioFileDecoder::StereoResult AudioFileDecoder::decodeStereo(const QString& path, int sampleRate,
                                                              const std::atomic_bool* cancel)
{
    StereoResult r;
    OpenedAudio in = openAudio(path);
    if (!in.error.isEmpty()) { r.error = in.error; return r; }

    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext* rawSwr = nullptr;
    int ret = swr_alloc_set_opts2(&rawSwr, &stereo, AV_SAMPLE_FMT_FLTP, sampleRate,
                                  &in.dec->ch_layout, in.dec->sample_fmt, in.dec->sample_rate, 0, nullptr);
    std::unique_ptr<SwrContext, SwrFreer> swr(rawSwr);
    if (ret < 0 || (ret = swr_init(swr.get())) < 0) {
        r.error = failed(QStringLiteral("音频重采样初始化失败"), ret).error;
        return r;
    }

    if (in.fmt->duration > 0) {
        const size_t expect = size_t(double(in.fmt->duration) / AV_TIME_BASE * sampleRate) + sampleRate;
        r.left.reserve(expect);
        r.right.reserve(expect);
    }

    std::unique_ptr<AVPacket, PacketFreer> pkt(av_packet_alloc());
    std::unique_ptr<AVFrame, FrameFreer> frame(av_frame_alloc());

    auto convert = [&](const uint8_t** inData, int inCount) {
        const int maxOut = swr_get_out_samples(swr.get(), inCount);
        if (maxOut <= 0) return;
        const size_t old = r.left.size();
        r.left.resize(old + maxOut);
        r.right.resize(old + maxOut);
        uint8_t* planes[2] = {reinterpret_cast<uint8_t*>(r.left.data() + old),
                              reinterpret_cast<uint8_t*>(r.right.data() + old)};
        const int got = swr_convert(swr.get(), planes, maxOut, inData, inCount);
        r.left.resize(old + qMax(0, got));
        r.right.resize(old + qMax(0, got));
    };
    auto drainFrames = [&]() {
        while (avcodec_receive_frame(in.dec.get(), frame.get()) == 0) {
            convert(const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
            av_frame_unref(frame.get());
        }
    };

    while (av_read_frame(in.fmt.get(), pkt.get()) >= 0) {
        if (cancel && cancel->load()) { r.left.clear(); r.right.clear(); r.error = QStringLiteral("已取消"); return r; }
        if (pkt->stream_index == in.streamIndex && avcodec_send_packet(in.dec.get(), pkt.get()) == 0)
            drainFrames();
        av_packet_unref(pkt.get());
    }
    avcodec_send_packet(in.dec.get(), nullptr);
    drainFrames();
    convert(nullptr, 0);

    if (r.left.empty()) r.error = QStringLiteral("没有解码出音频数据");
    return r;
}

#else

bool AudioFileDecoder::isAvailable()
{
    return false;
}

AudioFileDecoder::Result AudioFileDecoder::decode(const QString&, int, const std::atomic_bool*)
{
    return {{}, QStringLiteral("当前平台未启用 FFmpeg 解码")};
}

AudioFileDecoder::StereoResult AudioFileDecoder::decodeStereo(const QString&, int, const std::atomic_bool*)
{
    StereoResult r;
    r.error = QStringLiteral("当前平台未启用 FFmpeg 解码");
    return r;
}

#endif
