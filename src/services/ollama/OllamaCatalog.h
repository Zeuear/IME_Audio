#pragma once
#include <QString>
#include <QStringList>

// @brief 推荐给用户一键下载的 Ollama 模型：未下载时也列进润色 / 翻译模型下拉框，旁边的按钮负责下载。
// 依据：本项目的翻译评测（Index-Translate-9B Q4 综合 91.9，明显高于离线小模型与谷歌翻译；
// Q4_K_M 为首选；Q3_K_M 体积小约 1 GB，给显存/内存吃紧的用户提速用，质量略降）。
// 润色选通用小模型，翻译选翻译专用模型。
enum class OllamaPurpose { Polish, Translate };

struct OllamaCatalogEntry {
    OllamaPurpose purpose;
    const char* name;      // ollama pull 用的完整名称
    const char* size;      // 下载体积；不确定时留空
};

inline constexpr OllamaCatalogEntry kOllamaCatalog[] = {
    { OllamaPurpose::Translate, "hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M", "5.8 GB" },
    { OllamaPurpose::Translate, "hf.co/IndexTeam/Index-Translate-9B-GGUF:Q3_K_M", "4.7 GB" },
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
