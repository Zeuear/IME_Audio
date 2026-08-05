#include "TextPolishProvider.h"
#include "GeminiProvider.h"
#include "OpenAiProvider.h"

#include <memory>

std::unique_ptr<TextPolishProvider> createTextPolishProvider(const TextPolishService::RequestParams& params) {
    if (params.aiEngine == 1 || params.aiEngine == 2) {
        return std::make_unique<OpenAiProvider>();
    }
    return std::make_unique<GeminiProvider>();
}
