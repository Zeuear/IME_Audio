#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QQueue>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <memory>
#include <functional>
#include <variant>
#include "cxx-api.h"
#include "sherpa/GpuVariant.h"



namespace InstallMarker {

    bool isInstalled(const QString& modelDir);
    bool isIncomplete(const QString& modelDir);

    bool markInstalling(const QString& modelDir);
    void markComplete(const QString& modelDir);

}

enum class ModelArch {
    Paraformer,
    ParaformerStreaming,
    TransducerOffline,
    TransducerOnline,
    ZipformerCtcOffline,
    ZipformerCtcStreaming,
    NemoCtc,
    NemoTransducer,
    TeleSpeechCtc,
    SenseVoice,
    DolphinCtc,
    FireRedAsr,
    WenetCtcOffline,
    OmnilingualAsr,
    TOneCtcStreaming,
    Whisper,
    Moonshine,
    FunasrNano,
    Qwen3Asr,
    Canary,
    FireRedAsrCtc,
    CohereTranscribe,
    Unsupported
};

enum class PunctMode {
    Auto,   // 默认：需要则绑定全局默认 punc 模型（仅 zh/en 且模型不自带头标点时）
    Off     // 不绑定神经标点，回退到 TextPostProcessor 启发式
};


struct TransducerFiles {
    QString modelSubfolder;
    QString tokensSubfolder;
    QString encoderFile = "encoder.int8.onnx";
    QString decoderFile = "decoder.int8.onnx";
    QString joinerFile = "joiner.int8.onnx";
};

struct SingleFileModelFiles {
    QString modelFile = "model.onnx";
    QString tokensFile = "tokens.txt";
};

struct FireRedAsrFiles {
    QString encoderFile = "encoder.int8.onnx";
    QString decoderFile = "decoder.int8.onnx";
    QString tokensFile = "tokens.txt";
};

struct SenseVoiceFiles {
    QString modelFile = "model.int8.onnx";
    QString tokenFile = "tokens.txt";
    QString language = "auto";
    bool useItn = true;
};

struct ParaformerStreamingFiles {
    QString encoderFile = "encoder.onnx";
    QString decoderFile = "decoder.onnx";
};

struct WhisperFiles {
    QString name;
};

// v1：preprocessor + encoder + uncached/cached decoder；
// v2（2026 起的单语模型）：encoder + merged decoder（.ort），其余留空
struct MoonshineFiles {
    QString preprocessorFile = "preprocess.onnx";
    QString encoderFile = "encode.int8.onnx";
    QString uncachedDecoderFile = "uncached_decode.int8.onnx";
    QString cachedDecoderFile = "cached_decode.int8.onnx";
    QString mergedDecoderFile;
    QString tokensFile = "tokens.txt";
};

struct FunasrFiles {
    QString embeddingFile = "embedding.int8.onnx";
    QString encoderAdaptorFile = "encoder_adaptor.int8.onnx";
    QString llmFile = "llm.int8.onnx";

    QString tokenizerSubfolder = "Qwen3-0.6B";
    QString mergesFile = "merges.txt";
    QString tokenizerJsonFile = "tokenizer.json";
    QString vocabFile = "vocab.json";
	QString hotwords;
};

struct Qwen3AsrFiles {
    QString archiveUrl;
    QString archiveFileName;
    QString extractedDirName;
    QString encoderFile = "encoder.int8.onnx";
    QString decoderFile = "decoder.int8.onnx";
    QString convFile = "conv_frontend.onnx";
    QString decoderDataFile;                    // 外部权重，空串 = 没有；须与 decoderFile 同目录
    QStringList tokenizerFiles;
    QString tokenizer = "tokenizer";
    QString hotwords;
};

struct CanaryFiles {
    QString encoderFile = "encoder.int8.onnx";
    QString decoderFile = "decoder.int8.onnx";
    QString tokensFile = "tokens.txt";
    QString srcLang = "en";  
    QString tgtLang = "en";   
    bool usePnc = true;      
};

// 语种不在这里：同一仓库出现在多个语言表里，按当前选中的语言在加载时传入
struct CohereTranscribeFiles {
    QString encoderFile = "encoder.int8.onnx";
    QString encoderDataFile = "encoder.int8.onnx.data";   // encoder 的外部权重（约 2.7 GB），须与 .onnx 同目录
    QString decoderFile = "decoder.int8.onnx";
    QString tokensFile = "tokens.txt";
    bool usePunct = true;
    bool useItn = true;
};

using ModelFiles = std::variant<SingleFileModelFiles, TransducerFiles, SenseVoiceFiles,
    ParaformerStreamingFiles, WhisperFiles, MoonshineFiles, FireRedAsrFiles,
    FunasrFiles, Qwen3AsrFiles, CanaryFiles, CohereTranscribeFiles>;




struct ModelDescriptor {
    QString displayName;
    ModelArch arch;
    ModelFiles files;
    QString hotwords;
    float hotscores;
    int maxActivePaths = 4;
    int numThreads = 2;

    QString language;                       // 由语言表注入（zh/en/...），用于判断是否可绑 punc
    PunctMode punctMode = PunctMode::Auto;
    bool hasBuiltinPunctuation = false;    // 模型自带标点（SenseVoice/Canary 等）→ 不绑 punc

    QString gpuRepoId;                     // 非空 = 该 CPU 模型有对应的 GPU 变体（另一行）
    bool gpuVariant = false;               // 该行本身是 GPU 变体（fp16），只有它才会用 CUDA
    int approxSizeMb = 0;

    ModelDescriptor(ModelArch a)
        : arch(a), files(DefaultFilesFor(a)), displayName(modelArchToString(a)) {
    }

    ModelDescriptor(ModelArch a, ModelFiles f)
        : arch(a), files(std::move(f)), displayName(modelArchToString(a)) {
    }

    ModelDescriptor(ModelArch a, QString name)
        : arch(a), files(DefaultFilesFor(a)), displayName(std::move(name)) {
    }

    ModelDescriptor(ModelArch a, ModelFiles f, QString name)
        : arch(a), files(std::move(f)), displayName(std::move(name)) {
    }

    static ModelFiles DefaultFilesFor(ModelArch a)
    {
        switch (a) {
        case ModelArch::TransducerOffline:
        case ModelArch::TransducerOnline:
        case ModelArch::NemoTransducer:
            return TransducerFiles{};

        case ModelArch::Paraformer:
        case ModelArch::NemoCtc:
        case ModelArch::DolphinCtc:
        case ModelArch::ZipformerCtcOffline:
        case ModelArch::ZipformerCtcStreaming:
        case ModelArch::TeleSpeechCtc:
        case ModelArch::WenetCtcOffline:
        case ModelArch::OmnilingualAsr:
        case ModelArch::TOneCtcStreaming:
            return SingleFileModelFiles{};

        case ModelArch::FireRedAsrCtc:
            return SingleFileModelFiles{.modelFile = "model.int8.onnx"};

        case ModelArch::FireRedAsr:
            return FireRedAsrFiles{};

        case ModelArch::SenseVoice:
            return SenseVoiceFiles{};

        case ModelArch::ParaformerStreaming:
            return ParaformerStreamingFiles{};

        case ModelArch::Whisper:
            return WhisperFiles{};

        case ModelArch::Moonshine:
            return MoonshineFiles{};

        case ModelArch::FunasrNano:
            return FunasrFiles{};

        case ModelArch::Canary:               
            return CanaryFiles{};

        case ModelArch::CohereTranscribe:
            return CohereTranscribeFiles{};

        case ModelArch::Qwen3Asr:
            return Qwen3AsrFiles{
                .archiveUrl = "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-qwen3-asr-0.6B-int8-2026-03-25.tar.bz2",
                .archiveFileName = "sherpa-onnx-qwen3-asr-0.6B-int8-2026-03-25.tar.bz2",
                .extractedDirName = "sherpa-onnx-qwen3-asr-0.6B-int8-2026-03-25"
            };

        case ModelArch::Unsupported:
        default:
            qWarning() << "ModelDescriptor::DefaultFilesFor: no mapping for ModelArch"
                << static_cast<int>(a) << ", falling back to SingleFileModelFiles";
            return SingleFileModelFiles{};
        }
    }

        static QString modelArchToString(ModelArch arch) {
            switch (arch) {
            case ModelArch::Paraformer:          return "Paraformer (非流式/默认)";
            case ModelArch::ParaformerStreaming: return "Paraformer (流式)";
            case ModelArch::TransducerOffline:   return "Transducer (非流式)";
            case ModelArch::TransducerOnline:    return "Transducer (流式)";
            case ModelArch::ZipformerCtcOffline: return "Zipformer (非流式/高性能)";
            case ModelArch::ZipformerCtcStreaming: return "Zipformer (流式/高性能)";
            case ModelArch::NemoCtc:             return "Nemo CTC";
            case ModelArch::NemoTransducer:      return "Nemo Transducer";
            case ModelArch::TeleSpeechCtc:       return "TeleSpeech CTC";
            case ModelArch::SenseVoice:          return "SenseVoice (多语言)";
            case ModelArch::DolphinCtc:          return "Dolphin CTC";
            case ModelArch::FireRedAsr:          return "FireRed ASR";
            case ModelArch::WenetCtcOffline:     return "Wenet CTC";
            case ModelArch::OmnilingualAsr:      return "Omnilingual (多语言)";
            case ModelArch::TOneCtcStreaming:    return "T-One Streaming";
            case ModelArch::Whisper:             return "Whisper (OpenAI)";
            case ModelArch::Moonshine:           return "Moonshine";
            case ModelArch::FunasrNano:          return "FunASR Nano (2025)";
            case ModelArch::Qwen3Asr:            return "Qwen3-ASR (大语言声学模型)";
            case ModelArch::Canary:              return "Canary";
            case ModelArch::FireRedAsrCtc:       return "FireRed ASR2 CTC (中英/方言)";
            case ModelArch::CohereTranscribe:    return "Cohere Transcribe (14 语种)";
            default:                             return "Unknown";
            }
    }
};

struct ModelEntry {
    QString displayName;
    QString repoPath;
};


class ModelConfigFactory {
public:
    static QString getSherpaRoot();
    static QString getSherpaModel();

    static QUrl getHfUrl(const QString& repoId, const QString& subfolder, const QString& filename);
    static QString buildLocalPath(const QString& repoId, const QString& subfolder, const QString& filename);

    static QString getTokensPath(const QString& repoId);
    static QString getModelPath(const QString& repoId, const QString& subfolder, const QString& filename);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildOfflineTransducer(
        const QString& repoId,
        const QString& encoderFile,
        const QString& decoderFile,
        const QString& joinerFile,
        const QString& tokensSubfolder,
        const QString& modelSubfolder,
        int maxActivePaths,
        int numThreads);

	static sherpa_onnx::cxx::OnlineRecognizerConfig buildOnlineTransducer(
        const QString& repoId,
        const QString& encoderFile,
        const QString& decoderFile,
        const QString& joinerFile,
        int maxActivePaths,
        int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildParaformer(
        const QString& repoId,
        const QString& modelFile,
        const QString& tokensFile,
        int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildSenseVoice(
        const QString& repoId,
        const QString& modelFile,
        const QString& tokensFile,
        const QString& language,
        bool useItn,
        int numThreads);

	static sherpa_onnx::cxx::OfflineRecognizerConfig buildNemoCtc(const QString& repoId, const QString& modelFile, int numThreads);
	static sherpa_onnx::cxx::OfflineRecognizerConfig buildWhisper(const QString& repoId, const QString& name, const QString& languageCode, int numThreads);
	static sherpa_onnx::cxx::OfflineRecognizerConfig buildMoonshine(const QString& repoId, const MoonshineFiles& files, int numThreads);
    static sherpa_onnx::cxx::OfflineRecognizerConfig buildFireRedAsr(const QString& repoId, const FireRedAsrFiles& files, int numThreads);
    static sherpa_onnx::cxx::OfflineRecognizerConfig buildDolphinCtc(const QString& repoId, bool useInt8, int numThreads);

    static sherpa_onnx::cxx::OnlineRecognizerConfig buildTOneCtcStreaming(const QString& repoId, const QString& modelFile, int numThreads);
    static sherpa_onnx::cxx::OfflineRecognizerConfig buildOmnilingualAsr(const QString& repoId, const QString& modelFile, int numThreads);
    static sherpa_onnx::cxx::OfflineRecognizerConfig buildWenetCtcOffline(const QString& repoId, const QString& modelFile, int numThreads);
    static sherpa_onnx::cxx::OfflineRecognizerConfig buildTeleSpeechCtc(const QString& repoId, const QString& modelFile, int numThreads);
    static sherpa_onnx::cxx::OnlineRecognizerConfig buildZipformerCtcStreaming(const QString& repoId, const QString& modelFile, int numThreads);
    static sherpa_onnx::cxx::OfflineRecognizerConfig buildZipformerCtcOffline(const QString& repoId, const QString& modelFile, int numThreads);
    static sherpa_onnx::cxx::OnlineRecognizerConfig buildParaformerStreaming(const QString& repoId, const QString& encoderFile, const QString& decoderFile, int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildNemoTransducer(
        const QString& repoId, const
        QString& encoderFile,
        const QString& decoderFile,
        const QString& joinerFile,
        const QString& tokensSubfolder, const QString& modelSubfolder, int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildFunasrNano(
        const QString& repoId,
        const FunasrFiles& files,
        int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildQwen3Asr(
        const QString& repoId,
        const Qwen3AsrFiles& files,
        int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildCanary(
        const QString& repoId,
        const CanaryFiles& files,
        int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildFireRedAsrCtc(
        const QString& repoId,
        const QString& modelFile,
        int numThreads);

    static sherpa_onnx::cxx::OfflineRecognizerConfig buildCohereTranscribe(
        const QString& repoId,
        const CohereTranscribeFiles& files,
        const QString& languageCode,
        int numThreads);

    static QString cohereLanguageCode(const QString& languageName);

};


struct ModelFileEntry {
    QString subfolder;  
    QString filename;
    QUrl sourceUrl;       
    QString localPath;  
};


struct ModelInstallManifest {
    QString repoId;
    QString displayName;                 
    std::vector<ModelFileEntry> files;  

    QString archiveUrl;
    QString archiveLocalPath;
    QString archiveExtractedDirName;
    QString archiveTargetDir;
};

enum class RecognizerKind { None, Offline, Online };

using RecognizerConfigVar = std::variant<
    std::monostate,
    sherpa_onnx::cxx::OfflineRecognizerConfig,
    sherpa_onnx::cxx::OnlineRecognizerConfig
>;

using RecognizerPtrVar = std::variant<
    std::monostate,
    std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer>,
    std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer>
>;

class ModelRegistry: public QObject {
	Q_OBJECT
public:

    struct Result {
		QString repoId;
		bool isLoaded = false;
		bool cudaFailed = false;     // GPU 变体的 CUDA 初始化失败（不回退 CPU，isLoaded 为 false）
		QString failReason;
		GpuProvider provider = GpuProvider::Unknown;
		bool requestedCuda = false;
		RecognizerKind kind = RecognizerKind::None;
        RecognizerPtrVar recognizer = std::monostate{};
	};

    struct LanguageTableEntry {
        QString languageName;
        const std::vector<std::pair<QString, ModelDescriptor>>& (*getter)();
    };

    struct VadModel {
        inline static const QString repoId = QStringLiteral("silero-vad");
        inline static const QString archiveUrl =
            "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx";
        inline static const QString localPath = ModelConfigFactory::getSherpaRoot() + "/vad/silero_vad.onnx";

    };

    struct NeuralPunctModel {
        inline static const QString repoId =
            "csukuangfj/sherpa-onnx-punct-ct-transformer-zh-en-vocab272727-2024-04-12";
        inline static const QString archiveUrl =
            "https://github.com/k2-fsa/sherpa-onnx/releases/download/punctuation-models/sherpa-onnx-punct-ct-transformer-zh-en-vocab272727-2024-04-12.tar.bz2";
        inline static const QString localPath = ModelConfigFactory::getSherpaRoot() + "/punct-zh-en.tar.bz2";
        static QString sharedDir();         
        static bool isInstalled();        
    };

    // language 取用户所选语言：同一模型会出现在多个语言分组里，不能用注册表里记录的语言
    static bool shouldUseNeuralPunct(const ModelDescriptor& desc, const QString& language);

    // 模型覆盖多种语言、而识别时无法指定语种（由模型自己判断）。
    static bool detectsLanguageAutomatically(const ModelDescriptor& desc, int coveredLanguageCount);

    static const ModelDescriptor* Find(const QString& repoId);
    static bool SupportsHotwords(const QString& repoId);

    static bool IsGpuVariant(const QString& repoId);
    static QString GpuVariantOf(const QString& cpuRepoId);
    static QString CpuVariantOf(const QString& gpuRepoId);
    static bool ShouldUseCuda(const ModelDescriptor& desc, bool globalUseGpu);
    static QString ResolveForMode(const QString& repoId, bool useGpu);
    static GpuVariant::GpuState GpuStateOf(const QString& cpuRepoId);
    // allowHotwords=false 用于实时字幕：流式 BPE 模型没配词表，套用全局热词可能导致创建失败
    static Result GetConfig(const QString& repoId, int numThreads, bool useGpu, bool allowHotwords = true);

    // 实时字幕的流式伴随模型：不在语言表里，但并入 Table()，Find / BuildManifest / GetConfig 照常可用；无则返回空串。
    static QString StreamingModelFor(const QString& language);
    static bool IsStreamingCompanion(const QString& repoId);

    static QStringList GetLanguages();                              
    static QStringList GetModelsByLanguage(const QString& language); 
    static QStringList GetLanguagesByModel(const QString& repoId);
    static QList<ModelEntry> GetModelEntriesByLanguage(const QString& language);
    static QStringList GetLanguagesByRepo(const QString& repoId);

    static ModelInstallManifest BuildManifest(const QString& repoId);
    static const QString FindByDisplayName(const QString& language, const QString& displayName);

private:
    static const QMap<QString, ModelDescriptor>& Table();
    static const std::vector<std::pair<QString, QStringList>>& LanguageToModels();
};

