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

}

bool AudioFileDecoder::isAvailable()
{
    return true;
}

AudioFileDecoder::Result AudioFileDecoder::decode(const QString& path, int targetSampleRate,
                                                  const std::atomic_bool* cancel)
{
    // FFmpeg 在 Windows 上按 UTF-8 解释路径，中文文件名可直接打开
    AVFormatContext* rawFmt = nullptr;
    int ret = avformat_open_input(&rawFmt, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) return failed(QStringLiteral("无法打开文件"), ret);
    std::unique_ptr<AVFormatContext, FormatCloser> fmt(rawFmt);

    if ((ret = avformat_find_stream_info(fmt.get(), nullptr)) < 0)
        return failed(QStringLiteral("无法识别文件格式"), ret);

    const AVCodec* codec = nullptr;
    const int streamIndex = av_find_best_stream(fmt.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (streamIndex < 0) return failed(QStringLiteral("文件中没有音频"), streamIndex);

    std::unique_ptr<AVCodecContext, CodecFreer> dec(avcodec_alloc_context3(codec));
    avcodec_parameters_to_context(dec.get(), fmt->streams[streamIndex]->codecpar);
    if ((ret = avcodec_open2(dec.get(), codec, nullptr)) < 0)
        return failed(QStringLiteral("不支持该音频编码"), ret);

    // 只读音频流，其它流（m4a 的封面图、视频文件的画面）直接丢弃，省去无用的解复用
    for (unsigned i = 0; i < fmt->nb_streams; ++i)
        if (int(i) != streamIndex) fmt->streams[i]->discard = AVDISCARD_ALL;

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

#else

bool AudioFileDecoder::isAvailable()
{
    return false;
}

AudioFileDecoder::Result AudioFileDecoder::decode(const QString&, int, const std::atomic_bool*)
{
    return {{}, QStringLiteral("当前平台未启用 FFmpeg 解码")};
}

#endif
