#include <gtest/gtest.h>

#include "../../tools/translate_worker/WorkerProtocol.h"

using namespace worker_protocol;

TEST(WorkerProtocolTest, EscapeRoundTripsSpecialCharacters) {
    const std::string text = "a\tb\nc\r\nd\\e\\n 中文";
    const std::string escaped = escape(text);

    // 转义后不能含有真实的 TAB 或换行，否则一条消息会占多行
    EXPECT_EQ(escaped.find('\t'), std::string::npos);
    EXPECT_EQ(escaped.find('\n'), std::string::npos);
    EXPECT_EQ(escaped.find('\r'), std::string::npos);
    EXPECT_EQ(unescape(escaped), text);
}

TEST(WorkerProtocolTest, UnknownEscapeAndTrailingBackslashAreKept) {
    EXPECT_EQ(unescape("a\\xb"), "a\\xb");
    EXPECT_EQ(unescape("end\\"), "end\\");
}

TEST(WorkerProtocolTest, TranslateRequestRoundTrip) {
    const std::string line = formatTranslate(42, "zh-Hans", "en", "第一行\n第二行\t带TAB");
    EXPECT_EQ(line.find('\n'), std::string::npos);

    const Request r = parseRequest(line);
    ASSERT_EQ(r.kind, Request::Kind::Translate);
    EXPECT_EQ(r.id, 42);
    EXPECT_EQ(r.from, "zh-Hans");
    EXPECT_EQ(r.to, "en");
    EXPECT_EQ(r.text, "第一行\n第二行\t带TAB");
}

TEST(WorkerProtocolTest, LoadAndQuitRequests) {
    const Request load = parseRequest(formatLoad("en", "de", "D:/models/en-de/config.yml"));
    ASSERT_EQ(load.kind, Request::Kind::Load);
    EXPECT_EQ(load.from, "en");
    EXPECT_EQ(load.to, "de");
    EXPECT_EQ(load.text, "D:/models/en-de/config.yml");

    EXPECT_EQ(parseRequest(formatQuit()).kind, Request::Kind::Quit);
}

TEST(WorkerProtocolTest, MalformedRequestsAreInvalid) {
    EXPECT_EQ(parseRequest("").kind, Request::Kind::Invalid);
    EXPECT_EQ(parseRequest("X\t1").kind, Request::Kind::Invalid);
    EXPECT_EQ(parseRequest("T\tnot-a-number\ten\tde\thi").kind, Request::Kind::Invalid);
    EXPECT_EQ(parseRequest("T\t1\ten\tde").kind, Request::Kind::Invalid);       // 缺文本字段
    EXPECT_EQ(parseRequest("T\t1\t\tde\thi").kind, Request::Kind::Invalid);     // 缺源语言
    EXPECT_EQ(parseRequest("L\ten\tde").kind, Request::Kind::Invalid);
}

TEST(WorkerProtocolTest, EmptyTextIsAValidTranslateRequest) {
    const Request r = parseRequest("T\t7\ten\tde\t");
    ASSERT_EQ(r.kind, Request::Kind::Translate);
    EXPECT_TRUE(r.text.empty());
}

TEST(WorkerProtocolTest, ResponsesRoundTrip) {
    EXPECT_EQ(parseResponse(formatReady()).kind, Response::Kind::Ready);

    const Response result = parseResponse(formatResult(5, 123, "Hallo\nWelt"));
    ASSERT_EQ(result.kind, Response::Kind::Result);
    EXPECT_EQ(result.id, 5);
    EXPECT_EQ(result.ms, 123);
    EXPECT_EQ(result.text, "Hallo\nWelt");

    const Response error = parseResponse(formatError(9, kNoModel, "model en>de not loaded"));
    ASSERT_EQ(error.kind, Response::Kind::Error);
    EXPECT_EQ(error.id, 9);
    EXPECT_EQ(error.code, kNoModel);
    EXPECT_EQ(error.text, "model en>de not loaded");

    const Response loaded = parseResponse(formatLoaded("en", "de", 850));
    ASSERT_EQ(loaded.kind, Response::Kind::Loaded);
    EXPECT_EQ(loaded.from, "en");
    EXPECT_EQ(loaded.to, "de");
    EXPECT_EQ(loaded.ms, 850);
}

TEST(WorkerProtocolTest, MalformedResponsesAreInvalid) {
    EXPECT_EQ(parseResponse("").kind, Response::Kind::Invalid);
    EXPECT_EQ(parseResponse("garbage").kind, Response::Kind::Invalid);
    EXPECT_EQ(parseResponse("R\tx\ty\ttext").kind, Response::Kind::Invalid);
    EXPECT_EQ(parseResponse("R\t1\t2").kind, Response::Kind::Invalid);
}
