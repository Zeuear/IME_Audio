#pragma once
#include <QString>

// @brief 把 Ollama 返回的英文错误翻成用户能照做的中文原因。纯逻辑，便于单测。
// 旧版 Ollama 不认识新模型的结构时，只会报 "unable to load model: <blob 路径>"，
// 用户看不出是 Ollama 版本太旧，所以在这里点明；其余错误原样返回。
inline QString explainOllamaError(const QString& error)
{
    const QString e = error.toLower();
    if (e.contains(QStringLiteral("unable to load model")) || e.contains(QStringLiteral("unknown model architecture")))
        return QStringLiteral("Ollama 无法加载该模型，通常是 Ollama 版本过旧，请升级到最新版后重试");
    if (e.contains(QStringLiteral("requires more system memory")) || e.contains(QStringLiteral("out of memory")))
        return QStringLiteral("内存不足，无法加载该模型，请换用更小的模型（如 Q3 量化）");
    return error;
}
