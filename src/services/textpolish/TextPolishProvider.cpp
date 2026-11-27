#include "TextPolishProvider.h"
#include "GeminiProvider.h"
#include "OpenAiProvider.h"
#include "OpenWebUiProvider.h"
#include "config/AppConfig.h"

#include <memory>

std::unique_ptr<TextPolishProvider> createTextPolishProvider(const TextPolishService::RequestParams& params) {
    if (params.aiEngine == 1 || params.aiEngine == 2) {
        return std::make_unique<OpenAiProvider>(params.aiEngine, params.apiKey);
    }
    if (params.aiEngine == kOpenWebUiEngine) {
        return std::make_unique<OpenWebUiProvider>(params.apiKey, params.allowSelfSigned);
    }
    return std::make_unique<GeminiProvider>();
}
