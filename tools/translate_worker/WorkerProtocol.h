#pragma once

#include <cstdint>
#include <string>
#include <vector>

// @brief 主程序与翻译 worker 之间的行协议。只依赖标准库：worker 是静态 CRT 的独立进程，不能引入 Qt。
//
// 一行一条消息，字段用 TAB 分隔，文本里的 \ 换行 回车 TAB 转义，保证一条消息永远只占一行。
//   → T <id> <from> <to> <文本>     翻译；非英语互译时 worker 自己经英语中转
//   → L <from> <to> <配置文件>      加载一个方向的模型
//   → Q                              退出
//   ← READY                          启动完成
//   ← R <id> <毫秒> <译文>
//   ← E <id> <错误码> <说明>         id 为 0 表示不属于某个翻译请求（如加载失败）
//   ← K <from> <to> <毫秒>           加载完成
namespace worker_protocol {

constexpr const char* kNoModel = "NO_MODEL";
constexpr const char* kBadRequest = "BAD_REQUEST";
constexpr const char* kInternal = "INTERNAL";

inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out;
}

inline std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) {
            out += s[i];
            continue;
        }
        switch (s[++i]) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case '\\': out += '\\'; break;
        default: out += '\\'; out += s[i];   // 未知转义原样保留，不丢字符
        }
    }
    return out;
}

// 按 TAB 切成最多 maxParts 段，最后一段保留剩余内容（译文里的 TAB 已被转义，这里只是兜底）
inline std::vector<std::string> splitFields(const std::string& line, size_t maxParts) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (parts.size() + 1 < maxParts) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) break;
        parts.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    parts.push_back(line.substr(start));
    return parts;
}

// ---- 请求（主程序 → worker）
struct Request {
    enum class Kind { Invalid, Translate, Load, Quit };
    Kind kind = Kind::Invalid;
    int64_t id = 0;
    std::string from;
    std::string to;
    std::string text;   // Translate：原文；Load：配置文件路径
};

inline std::string formatTranslate(int64_t id, const std::string& from, const std::string& to, const std::string& text) {
    return "T\t" + std::to_string(id) + "\t" + from + "\t" + to + "\t" + escape(text);
}

inline std::string formatLoad(const std::string& from, const std::string& to, const std::string& configPath) {
    return "L\t" + from + "\t" + to + "\t" + escape(configPath);
}

inline std::string formatQuit() { return "Q"; }

inline Request parseRequest(const std::string& line) {
    Request r;
    if (line.empty()) return r;

    if (line == "Q") {
        r.kind = Request::Kind::Quit;
        return r;
    }

    if (line[0] == 'T') {
        const auto f = splitFields(line, 5);
        if (f.size() != 5 || f[0] != "T" || f[2].empty() || f[3].empty()) return r;
        try { r.id = std::stoll(f[1]); } catch (...) { return r; }
        r.kind = Request::Kind::Translate;
        r.from = f[2];
        r.to = f[3];
        r.text = unescape(f[4]);
        return r;
    }

    if (line[0] == 'L') {
        const auto f = splitFields(line, 4);
        if (f.size() != 4 || f[0] != "L" || f[1].empty() || f[2].empty() || f[3].empty()) return r;
        r.kind = Request::Kind::Load;
        r.from = f[1];
        r.to = f[2];
        r.text = unescape(f[3]);
        return r;
    }
    return r;
}

// ---- 响应（worker → 主程序）
struct Response {
    enum class Kind { Invalid, Ready, Result, Error, Loaded };
    Kind kind = Kind::Invalid;
    int64_t id = 0;
    int64_t ms = 0;
    std::string from;
    std::string to;
    std::string text;   // Result：译文；Error：说明
    std::string code;   // Error：错误码
};

inline std::string formatReady() { return "READY"; }

inline std::string formatResult(int64_t id, int64_t ms, const std::string& text) {
    return "R\t" + std::to_string(id) + "\t" + std::to_string(ms) + "\t" + escape(text);
}

inline std::string formatError(int64_t id, const std::string& code, const std::string& message) {
    return "E\t" + std::to_string(id) + "\t" + code + "\t" + escape(message);
}

inline std::string formatLoaded(const std::string& from, const std::string& to, int64_t ms) {
    return "K\t" + from + "\t" + to + "\t" + std::to_string(ms);
}

inline Response parseResponse(const std::string& line) {
    Response r;
    if (line == "READY") {
        r.kind = Response::Kind::Ready;
        return r;
    }
    if (line.size() < 2) return r;

    try {
        if (line[0] == 'R') {
            const auto f = splitFields(line, 4);
            if (f.size() != 4 || f[0] != "R") return r;
            r.id = std::stoll(f[1]);
            r.ms = std::stoll(f[2]);
            r.text = unescape(f[3]);
            r.kind = Response::Kind::Result;
        }
        else if (line[0] == 'E') {
            const auto f = splitFields(line, 4);
            if (f.size() != 4 || f[0] != "E") return r;
            r.id = std::stoll(f[1]);
            r.code = f[2];
            r.text = unescape(f[3]);
            r.kind = Response::Kind::Error;
        }
        else if (line[0] == 'K') {
            const auto f = splitFields(line, 4);
            if (f.size() != 4 || f[0] != "K") return r;
            r.from = f[1];
            r.to = f[2];
            r.ms = std::stoll(f[3]);
            r.kind = Response::Kind::Loaded;
        }
    }
    catch (...) {
        return Response{};
    }
    return r;
}

}
