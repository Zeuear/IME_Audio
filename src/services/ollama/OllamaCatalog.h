#pragma once
#include <QString>
#include <QStringList>

// @brief 推荐给用户一键下载的 Ollama 模型：未下载时也列进润色 / 翻译模型下拉框，旁边的按钮负责下载。
// 依据本项目翻译评测：Index-Translate-9B Q4_K_M 首选，Q3_K_M 给显存吃紧的用户，2B 速度约 3 倍、质量略降给低配机器；润色选通用小模型。
enum class OllamaPurpose { Polish, Translate };

struct OllamaCatalogEntry {
    OllamaPurpose purpose;
    const char* name;
    const char* size;
};

inline constexpr OllamaCatalogEntry kOllamaCatalog[] = {
    { OllamaPurpose::Translate, "hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M", "5.8 GB" },
    { OllamaPurpose::Translate, "hf.co/IndexTeam/Index-Translate-9B-GGUF:Q3_K_M", "4.7 GB" },
    { OllamaPurpose::Translate, "hf.co/IndexTeam/Index-Translate-2B-GGUF:Q4_K_M", "1.3 GB" },
    { OllamaPurpose::Polish, "gemma4:e4b-it-q4_K_M", "" },
};

inline QStringList ollamaCatalogNames(OllamaPurpose purpose)
{
    QStringList names;
    for (const OllamaCatalogEntry& e : kOllamaCatalog) {
        if (e.purpose == purpose) names << QString::fromUtf8(e.name);
    }
    return names;
}

inline QString ollamaCatalogSize(const QString& name)
{
    for (const OllamaCatalogEntry& e : kOllamaCatalog) {
        if (name.compare(QString::fromUtf8(e.name), Qt::CaseInsensitive) == 0) return QString::fromUtf8(e.size);
    }
    return {};
}
