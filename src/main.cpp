#include "MainWin.h"
#include "Application.h"


#include <cstdio>
#include "cxx-api.h"
#include "c-api.h"  
#include <iostream>
#include <vector>
#include <fstream>
#include "utils/AudioStreamRecorder.h"
//#include "miniaudio.h"
void test() {
    //"B:/Project/SW/SW_Ime_Audio/app/RelWithDebInfo/sherpa/models/sherpa-onnx-streaming-zipformer-ctc-zh-int8-2025-06-30"
    //"B:/Project/SW/SW_Ime_Audio/app/RelWithDebInfo/sherpa/models/sherpa-onnx-streaming-zipformer-ctc-zh-int8-2025-06-30/tokens.txt"
    const std::string modelDir = "B:/Project/SW/SW_Ime_Audio/app/RelWithDebInfo/sherpa/models/sherpa-onnx-streaming-zipformer-ctc-zh-int8-2025-06-30";
    const std::string tokensPath = modelDir + "/tokens.txt";
    const std::string modelPath = modelDir + "/model.int8.onnx"; 

    {
        std::ifstream tokensFile(tokensPath, std::ios::binary | std::ios::ate);
        std::ifstream modelFile(modelPath, std::ios::binary | std::ios::ate);

        if (!tokensFile.good() || !modelFile.good()) {
            std::cerr << "文件缺失,先解决文件问题,不用往下测了" << std::endl;
            return;
        }
    }

    // 构造 config,注意显式设置 feat_config(这是我们上次怀疑的关键点)
    sherpa_onnx::cxx::FeatureConfig featConfig;
    featConfig.sample_rate = 16000;
    featConfig.feature_dim = 80;

    sherpa_onnx::cxx::OnlineRecognizerConfig config;
    config.feat_config = featConfig;
    config.model_config.tokens = tokensPath;
    config.model_config.zipformer2_ctc.model = modelPath;
    config.model_config.num_threads = 1;
    config.model_config.provider = "cuda";
    qDebug() << "开始创建识别器...";

    try {
        sherpa_onnx::cxx::OnlineRecognizer recognizer =
            sherpa_onnx::cxx::OnlineRecognizer::Create(config);
        qDebug() << "识别器创建成功!" ;
    }       
    catch (const std::exception& e) {
        qDebug() << "创建/解码识别器时抛出异常: " << e.what();
        return;
    }

    return;
}


inline bool writeWavFile(const std::string& filePath,
    const std::vector<int16_t>& pcmData,
    uint32_t sampleRate,
    uint16_t channels) {
    FILE* f = fopen(filePath.c_str(), "wb");
    if (!f) return false;

    const uint16_t bitsPerSample = 16;
    const uint32_t dataSize = static_cast<uint32_t>(pcmData.size() * sizeof(int16_t));
    const uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
    const uint16_t blockAlign = static_cast<uint16_t>(channels * (bitsPerSample / 8));
    const uint32_t riffSize = 36 + dataSize;

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&riffSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtChunkSize = 16;
    uint16_t audioFormat = 1;  // PCM
    fwrite(&fmtChunkSize, 4, 1, f);
    fwrite(&audioFormat, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sampleRate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f);
    fwrite(&bitsPerSample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    if (!pcmData.empty()) {
        fwrite(pcmData.data(), sizeof(int16_t), pcmData.size(), f);
    }

    fclose(f);
    return true;
}

int testAudio() {
    // 1. 枚举设备，给 UI 下拉框用
    auto devices = AudioStreamRecorder::enumerateInputDevices();
    for (const auto& d : devices) {
        std::cout << (d.isDefault ? "[默认] " : "        ")
            << d.name << "  id=" << d.idHex << "\n";
    }

    AudioStreamRecorder recorder;

    // 2. 配置：16kHz / 单声道 / 尝试跳过系统麦克风增强效果
    AudioStreamConfig config;
    config.sampleRate = 16000;
    config.channels = 1;
    config.requestRawMode = true;
    // config.deviceIdHex = devices[1].idHex; // 如果要指定某个设备

    std::vector<int16_t> segmentBuffer;

    // 3. 开始录音，回调里只做轻量的缓冲/VAD，不要直接跑 ASR
    bool ok = recorder.start(config, [&](const int16_t* data, size_t frameCount,
        uint32_t sampleRate, uint32_t channels) {
            // 这里对应你原来 onAudioDataReady 里的逻辑：
            segmentBuffer.insert(segmentBuffer.end(), data, data + frameCount * channels);

            // 简单示例：每次都打印一下音量峰值，实际项目里替换成你的 VAD 逻辑
            int16_t peak = 0;
            for (size_t i = 0; i < frameCount * channels; ++i) {
                peak = std::max(peak, static_cast<int16_t>(std::abs(data[i])));
            }
            // std::cout << "peak=" << peak << "\n";
        });

    if (!ok) {
        std::cerr << "启动录音失败: " << recorder.lastError() << "\n";
        return 1;
    }

    std::cout << "实际生效采样率=" << recorder.actualSampleRate()
        << " 声道=" << recorder.actualChannels() << "\n";

    std::cout << "录音中，按回车停止...\n";
    std::cin.get();

    recorder.stop();

	writeWavFile("test_output.wav", segmentBuffer, recorder.actualSampleRate(), recorder.actualChannels());
    return 0;
}
//#include "miniaudio.h"
//
//void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
//{
//    ma_encoder_write_pcm_frames((ma_encoder*)pDevice->pUserData, pInput, frameCount, NULL);
//
//    (void)pOutput;
//}
int main(int argc, char *argv[])
{

    //ma_result result;
    //ma_encoder_config encoderConfig;
    //ma_encoder encoder;
    //ma_device_config deviceConfig;
    //ma_device device;

    //encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);

    //if (ma_encoder_init_file("test_output2.wav", &encoderConfig, &encoder) != MA_SUCCESS) {
    //    printf("Failed to initialize output file.\n");
    //    return -1;
    //}

    //deviceConfig = ma_device_config_init(ma_device_type_capture);
    //deviceConfig.capture.format = encoder.config.format;
    //deviceConfig.capture.channels = encoder.config.channels;
    //deviceConfig.sampleRate = encoder.config.sampleRate;
    //deviceConfig.dataCallback = data_callback;
    //deviceConfig.pUserData = &encoder;
    //
    //int format = encoder.config.format;
    //int channels = encoder.config.channels;
    //int sampleRate = encoder.config.sampleRate;


    //deviceConfig.capture.format = encoder.config.format;   // ASR 通用格式：16-bit PCM
    //deviceConfig.capture.channels = 1;
    //deviceConfig.sampleRate = 16000;
    //deviceConfig.dataCallback = data_callback;
    //deviceConfig.pUserData = &encoder;

    //result = ma_device_init(NULL, &deviceConfig, &device);
    //if (result != MA_SUCCESS) {
    //    printf("Failed to initialize capture device.\n");
    //    return -2;
    //}

    //result = ma_device_start(&device);
    //if (result != MA_SUCCESS) {
    //    ma_device_uninit(&device);
    //    printf("Failed to start device.\n");
    //    return -3;
    //}

    //printf("Press Enter to stop recording...\n");
    //getchar();

    //ma_device_uninit(&device);
    //ma_encoder_uninit(&encoder);


    ////test();
    ////testAudio();
    //return 0;

    Application a(argc, argv);

    MainWin w;
    w.show();

    return a.exec();
}   
    