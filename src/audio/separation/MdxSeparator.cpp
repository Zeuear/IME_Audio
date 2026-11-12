#include "MdxSeparator.h"

#include <QFile>
#include <algorithm>
#include <cmath>
#include <complex>
#include <string>

#include "MdxDsp.h"
#include "OrtRuntime.h"
#include "onnxruntime_c_api.h"
#include "utils/Logger.h"

namespace MdxSeparator {

namespace {

// UVR 对 UVR-MDX-NET-Voc_FT 的配置（model_data.json，按模型 hash 查得）：
// n_fft=7680、dim_f=3072、dim_t=2^8、compensate=1.021。hop 是 UVR 全部 MDX 模型的固定值。
constexpr int kNFft = 7680;
constexpr int kHop = 1024;
constexpr float kCompensate = 1.021f;   // 补偿模型对幅度的系统性偏低
constexpr int kChannels = 4;            // 左右声道各一对实部/虚部

// 把 OrtStatus 转成错误文本并释放；成功返回空串
QString takeError(const OrtApi* api, OrtStatus* status)
{
    if (!status) return {};
    const QString text = QString::fromUtf8(api->GetErrorMessage(status));
    api->ReleaseStatus(status);
    return text;
}

struct Session {
    const OrtApi* api = nullptr;
    OrtEnv* env = nullptr;
    OrtSessionOptions* options = nullptr;
    OrtSession* session = nullptr;
    OrtMemoryInfo* memory = nullptr;
    std::string inputName;
    std::string outputName;
    int dimF = 0;
    int dimT = 0;

    ~Session()
    {
        if (!api) return;
        if (memory) api->ReleaseMemoryInfo(memory);
        if (session) api->ReleaseSession(session);
        if (options) api->ReleaseSessionOptions(options);
        if (env) api->ReleaseEnv(env);
    }
};

// 创建会话并从输入形状读出 dim_f / dim_t，不写死，换同族模型时不用改代码
QString openSession(Session& s, const QString& modelPath, int threads)
{
    s.api = OrtRuntime::api();
    if (!s.api) return QStringLiteral("找不到 ONNX Runtime，无法分离人声");
    const OrtApi* api = s.api;

    QString err;
    if (!(err = takeError(api, api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "mdx", &s.env))).isEmpty()) return err;
    if (!(err = takeError(api, api->CreateSessionOptions(&s.options))).isEmpty()) return err;
    api->SetIntraOpNumThreads(s.options, threads);
    api->SetSessionGraphOptimizationLevel(s.options, ORT_ENABLE_ALL);

#ifdef Q_OS_WIN
    const std::wstring path = modelPath.toStdWString();
#else
    const std::string path = modelPath.toUtf8().toStdString();
#endif
    if (!(err = takeError(api, api->CreateSession(s.env, path.c_str(), s.options, &s.session))).isEmpty())
        return QStringLiteral("人声分离模型加载失败：") + err;
    if (!(err = takeError(api, api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &s.memory))).isEmpty())
        return err;

    OrtAllocator* allocator = nullptr;
    if (!(err = takeError(api, api->GetAllocatorWithDefaultOptions(&allocator))).isEmpty()) return err;
    char* inName = nullptr;
    char* outName = nullptr;
    if (!(err = takeError(api, api->SessionGetInputName(s.session, 0, allocator, &inName))).isEmpty()) return err;
    s.inputName = inName;
    api->AllocatorFree(allocator, inName);
    if (!(err = takeError(api, api->SessionGetOutputName(s.session, 0, allocator, &outName))).isEmpty()) return err;
    s.outputName = outName;
    api->AllocatorFree(allocator, outName);

    OrtTypeInfo* typeInfo = nullptr;
    if (!(err = takeError(api, api->SessionGetInputTypeInfo(s.session, 0, &typeInfo))).isEmpty()) return err;
    const OrtTensorTypeAndShapeInfo* shape = nullptr;
    api->CastTypeInfoToTensorInfo(typeInfo, &shape);
    int64_t dims[4] = {};
    size_t rank = 0;
    if (shape) {
        api->GetDimensionsCount(shape, &rank);
        if (rank == 4) api->GetDimensions(shape, dims, 4);
    }
    api->ReleaseTypeInfo(typeInfo);

    if (rank != 4 || dims[1] != kChannels || dims[2] <= 0 || dims[3] <= 1
        || dims[2] > kNFft / 2 + 1)
        return QStringLiteral("人声分离模型的输入格式不是 MDX-Net，无法使用");
    s.dimF = int(dims[2]);
    s.dimT = int(dims[3]);
    return {};
}

}

Result separateVocals(const QString& modelPath, const Stereo& mix, int threads,
                      const std::atomic_bool* cancel, const Progress& progress)
{
    Result result;
    const int n = int(mix.left.size());
    if (n <= 0 || mix.right.size() != mix.left.size()) {
        result.error = QStringLiteral("没有可分离的音频");
        return result;
    }

    Session s;
    result.error = openSession(s, modelPath, std::max(1, threads));
    if (!result.error.isEmpty()) return result;
    const OrtApi* api = s.api;

    const int chunk = kHop * (s.dimT - 1);     // 一块输入对应的样本数
    const int trim = kNFft / 2;                // 块两端的 STFT 边缘效应，丢掉
    const int gen = chunk - 2 * trim;          // 每块真正采用的样本数
    const int chunks = (n + gen - 1) / gen;
    const int bins = kNFft / 2 + 1;
    const size_t plane = size_t(s.dimF) * s.dimT;

    MdxDsp::Stft stft(kNFft, kHop);
    std::vector<float> input(plane * kChannels);
    std::vector<float> segment(chunk);
    std::vector<std::complex<float>> spec;
    std::vector<std::complex<float>> rebuilt(size_t(s.dimT) * bins);
    std::vector<float> wave(chunk);

    result.vocals.left.resize(size_t(chunks) * gen);
    result.vocals.right.resize(size_t(chunks) * gen);

    const std::vector<float>* channels[2] = {&mix.left, &mix.right};
    std::vector<float>* outChannels[2] = {&result.vocals.left, &result.vocals.right};
    const char* inputNames[] = {s.inputName.c_str()};
    const char* outputNames[] = {s.outputName.c_str()};
    const int64_t shape[4] = {1, kChannels, s.dimF, s.dimT};

    if (progress) progress(0, chunks);
    for (int c = 0; c < chunks; ++c) {
        if (cancel && cancel->load()) {
            result.error = QStringLiteral("已取消");
            return result;
        }

        const int start = c * gen - trim;
        for (int ch = 0; ch < 2; ++ch) {
            const std::vector<float>& src = *channels[ch];
            for (int i = 0; i < chunk; ++i) {
                const int p = start + i;
                segment[i] = (p >= 0 && p < n) ? src[p] : 0.0f;
            }
            stft.forward(segment.data(), chunk, spec);   // [frame][bin]

            // 张量布局 [通道][频点][帧]，通道顺序 L实 L虚 R实 R虚
            float* re = input.data() + plane * (ch * 2);
            float* im = input.data() + plane * (ch * 2 + 1);
            for (int t = 0; t < s.dimT; ++t) {
                const std::complex<float>* row = spec.data() + size_t(t) * bins;
                for (int f = 0; f < s.dimF; ++f) {
                    re[size_t(f) * s.dimT + t] = row[f].real();
                    im[size_t(f) * s.dimT + t] = row[f].imag();
                }
            }
        }

        OrtValue* inTensor = nullptr;
        OrtValue* outTensor = nullptr;
        QString err = takeError(api, api->CreateTensorWithDataAsOrtValue(
            s.memory, input.data(), input.size() * sizeof(float), shape, 4,
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inTensor));
        if (err.isEmpty())
            err = takeError(api, api->Run(s.session, nullptr, inputNames, &inTensor, 1,
                                          outputNames, 1, &outTensor));
        float* out = nullptr;
        if (err.isEmpty())
            err = takeError(api, api->GetTensorMutableData(outTensor, reinterpret_cast<void**>(&out)));
        if (!err.isEmpty()) {
            if (inTensor) api->ReleaseValue(inTensor);
            if (outTensor) api->ReleaseValue(outTensor);
            LOG_WARN(QString("MdxSeparator: inference failed: %1").arg(err));
            result.error = QStringLiteral("人声分离失败：") + err;
            return result;
        }

        for (int ch = 0; ch < 2; ++ch) {
            const float* re = out + plane * (ch * 2);
            const float* im = out + plane * (ch * 2 + 1);
            std::fill(rebuilt.begin(), rebuilt.end(), std::complex<float>());   // 模型不覆盖的高频置零
            for (int t = 0; t < s.dimT; ++t) {
                std::complex<float>* row = rebuilt.data() + size_t(t) * bins;
                for (int f = 0; f < s.dimF; ++f)
                    row[f] = {re[size_t(f) * s.dimT + t], im[size_t(f) * s.dimT + t]};
            }
            stft.inverse(rebuilt.data(), s.dimT, chunk, wave.data());
            float* dst = outChannels[ch]->data() + size_t(c) * gen;
            for (int i = 0; i < gen; ++i) dst[i] = wave[trim + i] * kCompensate;
        }
        api->ReleaseValue(outTensor);
        api->ReleaseValue(inTensor);

        if (progress) progress(c + 1, chunks);
    }

    result.vocals.left.resize(n);
    result.vocals.right.resize(n);
    return result;
}

bool writeStereoWav(const QString& path, const Stereo& audio, int sampleRate)
{
    const quint32 frames = quint32(std::min(audio.left.size(), audio.right.size()));
    const quint32 dataBytes = frames * 4;

    QByteArray header;
    auto put32 = [&](quint32 v) { for (int i = 0; i < 4; ++i) header.append(char((v >> (8 * i)) & 0xFF)); };
    auto put16 = [&](quint16 v) { for (int i = 0; i < 2; ++i) header.append(char((v >> (8 * i)) & 0xFF)); };
    header.append("RIFF"); put32(36 + dataBytes); header.append("WAVEfmt ");
    put32(16); put16(1); put16(2); put32(sampleRate); put32(sampleRate * 4); put16(4); put16(16);
    header.append("data"); put32(dataBytes);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    if (file.write(header) != header.size()) return false;

    // 分批转换，避免为整段音频再占一份 16-bit 副本
    constexpr quint32 kBatch = 1 << 16;
    QByteArray buffer;
    for (quint32 pos = 0; pos < frames; pos += kBatch) {
        const quint32 count = std::min(kBatch, frames - pos);
        buffer.resize(qsizetype(count) * 4);
        auto* out = reinterpret_cast<qint16*>(buffer.data());
        for (quint32 i = 0; i < count; ++i) {
            out[2 * i] = qint16(std::lround(std::clamp(audio.left[pos + i], -1.0f, 1.0f) * 32767.0f));
            out[2 * i + 1] = qint16(std::lround(std::clamp(audio.right[pos + i], -1.0f, 1.0f) * 32767.0f));
        }
        if (file.write(buffer) != buffer.size()) return false;
    }
    return file.flush();
}

}
