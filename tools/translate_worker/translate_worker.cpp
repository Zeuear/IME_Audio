// 本地翻译 worker：常驻进程，按行协议（WorkerProtocol.h）接收请求，用 Bergamot 翻译。
// 独立进程是因为 Bergamot 依赖静态 CRT（/MT），与主程序的 /MD 不能链进同一个二进制；
// 同时翻译引擎崩溃不会带走听写主链路。
#include "WorkerProtocol.h"

#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace mb = marian::bergamot;
namespace wp = worker_protocol;

namespace {

// 同时驻留的方向数上限。一次中转需要两个方向，所以不能小于 2；内存占用见 spec 附录 B
constexpr size_t kMaxModels = 4;

struct LoadedModel {
    std::shared_ptr<mb::TranslationModel> model;
    uint64_t lastUsed = 0;
};

class Worker {
public:
    Worker() : m_service(makeConfig()) {}

    void load(const wp::Request& req) {
        const auto start = std::chrono::steady_clock::now();
        try {
            auto options = mb::parseOptionsFromFilePath(req.text);
            LoadedModel entry;
            entry.model = m_service.createCompatibleModel(options);
            entry.lastUsed = ++m_tick;
            m_models[key(req.from, req.to)] = entry;
            evictIfNeeded(key(req.from, req.to));
        }
        catch (const std::exception& e) {
            reply(wp::formatError(0, wp::kInternal, std::string("load ") + req.from + ">" + req.to + " failed: " + e.what()));
            return;
        }
        reply(wp::formatLoaded(req.from, req.to, elapsedMs(start)));
    }

    void translate(const wp::Request& req) {
        const auto start = std::chrono::steady_clock::now();
        if (req.text.empty()) {
            reply(wp::formatResult(req.id, 0, {}));
            return;
        }

        const auto direct = m_models.find(key(req.from, req.to));
        const bool needPivot = direct == m_models.end() && req.from != "en" && req.to != "en";
        const auto first = m_models.find(key(req.from, "en"));
        const auto second = m_models.find(key("en", req.to));

        if (direct == m_models.end() && !(needPivot && first != m_models.end() && second != m_models.end())) {
            reply(wp::formatError(req.id, wp::kNoModel, "model " + req.from + ">" + req.to + " not loaded"));
            return;
        }

        try {
            std::promise<mb::Response> promise;
            auto future = promise.get_future();
            auto callback = [&promise](mb::Response&& response) { promise.set_value(std::move(response)); };

            std::string source = req.text;
            mb::ResponseOptions options;
            if (direct != m_models.end()) {
                direct->second.lastUsed = ++m_tick;
                m_service.translate(direct->second.model, std::move(source), callback, options);
            }
            else {
                first->second.lastUsed = second->second.lastUsed = ++m_tick;
                m_service.pivot(first->second.model, second->second.model, std::move(source), callback, options);
            }
            mb::Response response = future.get();
            reply(wp::formatResult(req.id, elapsedMs(start), response.target.text));
        }
        catch (const std::exception& e) {
            reply(wp::formatError(req.id, wp::kInternal, e.what()));
        }
    }

private:
    static mb::AsyncService::Config makeConfig() {
        mb::AsyncService::Config cfg;
        cfg.numWorkers = 2;   // 与质量回归时验证过的线程数一致（OpenBLAS 已加锁）
        return cfg;
    }

    static std::string key(const std::string& from, const std::string& to) { return from + ">" + to; }

    static int64_t elapsedMs(std::chrono::steady_clock::time_point start) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    }

    // 淘汰最久没用的方向，刚加载的不动
    void evictIfNeeded(const std::string& keep) {
        while (m_models.size() > kMaxModels) {
            auto oldest = m_models.end();
            for (auto it = m_models.begin(); it != m_models.end(); ++it) {
                if (it->first == keep) continue;
                if (oldest == m_models.end() || it->second.lastUsed < oldest->second.lastUsed) oldest = it;
            }
            if (oldest == m_models.end()) return;
            m_models.erase(oldest);
        }
    }

    static void reply(const std::string& line) {
        std::cout << line << '\n' << std::flush;
    }

    mb::AsyncService m_service;
    std::map<std::string, LoadedModel> m_models;
    uint64_t m_tick = 0;
};

}  // namespace

int main() {
#ifdef _WIN32
    // 二进制模式：不让 CRT 把 \n 改成 \r\n，也不做代码页转换，译文按 UTF-8 原样进出
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    Worker worker;
    std::cout << wp::formatReady() << '\n' << std::flush;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        const wp::Request req = wp::parseRequest(line);
        switch (req.kind) {
        case wp::Request::Kind::Translate: worker.translate(req); break;
        case wp::Request::Kind::Load: worker.load(req); break;
        case wp::Request::Kind::Quit: return 0;
        case wp::Request::Kind::Invalid:
            std::cout << wp::formatError(0, wp::kBadRequest, "cannot parse request") << '\n' << std::flush;
            break;
        }
    }
    return 0;   // 主程序关闭 stdin 视同退出
}
