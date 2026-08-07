#include "SherpaConfig.h"
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QtConcurrent> 
#include "../utils/Logger.h"
#include "../ConfigManager.h"


QString ModelConfigFactory::getSherpaRoot()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    return dir.absoluteFilePath("sherpa");
}

QString ModelConfigFactory::getSherpaModel()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    QString dirPath = dir.absoluteFilePath("sherpa/models");
    if (!QFile::exists(dirPath)) {
        QDir().mkpath(dirPath);
    }
    return dirPath;
}


QUrl ModelConfigFactory::getHfUrl(const QString& repoId, const QString& subfolder, const QString& filename) {
    QString path = repoId + "/resolve/main";
    if (!subfolder.isEmpty() && subfolder != ".") {
        path += "/" + subfolder;
    }
    return QUrl("https://huggingface.co/" + path + "/" + filename);
}

QString ModelConfigFactory::buildLocalPath(const QString& repoId, const QString& subfolder, const QString& filename) {
    QString repoName = repoId.split("/").last();
    QString modelPath = getSherpaModel();
    QString path = subfolder.isEmpty() ? 
        modelPath + "/" + repoName + "/" + filename : 
        modelPath + "/" + repoName + "/" + subfolder + "/" + filename;
    return path;
}

QString ModelConfigFactory::getTokensPath(const QString& repoId) {
    QString path = buildLocalPath(repoId, "", "tokens.txt");
	return path;
}

QString ModelConfigFactory::getModelPath(const QString& repoId, const QString& subfolder, const QString& filename) {
	return buildLocalPath(repoId, subfolder, filename);
}

// Paraformer(单文件架构,只有 model + tokens 两个文件)
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildParaformer(
    const QString& repoId,
    const QString& modelFile,
    const QString& tokensFile,
    int numThreads)
{
    sherpa_onnx::cxx::FeatureConfig featConfig;
    featConfig.sample_rate = 16000;
    featConfig.feature_dim = 80;

    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.feat_config = featConfig;
    config.model_config.tokens = getModelPath(repoId, "", tokensFile).toStdString();
    config.model_config.paraformer.model = getModelPath(repoId, "", modelFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// SenseVoice(多一个 language / use_itn 参数)
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildSenseVoice(
    const QString& repoId,
    const QString& modelFile,
    const QString& tokensFile,
    const QString& language,
    bool useItn,
    int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", tokensFile).toStdString();
    config.model_config.sense_voice.model = getModelPath(repoId, "", modelFile).toStdString();
    config.model_config.sense_voice.language = language.toStdString();
    config.model_config.sense_voice.use_itn = useItn;
    config.model_config.num_threads = numThreads;
    return config;
}

// ransducer builder
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildOfflineTransducer(
    const QString& repoId,
    const QString& encoderFile,
    const QString& decoderFile,
    const QString& joinerFile,
    const QString& tokensSubfolder,  
    const QString& modelSubfolder,  
    int maxActivePaths,
    int numThreads)
{
    sherpa_onnx::cxx::FeatureConfig featConfig;
    featConfig.sample_rate = 16000;
    featConfig.feature_dim = 80;

    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.feat_config = featConfig;
    config.max_active_paths = maxActivePaths;
    config.model_config.tokens = getModelPath(repoId, tokensSubfolder, "tokens.txt").toStdString();
    config.model_config.transducer.encoder = getModelPath(repoId, modelSubfolder, encoderFile).toStdString();
    config.model_config.transducer.decoder = getModelPath(repoId, modelSubfolder, decoderFile).toStdString();
    config.model_config.transducer.joiner = getModelPath(repoId, modelSubfolder, joinerFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}


sherpa_onnx::cxx::OnlineRecognizerConfig ModelConfigFactory::buildOnlineTransducer(
    const QString& repoId,
    const QString& encoderFile,
    const QString& decoderFile,
    const QString& joinerFile,
    int maxActivePaths,
    int numThreads)
{
    sherpa_onnx::cxx::FeatureConfig featConfig;
    featConfig.sample_rate = 16000;
    featConfig.feature_dim = 80;

    sherpa_onnx::cxx::OnlineRecognizerConfig config;
    config.feat_config = featConfig;
    config.max_active_paths = maxActivePaths;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.transducer.encoder = getModelPath(repoId, "", encoderFile).toStdString();
    config.model_config.transducer.decoder = getModelPath(repoId, "", decoderFile).toStdString();
    config.model_config.transducer.joiner = getModelPath(repoId, "", joinerFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}


// 对应 _get_russian_pre_trained_model_ctc
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildNemoCtc(
    const QString& repoId, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getTokensPath(repoId).toStdString();
    config.model_config.nemo_ctc.model = getModelPath(repoId, "", "model.int8.onnx").toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}


// Whisper
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildWhisper(
    const QString& repoId, const QString& name, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", name + "-tokens.txt").toStdString();
    config.model_config.whisper.encoder = getModelPath(repoId, "", name + "-encoder.int8.onnx").toStdString();
    config.model_config.whisper.decoder = getModelPath(repoId, "", name + "-decoder.int8.onnx").toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// Moonshine
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildMoonshine(const QString& repoId, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.moonshine.preprocessor = getModelPath(repoId, "", "preprocess.onnx").toStdString();
    config.model_config.moonshine.encoder = getModelPath(repoId, "", "encode.int8.onnx").toStdString();
    config.model_config.moonshine.uncached_decoder = getModelPath(repoId, "", "uncached_decode.int8.onnx").toStdString();
    config.model_config.moonshine.cached_decoder = getModelPath(repoId, "", "cached_decode.int8.onnx").toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// FireRedASR
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildFireRedAsr(
    const QString& repoId,
    const FireRedAsrFiles& files,
    int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", files.tokensFile).toStdString();
    config.model_config.fire_red_asr.encoder = getModelPath(repoId, "", files.encoderFile).toStdString();
    config.model_config.fire_red_asr.decoder = getModelPath(repoId, "", files.decoderFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// Dolphin CTC
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildDolphinCtc(const QString& repoId, bool useInt8, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.dolphin.model = getModelPath(repoId, "", useInt8 ? "model.int8.onnx" : "model.onnx").toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}


// ZipformerCtc(离线)
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildZipformerCtcOffline(
    const QString& repoId, const QString& modelFile, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.zipformer_ctc.model = getModelPath(repoId, "", modelFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// ZipformerCtc(流式)
sherpa_onnx::cxx::OnlineRecognizerConfig ModelConfigFactory::buildZipformerCtcStreaming(
    const QString& repoId, const QString& modelFile, int numThreads)
{
    std::string tokenFilePath = getModelPath(repoId, "", "tokens.txt").toStdString();
    std::string modelFilePath = getModelPath(repoId, "", modelFile).toStdString();

    sherpa_onnx::cxx::FeatureConfig featConfig;
    featConfig.sample_rate = 16000;
    featConfig.feature_dim = 80;

    sherpa_onnx::cxx::OnlineRecognizerConfig config;
    config.feat_config = featConfig;
    config.model_config.tokens = tokenFilePath;
    config.model_config.zipformer2_ctc.model = modelFilePath;
    config.model_config.num_threads = numThreads;
    return config;
}

// NemoTransducer 
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildNemoTransducer(
    const QString& repoId,
    const QString& encoderFile, const QString& decoderFile, const QString& joinerFile,
    const QString& tokensSubfolder, const QString& modelSubfolder,
    int numThreads)
{
    sherpa_onnx::cxx::FeatureConfig featConfig;
    featConfig.sample_rate = 16000;
    featConfig.feature_dim = 80;

    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.feat_config = featConfig;
    config.max_active_paths = 4;
    config.model_config.tokens = getModelPath(repoId, tokensSubfolder, "tokens.txt").toStdString();
    config.model_config.transducer.encoder = getModelPath(repoId, modelSubfolder, "encoder.int8.onnx").toStdString();
    config.model_config.transducer.decoder = getModelPath(repoId, modelSubfolder, "decoder.int8.onnx").toStdString();
    config.model_config.transducer.joiner = getModelPath(repoId, modelSubfolder, "joiner.int8.onnx").toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// ParaformerStreaming 流式 paraformer 只有 encoder+decoder,没有 joiner
sherpa_onnx::cxx::OnlineRecognizerConfig ModelConfigFactory::buildParaformerStreaming(
    const QString& repoId,
    const QString& encoderFile, const QString& decoderFile,
    int numThreads)
{
    sherpa_onnx::cxx::OnlineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.paraformer.encoder = getModelPath(repoId, "", encoderFile).toStdString();
    config.model_config.paraformer.decoder = getModelPath(repoId, "", decoderFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// TeleSpeechCtc(离线,方言)
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildTeleSpeechCtc(
    const QString& repoId, const QString& modelFile, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.telespeech_ctc = getModelPath(repoId, "", modelFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// WenetCtc(离线)
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildWenetCtcOffline(
    const QString& repoId, const QString& modelFile, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.wenet_ctc.model = getModelPath(repoId, "", modelFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// Omnilingual ASR(1600+ 语言,离线 CTC)
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildOmnilingualAsr(
    const QString& repoId, const QString& modelFile, int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.omnilingual.model = getModelPath(repoId, "", modelFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}

// T-one CTC(俄语流式)注意字段是 tone_ctc
sherpa_onnx::cxx::OnlineRecognizerConfig ModelConfigFactory::buildTOneCtcStreaming(
    const QString& repoId, const QString& modelFile,int numThreads)
{
    sherpa_onnx::cxx::OnlineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", "tokens.txt").toStdString();
    config.model_config.t_one_ctc.model = getModelPath(repoId, "", modelFile).toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}


// FunasrNano(LLM 混合架构)
sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildFunasrNano(
    const QString& repoId,
    const FunasrFiles& files,
    int numThreads)
{
    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.tokens = getModelPath(repoId, "", files.tokensFile).toStdString();
	config.model_config.funasr_nano.hotwords = files.hotwords.toStdString();
    config.model_config.funasr_nano.embedding = getModelPath(repoId, "", files.embeddingFile).toStdString();
    config.model_config.funasr_nano.encoder_adaptor = getModelPath(repoId, "", files.encoderAdaptorFile).toStdString();
    config.model_config.funasr_nano.llm = getModelPath(repoId, "", files.llmFile).toStdString();

    // Qwen3-0.6B 子目录下的分词器文件
    QString tokenizerJsonPath = getModelPath(repoId, files.tokenizerSubfolder, files.tokenizerJsonFile);
    getModelPath(repoId, files.tokenizerSubfolder, files.mergesFile);
    getModelPath(repoId, files.tokenizerSubfolder, files.vocabFile);

    QFileInfo tokenizerFileInfo(tokenizerJsonPath);
    QString tokenizerDir = tokenizerFileInfo.absolutePath();

    config.model_config.funasr_nano.tokenizer = tokenizerDir.toStdString();
    config.model_config.num_threads = numThreads;
    return config;
}


sherpa_onnx::cxx::OfflineRecognizerConfig ModelConfigFactory::buildQwen3Asr(
    const QString& repoId,
    const Qwen3AsrFiles& files,
    int numThreads)
{
    QString folderName = repoId.split("/")[1];
    QString modelDir = getSherpaModel() + "/" + folderName;

    sherpa_onnx::cxx::OfflineRecognizerConfig config;
    config.model_config.qwen3_asr.tokenizer = QDir(modelDir).filePath(files.tokenizer).toStdString();
    config.model_config.qwen3_asr.encoder = QDir(modelDir).filePath(files.encoderFile).toStdString();
    config.model_config.qwen3_asr.decoder = QDir(modelDir).filePath(files.decoderFile).toStdString();
    config.model_config.qwen3_asr.conv_frontend = QDir(modelDir).filePath(files.convFile).toStdString();
    config.model_config.qwen3_asr.hotwords = files.hotwords.toStdString();

    config.model_config.num_threads = numThreads;
    return config;
}
const std::vector<std::pair<QString, ModelDescriptor>>& ChineseModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-paraformer-zh-2024-03-09",
            {ModelArch::Paraformer, SingleFileModelFiles{.modelFile = "model.int8.onnx"}}},
        {"csukuangfj/sherpa-onnx-zipformer-ctc-zh-int8-2025-07-03",
            {ModelArch::ZipformerCtcOffline, SingleFileModelFiles{.modelFile = "model.int8.onnx"}}},
        {"zrjin/icefall-asr-zipformer-multi-zh-en-2023-11-22", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "exp",
                .tokensSubfolder = "data/lang_bbpe_2000",
                .encoderFile = "encoder-epoch-34-avg-19.int8.onnx",
                .decoderFile = "decoder-epoch-34-avg-19.onnx",
                .joinerFile = "joiner-epoch-34-avg-19.int8.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-fire-red-asr-large-zh_en-2025-02-16",
            {ModelArch::FireRedAsr}},
        {"csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2025-09-09",
            {ModelArch::SenseVoice}},
        {"k2-fsa/sherpa-onnx-qwen3-asr-0.6B-int8-2026-03-25",
            {ModelArch::Qwen3Asr}},
        {"csukuangfj/sherpa-onnx-funasr-nano-int8-2025-12-30",
            {ModelArch::FunasrNano}},
    };
    return table;
}


const std::vector<std::pair<QString, ModelDescriptor>>& EnglishModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-tiny.en",
            {ModelArch::Whisper, WhisperFiles{.name = "tiny.en"}, "Whisper_tiny (OpenAI)"}},
        {"csukuangfj/sherpa-onnx-whisper-base.en",
            {ModelArch::Whisper, WhisperFiles{.name = "base.en"}, "Whisper_base (OpenAI)"}},
        {"csukuangfj/sherpa-onnx-whisper-small.en",
            {ModelArch::Whisper, WhisperFiles{.name = "small.en"}, "Whisper_small (OpenAI)"}},
        {"csukuangfj/sherpa-onnx-moonshine-base-en-int8",
            {ModelArch::Moonshine}},
        {"csukuangfj/sherpa-onnx-nemo-parakeet_tdt_ctc_110m-en-36000",
            {ModelArch::NemoCtc}},
        {"csukuangfj/sherpa-onnx-paraformer-en-2024-03-09",
            {ModelArch::Paraformer, SingleFileModelFiles{.modelFile = "model.int8.onnx"}}},
        {"csukuangfj/sherpa-onnx-fire-red-asr-large-zh_en-2025-02-16",
            {ModelArch::FireRedAsr}},
        {"zrjin/icefall-asr-zipformer-multi-zh-en-2023-11-22", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "exp",
                .tokensSubfolder = "data/lang_bbpe_2000",
                .encoderFile = "encoder-epoch-34-avg-19.int8.onnx",
                .decoderFile = "decoder-epoch-34-avg-19.onnx",
                .joinerFile = "joiner-epoch-34-avg-19.int8.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2025-09-09",
            {ModelArch::SenseVoice}},
    };
    return table;
}


const std::vector<std::pair<QString, ModelDescriptor>>& FunasrNano31LangModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-sense-voice-funasr-nano-int8-2025-12-17",
            {ModelArch::SenseVoice}},
        {"csukuangfj/sherpa-onnx-funasr-nano-int8-2025-12-30",
            {ModelArch::FunasrNano}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& Qwen3AsrModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"k2-fsa/sherpa-onnx-qwen3-asr-0.6B-int8-2026-03-25",
            {ModelArch::Qwen3Asr}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& MoreThan1600LangModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-omnilingual-asr-1600-languages-300M-ctc-int8-2025-11-12",
            {ModelArch::OmnilingualAsr, "Omnilingual (多语言) (small)"}},
        {"csukuangfj/sherpa-onnx-omnilingual-asr-1600-languages-1B-ctc-int8-2025-11-12",
            {ModelArch::OmnilingualAsr, "Omnilingual (多语言) (base)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& MultiLingualModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-dolphin-base-ctc-multi-lang-int8-2025-04-02",
            {ModelArch::DolphinCtc, "Dolphin CTC (base)"}},
        {"csukuangfj/sherpa-onnx-dolphin-small-ctc-multi-lang-int8-2025-04-02",
            {ModelArch::DolphinCtc, "Dolphin CTC (small)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& ChineseEnglishMixedModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-fire-red-asr-large-zh_en-2025-02-16",
            {ModelArch::FireRedAsr}},
        {"zrjin/icefall-asr-zipformer-multi-zh-en-2023-11-22", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "exp",
                .tokensSubfolder = "data/lang_bbpe_2000",
                .encoderFile = "encoder-epoch-34-avg-19.int8.onnx",
                .decoderFile = "decoder-epoch-34-avg-19.onnx",
                .joinerFile = "joiner-epoch-34-avg-19.int8.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-paraformer-zh-2023-09-14",
            {ModelArch::Paraformer, SingleFileModelFiles{.modelFile = "model.int8.onnx"}}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& RussianModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-nemo-ctc-punct-giga-am-v3-russian-2025-12-16",
            {ModelArch::NemoCtc}},
            {"csukuangfj/sherpa-onnx-nemo-ctc-giga-am-v3-russian-2025-12-16",
                {ModelArch::NemoCtc, "Nemo CTC v3"}},
            {"csukuangfj/sherpa-onnx-nemo-ctc-giga-am-v2-russian-2025-04-19",
                {ModelArch::NemoCtc, "Nemo CTC v2"}},
            {"csukuangfj/sherpa-onnx-nemo-ctc-giga-am-russian-2024-10-24",
                {ModelArch::NemoCtc, "Nemo CTC v1"}},
            {"csukuangfj/sherpa-onnx-nemo-transducer-giga-am-v3-russian-2025-12-16",
                {ModelArch::NemoTransducer, "Nemo Transducer v3"}},
            {"csukuangfj/sherpa-onnx-nemo-transducer-giga-am-v2-russian-2025-04-19",
                {ModelArch::NemoTransducer, "Nemo Transducer v2"}},
            {"csukuangfj/sherpa-onnx-nemo-transducer-giga-am-russian-2024-10-24",
                {ModelArch::NemoTransducer, "Nemo Transducer v1"}},
            {"csukuangfj/sherpa-onnx-nemo-transducer-punct-giga-am-v3-russian-2025-12-16",
                {ModelArch::NemoTransducer, "Nemo Transducer (带标点)"}},
            {"csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8",
                {ModelArch::NemoTransducer, "Nemo Transducer (多语言)"}},
            {"csukuangfj/sherpa-onnx-whisper-base",
                {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
            {"alphacep/vosk-model-ru",
                {ModelArch::NemoTransducer, "Nemo Transducer (base)"}},
            {"alphacep/vosk-model-small-ru",
                {ModelArch::NemoTransducer, "Nemo Transducer (small)"}},
            {"csukuangfj/sherpa-onnx-streaming-zipformer-small-ru-vosk-int8-2025-08-16",
                {ModelArch::TransducerOnline}},
            {"csukuangfj/sherpa-onnx-streaming-t-one-russian-2025-09-08",
                {ModelArch::TOneCtcStreaming}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& ChineseCantoneseEnglishModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-paraformer-trilingual-zh-cantonese-en",
            {ModelArch::Paraformer}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& ChineseCantoneseEnglishJapaneseKoreanModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2025-09-09",
            {ModelArch::SenseVoice}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& CantoneseModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-wenetspeech-yue-u2pp-conformer-ctc-zh-en-cantonese-int8-2025-09-10",
            {ModelArch::WenetCtcOffline, SingleFileModelFiles{.modelFile = "model.int8.onnx"}}},
       {"zrjin/icefall-asr-mdcc-zipformer-2024-03-11", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "exp",
                .tokensSubfolder = "data/lang_char",
                .encoderFile = "encoder-epoch-12-avg-8.onnx",
                .decoderFile = "decoder-epoch-12-avg-8.onnx",
                .joinerFile = "joiner-epoch-12-avg-8.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2025-09-09",
            {ModelArch::SenseVoice, "SenseVoice (中英日韩粤)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& JapaneseModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"reazon-research/reazonspeech-k2-v2", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "",
                .tokensSubfolder = "",
                .encoderFile = "encoder-epoch-99-avg-1.onnx",
                .decoderFile = "decoder-epoch-99-avg-1.onnx",
                .joinerFile = "joiner-epoch-99-avg-1.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2025-09-09",
            {ModelArch::SenseVoice}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& KoreanModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"k2-fsa/sherpa-onnx-zipformer-korean-2024-06-24", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "",
                .tokensSubfolder = "",
                .encoderFile = "encoder-epoch-99-avg-1.onnx",
                .decoderFile = "decoder-epoch-99-avg-1.onnx",
                .joinerFile = "joiner-epoch-99-avg-1.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2025-09-09",
            {ModelArch::SenseVoice}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& ThaiModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"yfyeung/icefall-asr-gigaspeech2-th-zipformer-2024-06-20", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "exp",
                .tokensSubfolder = "data/lang_bpe_2000",
                .encoderFile = "encoder-epoch-12-avg-5.int8.onnx",
                .decoderFile = "decoder-epoch-12-avg-5.onnx",
                .joinerFile = "joiner-epoch-12-avg-5.int8.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& VietnameseModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-zipformer-vi-int8-2025-04-20", {
            ModelArch::TransducerOffline,
            TransducerFiles{
                .modelSubfolder = "", .tokensSubfolder = "",
                .encoderFile = "encoder-epoch-12-avg-8.int8.onnx",
                .decoderFile = "decoder-epoch-12-avg-8.onnx",
                .joinerFile = "joiner-epoch-12-avg-8.int8.onnx"
            }
        }},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}



const std::vector<std::pair<QString, ModelDescriptor>>& TibetanModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& ArabicModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"LukeJacob2023/sherpa-onnx-stt_ar_fastconformer_hybrid_large_pc",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& GermanModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8",
            {ModelArch::NemoTransducer, "Nemo Transducer (多语言)"}},
        {"csukuangfj/sherpa-onnx-nemo-transducer-stt_de_fastconformer_hybrid_large_pc-int8",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-nemo-stt_de_fastconformer_hybrid_large_pc-int8",
            {ModelArch::NemoCtc}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& GeorgianModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"LukeJacob2023/sherpa-onnx-stt_ka_fastconformer_hybrid_large_pc",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& ArmenianModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"LukeJacob2023/sherpa-onnx-fastconformer-hybrid-arm-as",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& TagalogModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"LukeJacob2023/sherpa-onnx-stt_tl_fastconformer_hybrid_large",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& FrenchModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& SpanishModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& PortugueseBrazilianModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8",
            {ModelArch::NemoTransducer, "Nemo Transducer (多语言)"}},
        {"csukuangfj/sherpa-onnx-nemo-stt_pt_fastconformer_hybrid_large_pc-int8",
            {ModelArch::NemoCtc}},
        {"csukuangfj/sherpa-onnx-nemo-transducer-stt_pt_fastconformer_hybrid_large_pc-int8",
            {ModelArch::NemoTransducer}},
        {"csukuangfj/sherpa-onnx-whisper-base",
            {ModelArch::Whisper, WhisperFiles{.name = "base"}, "Whisper_base (OpenAI/多语言)"}},
    };
    return table;
}

const std::vector<std::pair<QString, ModelDescriptor>>& TwentyFiveLanguagesModels() {
    static const std::vector<std::pair<QString, ModelDescriptor>> table = {
        {"csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8",
            {ModelArch::NemoTransducer}},
    };
    return table;
}


const QVector<ModelRegistry::LanguageTableEntry>& LanguageTables() {
    static const QVector<ModelRegistry::LanguageTableEntry> tables = {
        //{"Chinese+English",             &ChineseEnglishMixedModels},
        //{"Chinese+English+Cantonese",   &ChineseCantoneseEnglishModels},
        //{"Chinese+English+Cantonese+Japanese+Korean", &ChineseCantoneseEnglishJapaneseKoreanModels},

        {"Chinese",                     &ChineseModels},
        {"English",                     &EnglishModels},
        {"Georgian",                    &GeorgianModels},
        {"Armenian",                    &ArmenianModels},
        {"Arabic",                      &ArabicModels},
        {"Cantonese",                   &CantoneseModels},
        {"French",                      &FrenchModels},
        {"German",                      &GermanModels},
        {"Japanese",                    &JapaneseModels},
        {"Korean",                      &KoreanModels},
        {"Portuguese",                  &PortugueseBrazilianModels},
        {"Russian",                     &RussianModels},
        {"Spanish",                     &SpanishModels},
        {"Thai",                        &ThaiModels},
        {"Tibetan",                     &TibetanModels},
        {"Vietnamese",                  &VietnameseModels},
        {"Tagalog",                     &TagalogModels},
        {"31 languages",                &FunasrNano31LangModels},
        {"1600+ languages",             &MoreThan1600LangModels},
        {"25 European languages",       &TwentyFiveLanguagesModels},
    };
    return tables;
}

const QMap<QString, ModelDescriptor>& ModelRegistry::Table()
{
    static const QMap<QString, ModelDescriptor> merged = [] {
        QMap<QString, ModelDescriptor> t;
        for (const auto& entry : LanguageTables()) {
            const auto& sub = entry.getter();
            for (const auto& pair : sub) {
                t.insert(pair.first, pair.second);
            }
        }
        return t;
        }();
    return merged;
}

const std::vector<std::pair<QString, QStringList>>& ModelRegistry::LanguageToModels()
{
    static const std::vector<std::pair<QString, QStringList>> table = [] {
        std::vector<std::pair<QString, QStringList>> t;
        t.reserve(LanguageTables().size());

        for (const auto& entry : LanguageTables()) {
            const auto& sub = entry.getter();

            QStringList models;
            models.reserve(static_cast<int>(sub.size()));
            for (const auto& pair : sub) {
                models << pair.second.displayName; 
            }
            t.emplace_back(entry.languageName, std::move(models));
        }
        return t;
        }();
    return table;
}

const ModelDescriptor* ModelRegistry::Find(const QString& repoId)
{
    auto it = Table().find(repoId);
    return it != Table().end() ? &it.value() : nullptr;
}

QStringList ModelRegistry::GetLanguages()
{
    QStringList langs;
    const auto& table = LanguageToModels();
    langs.reserve(static_cast<int>(table.size()));
    for (const auto& pair : table) {
        langs << pair.first; 
    }
    return langs;
}

QStringList ModelRegistry::GetModelsByLanguage(const QString& language)
{
    const auto& table = LanguageToModels();
    auto it = std::find_if(table.begin(), table.end(),
        [&language](const std::pair<QString, QStringList>& pair) {
            return pair.first == language;
        });
    return it != table.end() ? it->second : QStringList{};
}

QStringList ModelRegistry::GetLanguagesByModel(const QString& repoId)
{
    QStringList langs;
    const auto& table = LanguageToModels();
    for (const auto& pair : table) {
        if (pair.second.contains(repoId)) {
            langs << pair.first;
        }
    }
    return langs;
}

static bool ArchSupportsHotwords(ModelArch arch) {
    switch (arch) {
    case ModelArch::TransducerOnline:
    case ModelArch::TransducerOffline:
    case ModelArch::NemoTransducer:
    case ModelArch::ParaformerStreaming: 
        return true;
    default:
        return false; 
    }
}

ModelRegistry::Result ModelRegistry::GetConfig(const QString& repoId, int numThreads, bool useGpu)
{
    Result result;
    const ModelDescriptor* desc = ModelRegistry::Find(repoId);
    if (!desc) {
        LOG_ERROR(tr("Model not registered in ModelRegistry: %1").arg(repoId));
        return result;
    }
    if (desc->arch == ModelArch::Unsupported) {
        LOG_WARN(tr("This model only has a TorchScript(.pt) version, which is not supported by cxx-api: %1").arg(repoId));
        return result;
    }

    RecognizerKind kind = RecognizerKind::None;
	bool isCudaAvailable = useGpu;
    if (isCudaAvailable) {
        LOG_DEBUG("Sherpa initialized successfully with [CUDA] acceleration.");
    }else {
        LOG_DEBUG("CUDA not available. Sherpa fallback to [CPU].");
    }

	QString hotwords = ConfigManager::instance().config().sherpa.hotwords;
	float hotscores = ConfigManager::instance().config().sherpa.hotscores;

    RecognizerConfigVar configVar = std::monostate{};
    switch (desc->arch) {
    case ModelArch::Paraformer: {
        auto files = std::get<SingleFileModelFiles>(desc->files);
        configVar = ModelConfigFactory::buildParaformer(repoId, files.modelFile, files.tokensFile, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::TransducerOffline: {
        auto files = std::get<TransducerFiles>(desc->files);
        configVar = ModelConfigFactory::buildOfflineTransducer(
            repoId,
            files.encoderFile, files.decoderFile, files.joinerFile,
            files.tokensSubfolder, files.modelSubfolder, 4, 2);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::TransducerOnline: {
        auto files = std::get<TransducerFiles>(desc->files);
        configVar = ModelConfigFactory::buildOnlineTransducer(repoId, files.encoderFile, files.decoderFile, files.joinerFile, 4, 2);
        result.kind = RecognizerKind::Online;
        break;
    }
    case ModelArch::NemoCtc: {
        configVar = ModelConfigFactory::buildNemoCtc(repoId, 2);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::SenseVoice: {
        auto files = std::get<SenseVoiceFiles>(desc->files);
        configVar = ModelConfigFactory::buildSenseVoice(repoId, files.modelFile, files.tokenFile, files.language, files.useItn, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::Whisper: {
        configVar = ModelConfigFactory::buildWhisper(repoId, std::get<WhisperFiles>(desc->files).name, 2);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::Moonshine: {
        configVar = ModelConfigFactory::buildMoonshine(repoId, 2);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::FireRedAsr: {
        configVar = ModelConfigFactory::buildFireRedAsr(repoId, std::get<FireRedAsrFiles>(desc->files), 2);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::DolphinCtc: {
        bool useInt8 = repoId.contains("int8");
        configVar = ModelConfigFactory::buildDolphinCtc(repoId, useInt8, 2);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::ZipformerCtcOffline: {
        auto files = std::get<SingleFileModelFiles>(desc->files);
        configVar = ModelConfigFactory::buildZipformerCtcOffline(repoId, files.modelFile, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::ZipformerCtcStreaming: {
        auto files = std::get<SingleFileModelFiles>(desc->files);
        configVar = ModelConfigFactory::buildZipformerCtcStreaming(repoId, files.modelFile, numThreads);
        result.kind = RecognizerKind::Online;
        break;
    }
    case ModelArch::NemoTransducer: {
        auto files = std::get<TransducerFiles>(desc->files);
        configVar = ModelConfigFactory::buildNemoTransducer(
            repoId, files.encoderFile, files.decoderFile, files.joinerFile,
            files.tokensSubfolder, files.modelSubfolder, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::ParaformerStreaming: {
        auto files = std::get<ParaformerStreamingFiles>(desc->files);
        configVar = ModelConfigFactory::buildParaformerStreaming(repoId, files.encoderFile, files.decoderFile, numThreads);
        result.kind = RecognizerKind::Online;
        break;
    }
    case ModelArch::TeleSpeechCtc: {
        auto files = std::get<SingleFileModelFiles>(desc->files);
        configVar = ModelConfigFactory::buildTeleSpeechCtc(repoId, files.modelFile, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::WenetCtcOffline: {
        auto files = std::get<SingleFileModelFiles>(desc->files);
        configVar = ModelConfigFactory::buildWenetCtcOffline(repoId, files.modelFile, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::OmnilingualAsr: {
        auto files = std::get<SingleFileModelFiles>(desc->files);
        configVar = ModelConfigFactory::buildOmnilingualAsr(repoId, files.modelFile, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::TOneCtcStreaming: {
        auto files = std::get<SingleFileModelFiles>(desc->files);
        configVar = ModelConfigFactory::buildTOneCtcStreaming(repoId, files.modelFile, numThreads);
        result.kind = RecognizerKind::Online;
        break;
    }
    case ModelArch::FunasrNano: {
        auto files = std::get<FunasrFiles>(desc->files);
        configVar = ModelConfigFactory::buildFunasrNano(repoId, files, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    case ModelArch::Qwen3Asr: {
        auto files = std::get<Qwen3AsrFiles>(desc->files);

        QString folderName = repoId.split("/")[1];
        QString modelDir = ModelConfigFactory::getSherpaModel() + "/" + folderName;
        if (!QFile::exists(modelDir)) {
            LOG_WARN(tr("Qwen3-ASR model files not found, please install the model first: %1").arg(repoId));
            return result; 
        }

        configVar = ModelConfigFactory::buildQwen3Asr(repoId, files, numThreads);
        result.kind = RecognizerKind::Offline;
        break;
    }
    default:
        LOG_WARN("The builder for this architecture has not yet been implemented.: repo=" + repoId);
        return result;
    }


    result.recognizer = std::visit([&](auto&& config) -> RecognizerPtrVar {
        using T = std::decay_t<decltype(config)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return std::monostate{};
        } else {
            if (!hotwords.isEmpty() && ArchSupportsHotwords(desc->arch)) {
                if constexpr (requires { config.hotwords_buf; }) {
                    config.hotwords_buf = hotwords.toStdString();
                }
                if constexpr (requires { config.hotwords_score; }) {
                    config.hotwords_score = hotscores;
                }
                if constexpr (requires { config.decoding_method; }) {
                    config.decoding_method = "modified_beam_search";
                }
                LOG_DEBUG(tr("Hotwords applied for arch=%1, score=%2").arg(int(desc->arch)).arg(hotscores));
            }
            else if (!hotwords.isEmpty()) {
                LOG_WARN(tr("Hotwords configured but architecture does not support contextual biasing, ignored: %1").arg(repoId));
            }

            if (useGpu) {
                try {
                    config.model_config.provider = "cuda";
                    LOG_INFO("Sherpa使用[CUDA]初始化");
                    LOG_DEBUG("Attempting to initialize Sherpa with [CUDA] acceleration...");
                    if constexpr (std::is_same_v<T, sherpa_onnx::cxx::OfflineRecognizerConfig>) {
                        return std::make_unique<sherpa_onnx::cxx::OfflineRecognizer>(sherpa_onnx::cxx::OfflineRecognizer::Create(config));
                    } else {
                        return std::make_unique<sherpa_onnx::cxx::OnlineRecognizer>(sherpa_onnx::cxx::OnlineRecognizer::Create(config));
                    }
                } 
                catch (const std::exception& e) {
                    LOG_WARN(tr("CUDA initialization failed: %1. Automatically falling back to CPU.").arg(e.what()));
                } 
                catch (...) {
                    LOG_WARN("CUDA initialization failed due to an unknown hardware/driver error. Falling back to CPU.");
                }
            }

            config.model_config.provider = "cpu";
            LOG_INFO("Sherpa使用[CPU]初始化");
            LOG_DEBUG("Sherpa initialized successfully with [CPU].");
            if constexpr (std::is_same_v<T, sherpa_onnx::cxx::OfflineRecognizerConfig>) {
                return std::make_unique<sherpa_onnx::cxx::OfflineRecognizer>(sherpa_onnx::cxx::OfflineRecognizer::Create(config));
            } else {
                return std::make_unique<sherpa_onnx::cxx::OnlineRecognizer>(sherpa_onnx::cxx::OnlineRecognizer::Create(config));
            }
        }
    }, configVar);

    result.repoId = repoId;
    result.isLoaded = true;
    return result;
}

const QString ModelRegistry::FindByDisplayName(const QString& language, const QString& displayName)
{
    for (const auto& langEntry : LanguageTables()) {
        if (langEntry.languageName == language) {
            for (const auto& modelPair : langEntry.getter()) {
                if (modelPair.second.displayName == displayName) {
                    return modelPair.first;
                }
            }
        }
    }
    return nullptr;
}

ModelInstallManifest ModelRegistry::BuildManifest(const QString& repoId)
{
    ModelInstallManifest manifest;
    manifest.repoId = repoId;
    manifest.displayName = repoId.split("/").last();

    const ModelDescriptor* desc = Find(repoId);
    if (!desc) {
        LOG_ERROR(tr("Model not registered in ModelRegistry: %1").arg(repoId));
        return manifest;
    }

    auto addFile = [&](const QString& subfolder, const QString& filename) {
        ModelFileEntry entry;
        entry.subfolder = subfolder;
        entry.filename = filename;
        entry.sourceUrl = ModelConfigFactory::getHfUrl(repoId, subfolder, filename);
        entry.localPath = ModelConfigFactory::buildLocalPath(repoId, subfolder, filename);
        manifest.files.push_back(entry);
        };

    auto safeGet = [&](auto tag) -> decltype(tag) {
        using T = decltype(tag);
        if (auto* p = std::get_if<T>(&desc->files)) {
            return *p;
        }
        LOG_WARN(tr("ModelDescriptor.files type mismatch for %1, using default values").arg(repoId));
        return T{};
    };

    switch (desc->arch) {

    case ModelArch::Paraformer:
    case ModelArch::NemoCtc:
    case ModelArch::NemoTransducer:
    case ModelArch::DolphinCtc:
    case ModelArch::ZipformerCtcOffline:
    case ModelArch::ZipformerCtcStreaming:
    case ModelArch::TeleSpeechCtc:
    case ModelArch::WenetCtcOffline:
    case ModelArch::OmnilingualAsr:
    case ModelArch::TOneCtcStreaming: {
        auto files = safeGet(SingleFileModelFiles{});
        addFile("", files.tokensFile);
        addFile("", files.modelFile);
        break;
    }

    case ModelArch::FireRedAsr: {
        auto files = safeGet(FireRedAsrFiles{});
        addFile("", files.tokensFile);
        addFile("", files.encoderFile);
        addFile("", files.decoderFile);
        break;
    }
    case ModelArch::TransducerOffline:
    case ModelArch::TransducerOnline: {
        auto files = safeGet(TransducerFiles{});
        addFile(files.tokensSubfolder, "tokens.txt");
        addFile(files.modelSubfolder, files.encoderFile);
        addFile(files.modelSubfolder, files.decoderFile);
        addFile(files.modelSubfolder, files.joinerFile);
        break;
    }

    case ModelArch::SenseVoice: {
        auto files = safeGet(SenseVoiceFiles{});
        addFile("", files.tokenFile);
        addFile("", files.modelFile);
        break;
    }

    case ModelArch::ParaformerStreaming: {
        auto files = safeGet(ParaformerStreamingFiles{});
        addFile("", "tokens.txt");
        addFile("", files.encoderFile);
        addFile("", files.decoderFile);
        break;
    }

    case ModelArch::Whisper: {
        auto files = safeGet(WhisperFiles{});
        addFile("", files.name + "-encoder.int8.onnx");
        addFile("", files.name + "-decoder.int8.onnx");
        addFile("", files.name + "-tokens.txt");
        break;
    }

    case ModelArch::Moonshine: {
        addFile("", "tokens.txt");
        addFile("", "encode.int8.onnx");
        addFile("", "uncached_decode.int8.onnx");
        addFile("", "cached_decode.int8.onnx");
        addFile("", "preprocess.onnx");
        break;
    }

    case ModelArch::FunasrNano: {
        auto files = safeGet(FunasrFiles{});
        addFile("", files.embeddingFile);
        addFile("", files.encoderAdaptorFile);
        addFile("", files.llmFile);
        addFile("", files.tokensFile);
        addFile(files.tokenizerSubfolder, files.tokenizerJsonFile);
        addFile(files.tokenizerSubfolder, files.mergesFile);
        addFile(files.tokenizerSubfolder, files.vocabFile);
        break;
    }

    case ModelArch::Qwen3Asr: {
        auto files = safeGet(Qwen3AsrFiles{});
        manifest.archiveUrl = files.archiveUrl;
        manifest.archiveLocalPath = ModelConfigFactory::getSherpaRoot() + "/" + files.archiveFileName;
        QDir().mkpath(ModelConfigFactory::getSherpaRoot());

        manifest.archiveExtractedDirName = files.extractedDirName;
        manifest.archiveTargetDir = ModelConfigFactory::getSherpaModel() + "/" + files.extractedDirName;
        QDir().mkpath(manifest.archiveTargetDir);
        break;
    }

    case ModelArch::Unsupported:
    default:
        LOG_WARN(tr("BuildManifest: unsupported or unmapped ModelArch for %1").arg(repoId));
        break;
    }

    return manifest;
}