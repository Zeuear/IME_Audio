#include <gtest/gtest.h>
#include <QObject>

#include "WorkflowManager.h"
#include "interfaces/workflow_interfaces.h"

class FakeRecorder : public IRecorder {
    Q_OBJECT
public:
    explicit FakeRecorder(QObject* parent = nullptr) : IRecorder(parent) {}

    bool startListening() override { ++startListeningCalls; return true; }
    void stopListening() override { ++stopListeningCalls; if (emitUtteranceOnStop) emit utteranceReady(QByteArray("final"), 16000); }
    bool isListening() const override { return listening; }
    bool isPaused() const override { return paused; }
    void pause() override { paused = true; }
    void resume() override { paused = false; }
    void updateConfig() override { ++updateConfigCalls; }
    void playTestTone() override { ++playTestToneCalls; }

    // 测试侧驱动上行信号
    void emitUtteranceReady(const QByteArray& pcm = QByteArray("x"), int sr = 16000) {
        emit utteranceReady(pcm, sr);
    }
    void emitVoiceStarted() { emit voiceStarted(); }
    void emitVoiceStopped() { emit voiceStopped(); }

    int startListeningCalls = 0;
    int stopListeningCalls = 0;
    int updateConfigCalls = 0;
    int playTestToneCalls = 0;
    bool listening = false;
    bool paused = false;
    bool emitUtteranceOnStop = false;   // 模拟非连续模式 stopListening 产出最终 utterance
};

class FakeTranscription : public ITranscription {
    Q_OBJECT
public:
    explicit FakeTranscription(QObject* parent = nullptr) : ITranscription(parent) {}
    void transcribe(const QByteArray&, int, int, int) override { ++transcribeCalls; }
    void emitFinished(bool ok, const QString& finalText = "txt") {
        emit transcriptionFinished(ok, ok ? "raw" : "err", finalText, ok ? "" : "boom");
    }
    int transcribeCalls = 0;
};

class FakeSherpaModel : public ISherpaModel {
    Q_OBJECT
public:
    explicit FakeSherpaModel(QObject* parent = nullptr) : ISherpaModel(parent) {}

    bool reloadModel(const AppConfig&) override { ++reloadCalls; return reloadReturns; }
    void loadModelAsync(const AppConfig&, bool) override { ++loadAsyncCalls; }
    bool isModelLoaded() const override { return loaded; }
    void pauseIdleTimer() override { ++pauseIdleCalls; }
    void resumeIdleTimer() override { ++resumeIdleCalls; }

    void emitModelLoadFinished(bool ok) { emit modelLoadFinished(ok); }

    bool reloadReturns = true;   // true=发起了异步加载（需等 modelLoadFinished）
    bool loaded = true;
    int reloadCalls = 0;
    int loadAsyncCalls = 0;
    int pauseIdleCalls = 0;
    int resumeIdleCalls = 0;
};

// ---- 测试夹具 ----

class WorkflowManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        rec = new FakeRecorder;
        trans = new FakeTranscription;
        sherpa = new FakeSherpaModel;
        wf = new WorkflowManager(config);   // 默认 backend=Sherpa
        wf->initialize(rec, trans, sherpa);
        // 父级联：删除 wf 时一并清理 fakes
        rec->setParent(wf);
        trans->setParent(wf);
        sherpa->setParent(wf);
    }
    void TearDown() override {
        delete wf;      // 子对象 rec/trans/sherpa 随 wf 的 parent 树销毁（见下）
    }

    AppConfig config;
    FakeRecorder* rec = nullptr;
    FakeTranscription* trans = nullptr;
    FakeSherpaModel* sherpa = nullptr;
    WorkflowManager* wf = nullptr;
};

// T2 smoke：构造 + start → 进入 Recording
TEST_F(WorkflowManagerTest, Smoke_StartEntersRecording) {
    wf->start();
    EXPECT_EQ(wf->state(), WorkflowState::Loading);     // 异步加载中
    EXPECT_EQ(sherpa->reloadCalls, 1);
    sherpa->emitModelLoadFinished(true);                // 模拟 worker 完成
    EXPECT_EQ(wf->state(), WorkflowState::Recording);
    EXPECT_EQ(rec->startListeningCalls, 1);
}

// T3 基础转移：utterance → transcribe → finished → Processing → (全完) → Recording
TEST_F(WorkflowManagerTest, Basic_RecordingToTranscribingToRecording) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    ASSERT_EQ(wf->state(), WorkflowState::Recording);

    rec->emitUtteranceReady();
    EXPECT_EQ(wf->state(), WorkflowState::Transcribing);
    EXPECT_EQ(trans->transcribeCalls, 1);

    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Idle);   // 单句：转完回 Recording
}

// T3 门面透传
TEST_F(WorkflowManagerTest, Facade_CommandsForwardToCollaborators) {
    wf->playTestTone();
    EXPECT_EQ(rec->playTestToneCalls, 1);

    wf->applyRecorderConfig();
    EXPECT_EQ(rec->updateConfigCalls, 1);

    wf->togglePause();
    EXPECT_TRUE(rec->paused);
    wf->togglePause();
    EXPECT_FALSE(rec->paused);

    AppConfig cfg;
    wf->preloadModel(cfg);
    EXPECT_EQ(sherpa->loadAsyncCalls, 1);
}

// T3 非法转移防御：Idle 直接收到 transcriptionFinished 被拒（状态不变且计数不被破坏）
TEST_F(WorkflowManagerTest, Illegal_IdleReceivesTranscriptionIgnored) {
    // 尚未 start，直接发 transcriptionFinished
    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
    EXPECT_GE(wf->pendingCount(), 0);   // 计数不应被非法信号改负
}

// T4 R4 连说 3 句
TEST_F(WorkflowManagerTest, Burst_ThreeUtterancesInterleaved) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    ASSERT_EQ(wf->state(), WorkflowState::Recording);

    rec->emitUtteranceReady();        // 1
    rec->emitUtteranceReady();        // 2（处理中又说话）
    trans->emitFinished(true);        // 句1 完成
    rec->emitUtteranceReady();        // 3
    trans->emitFinished(true);        // 句2 完成
    trans->emitFinished(true);        // 句3 完成
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T4 大量零间隔注入 10 句
TEST_F(WorkflowManagerTest, Burst_TenUtterancesNoGap) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    for (int i = 0; i < 10; ++i) rec->emitUtteranceReady();
    for (int i = 0; i < 10; ++i) trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T4 长时间 Recording 空闲后再次说话
TEST_F(WorkflowManagerTest, IdleLongRecordingThenSpeak) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    ASSERT_EQ(wf->state(), WorkflowState::Recording);
    // 不发任何信号（模拟长时间无声）
    rec->emitUtteranceReady();
    EXPECT_EQ(wf->state(), WorkflowState::Transcribing);
}

// T4 处理中又说话（Processing + UtteranceCaptured → Transcribing）
TEST_F(WorkflowManagerTest, ProcessingThenSpeakAgain) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    rec->emitUtteranceReady();          // → Transcribing
    trans->emitFinished(true);          // → Processing
    rec->emitUtteranceReady();          // 处理中又说话 → Transcribing
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T5 停止冲刷（A 语义）：停止后仍有 2 句 → 全转完落 Idle
TEST_F(WorkflowManagerTest, Stop_FlushPendingToIdle) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    rec->emitUtteranceReady();          // pending 1
    rec->emitUtteranceReady();          // pending 2
    wf->stop();                         // → Stopping
    EXPECT_EQ(wf->state(), WorkflowState::Stopping);
    EXPECT_EQ(rec->stopListeningCalls, 1);
    trans->emitFinished(true);          // 句1
    EXPECT_EQ(wf->state(), WorkflowState::Stopping);
    trans->emitFinished(true);          // 句2 → 全完 → Idle
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T5 从 Transcribing 停止
TEST_F(WorkflowManagerTest, Stop_FromTranscribingThenFlush) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    rec->emitUtteranceReady();          // → Transcribing, pending 1
    wf->stop();                         // → Stopping
    EXPECT_EQ(wf->state(), WorkflowState::Stopping);
    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T5 Loading 中停止：modelLoadFinished 被忽略
TEST_F(WorkflowManagerTest, Stop_DuringLoadingIgnoresModelLoaded) {
    wf->start();                        // Loading
    wf->stop();                         // Stopping（pending==0 立即 Idle）
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
    sherpa->emitModelLoadFinished(true); // 应被忽略
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T5 冲刷期转录失败仍 drain 到 Idle（≥2 句，首句失败，验证后续仍继续 drain）
TEST_F(WorkflowManagerTest, Stop_FlushWithFailureStillIdle) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    rec->emitUtteranceReady();          // pending 1
    rec->emitUtteranceReady();          // pending 2
    wf->stop();                         // Stopping
    trans->emitFinished(false);         // 句1 失败，不应阻断冲刷
    EXPECT_EQ(wf->state(), WorkflowState::Stopping);
    trans->emitFinished(true);          // 句2 成功 → 全完 → Idle
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T5 已加载快速路径：reloadModel 返 false → 直进 Recording
TEST_F(WorkflowManagerTest, Load_AlreadyLoadedFastPath) {
    sherpa->reloadReturns = false;
    wf->start();
    EXPECT_EQ(wf->state(), WorkflowState::Recording);  // 不经 Loading 等待
    EXPECT_EQ(rec->startListeningCalls, 1);
}

// T5 模型加载失败 → Error
TEST_F(WorkflowManagerTest, Load_FailureToError) {
    wf->start();                        // Loading
    EXPECT_EQ(wf->state(), WorkflowState::Loading);
    sherpa->emitModelLoadFinished(false);
    EXPECT_EQ(wf->state(), WorkflowState::Error);
}

// T5 快速 start/stop 来回不卡死
TEST_F(WorkflowManagerTest, Rapid_StartStopConverges) {
    for (int i = 0; i < 5; ++i) {
        wf->start();
        sherpa->emitModelLoadFinished(true);
        wf->stop();
        trans->emitFinished(true);
    }
    EXPECT_TRUE(wf->state() == WorkflowState::Idle || wf->state() == WorkflowState::Recording);
}

// T6 continuousMode=false 停止后 utterance 才到达
// 真实情况：非连续模式下 stopListening() 经 finalizeSegmentIfNeeded 同步 emit utteranceReady，
// 即 utterance 在 stopRecording 进入 Stopping 之后、但 stop() 判定 m_pending 之前到达。
TEST_F(WorkflowManagerTest, ContinuousModeOff_StopThenUtteranceArrives) {
    rec->emitUtteranceOnStop = true;    // 模拟非连续模式 stopListening 产出最终句
    wf->start();
    sherpa->emitModelLoadFinished(true);
    ASSERT_EQ(wf->state(), WorkflowState::Recording);
    wf->stop();                         // → Stopping；stopListening 同步 emit utterance → m_pending=1 → 转 Transcribing
    EXPECT_EQ(wf->state(), WorkflowState::Transcribing);   // 已进入 Transcribing（非被拒，非卡在 Stopping）
    EXPECT_EQ(rec->stopListeningCalls, 1);
    // stopListening 已产出 utterance，正在转写；转完 → 全完 → Idle（m_stopping 持久意图生效）
    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// T6 停止后多句连续到达（stopListening 出一句 + 仍可能再 buffer 一句）
TEST_F(WorkflowManagerTest, ContinuousModeOff_StopThenMultipleUtterances) {
    rec->emitUtteranceOnStop = true;
    wf->start();
    sherpa->emitModelLoadFinished(true);
    wf->stop();                         // Stopping；stopListening 出 1 句
    rec->emitUtteranceReady();          // pending +1（缓冲/后续句）
    trans->emitFinished(true);
    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

// ---- 连续模式：m_stopping 不得触发关窗（回归 T6 续）----

class WorkflowManagerContinuousTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.continuousMode.store(true);   // 关键：开启连续监听
        rec = new FakeRecorder;
        trans = new FakeTranscription;
        sherpa = new FakeSherpaModel;
        wf = new WorkflowManager(config);
        wf->initialize(rec, trans, sherpa);
        rec->setParent(wf); trans->setParent(wf); sherpa->setParent(wf);
    }
    void TearDown() override { delete wf; }

    AppConfig config;
    FakeRecorder* rec = nullptr;
    FakeTranscription* trans = nullptr;
    FakeSherpaModel* sherpa = nullptr;
    WorkflowManager* wf = nullptr;
};

// 连续模式：一句转录完成后应回到 Recording 常驻，绝不因冲刷关窗
TEST_F(WorkflowManagerContinuousTest, StaysRecordingAfterTranscription) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    rec->emitUtteranceReady();
    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Recording);   // 非 Idle/关窗
}

// 连续模式：正常多句流转不关窗（模拟一会儿变 true 的历史残留场景）
TEST_F(WorkflowManagerContinuousTest, MultipleUtterancesStayRecording) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    for (int i = 0; i < 3; ++i) {
        rec->emitUtteranceReady();
        trans->emitFinished(true);
    }
    EXPECT_EQ(wf->state(), WorkflowState::Recording);
}

// 连续模式：用户显式 stop 仍应真正关窗（forceStop 路径）
TEST_F(WorkflowManagerContinuousTest, ExplicitStopClosesWindow) {
    wf->start();
    sherpa->emitModelLoadFinished(true);
    rec->emitUtteranceReady();
    trans->emitFinished(true);
    EXPECT_EQ(wf->state(), WorkflowState::Recording);

    wf->stop();                       // 连续模式下显式停止
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}

#include "workflow_manager_test.moc"


