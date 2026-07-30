#include "MainWin.h"
#include "Application.h"

#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>
#include "cxx-api.h"

void test() {
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

int main(int argc, char *argv[])
{
    //test();
    Application a(argc, argv);

    MainWin w;
    w.show();

    return a.exec();
}   
    