# @brief 基准测试的语言表与模型表，对照 src/sherpa/SherpaConfig.cpp 的 LanguageTables()。
#
# 多语言分组（31 languages / 1600+ / 25 European）不单独测。
# 模型参数（线程数、文件名、Whisper 不指定语言）与 ModelRegistry::GetConfig 保持一致，
# 这样分数反映的是软件里真实跑出来的效果。
# in_app=False 的模型软件里没有，只作对照（如 FireRed、非商用许可的 Shenava）。
from dataclasses import dataclass, field


@dataclass
class Lang:
    fleurs: str | None       # FLEURS 语言代码；None 表示没有可用公开集
    metric: str              # 主指标：无空格分词的语言用 cer，其余用 wer
    models: list[str] = field(default_factory=list)
    note: str = ""
    new: bool = False        # 软件里尚未提供的语种
    in_app: bool = True      # False：只测不进软件（不导出到 model_benchmarks.json）


# key 为软件语言列表里的名字
LANGS: dict[str, Lang] = {
    "Chinese":    Lang("cmn_hans_cn", "cer", ["paraformer-zh", "zipformer-ctc-zh", "zipformer-multi-zh-en",
                                              "sense-voice", "sense-voice-2024", "qwen3-asr", "funasr-nano", "fire-red-asr"]),
    "English":    Lang("en_us", "wer", ["parakeet-tdt-v3", "whisper-tiny.en", "whisper-base.en", "whisper-small.en",
                                        "moonshine-base-en", "paraformer-en", "zipformer-multi-zh-en",
                                        "sense-voice", "sense-voice-2024", "nemo-fc-20k", "fire-red-asr"]),
    # ka/hy/tl：sherpa 没有其它专用模型，补测多语言模型作为替换候选
    "Georgian":   Lang("ka_ge", "wer", ["nemo-ka", "whisper-base", "whisper-small", "omnilingual-300m", "omnilingual-1b"]),
    "Armenian":   Lang("hy_am", "wer", ["nemo-hy", "whisper-base", "whisper-small", "omnilingual-300m", "omnilingual-1b"]),
    "Arabic":     Lang("ar_eg", "wer", ["nemo-ar", "whisper-base"]),
    "Cantonese":  Lang("yue_hant_hk", "cer", ["wenet-yue", "mdcc-zipformer", "sense-voice", "sense-voice-2024"]),
    "French":     Lang("fr_fr", "wer", ["parakeet-tdt-v3", "whisper-base"]),
    "German":     Lang("de_de", "wer", ["parakeet-tdt-v3", "nemo-transducer-de", "nemo-ctc-de", "whisper-base"]),
    "Japanese":   Lang("ja_jp", "cer", ["reazonspeech", "sense-voice", "sense-voice-2024", "whisper-base"]),
    "Korean":     Lang("ko_kr", "cer", ["zipformer-ko", "sense-voice", "sense-voice-2024", "whisper-base"]),
    "Portuguese": Lang("pt_br", "wer", ["parakeet-tdt-v3", "nemo-ctc-pt", "nemo-transducer-pt", "whisper-base"]),
    "Russian":    Lang("ru_ru", "wer", ["gigaam-ctc-v3", "gigaam-ctc-v2", "gigaam-ctc-v1",
                                        "gigaam-rnnt-v3", "gigaam-rnnt-v2", "gigaam-rnnt-v1", "gigaam-rnnt-punct-v3",
                                        "parakeet-tdt-v3", "whisper-base", "vosk-ru", "vosk-small-ru"]),
    "Spanish":    Lang("es_419", "wer", ["parakeet-tdt-v3", "whisper-base", "nemo-ctc-es"]),
    "Thai":       Lang("th_th", "cer", ["zipformer-th", "whisper-base"]),
    "Tibetan":    Lang(None, "cer", ["whisper-base"],
                       "FLEURS / Common Voice have no Tibetan; no open test set with a stable download"),
    "Vietnamese": Lang("vi_vn", "wer", ["zipformer-vi", "whisper-base"]),
    "Tagalog":    Lang("fil_ph", "wer", ["nemo-tl", "whisper-base", "qwen3-asr", "whisper-small",
                                         "omnilingual-300m", "omnilingual-1b"]),

    # ---- 新增语种候选 ----
    "Italian":    Lang("it_it", "wer", ["parakeet-tdt-v3", "nemo-fc-20k", "qwen3-asr", "whisper-base"], new=True),
    "Ukrainian":  Lang("uk_ua", "wer", ["parakeet-tdt-v3", "nemo-fc-20k", "moonshine-base-uk", "whisper-base"], new=True),
    "Polish":     Lang("pl_pl", "wer", ["parakeet-tdt-v3", "nemo-fc-20k", "qwen3-asr", "whisper-base"], new=True),
    "Dutch":      Lang("nl_nl", "wer", ["parakeet-tdt-v3", "qwen3-asr", "whisper-base"], new=True),
    "Turkish":    Lang("tr_tr", "wer", ["qwen3-asr", "whisper-base"], new=True),
    "Indonesian": Lang("id_id", "wer", ["qwen3-asr", "zipformer-streaming-8lang", "whisper-base"], new=True),
    "Hindi":      Lang("hi_in", "wer", ["qwen3-asr", "whisper-base"], new=True),
    "Persian":    Lang("fa_ir", "wer", ["qwen3-asr", "shenava-rizeh-fa", "shenava-koochik-fa", "whisper-base"], new=True,
                       # 通用模型都不可用（Qwen3 CER 58%），唯一可用的 Shenava 为 CC-BY-NC 非商用许可
                       in_app=False),
    "Hebrew":     Lang("he_il", "wer", ["whisper-base", "whisper-small", "omnilingual-300m", "omnilingual-1b"], new=True),
}


# 测过但没放进软件对应语言表的组合（得分 < 50 且有更好选择，或有缺陷），不导出到 model_benchmarks.json
NOT_IN_APP: set[tuple[str, str]] = {
    ("English", "zipformer-multi-zh-en"),
    ("English", "sense-voice"), ("Chinese", "sense-voice"),          # 已换成 2024-07-17 原版
    ("Japanese", "sense-voice"), ("Korean", "sense-voice"),
    ("Cantonese", "sense-voice-2024"),
    ("Korean", "zipformer-ko"),          # 超过约 10 秒的音频返回空结果
    ("Russian", "vosk-small-ru"),        # 超过约 20 秒的音频推理报错
    ("Ukrainian", "moonshine-base-uk"),  # 超过约 9 秒的音频返回空结果
    ("Ukrainian", "whisper-base"), ("Arabic", "whisper-base"), ("Thai", "whisper-base"),
    ("Georgian", "whisper-base"), ("Georgian", "whisper-small"),
    ("Armenian", "whisper-base"), ("Armenian", "whisper-small"),
    ("Tagalog", "whisper-base"), ("Tagalog", "whisper-small"), ("Tagalog", "qwen3-asr"),
    ("Hindi", "whisper-base"),
    ("Hebrew", "whisper-base"), ("Hebrew", "whisper-small"),
}


@dataclass
class Model:
    repo: str              # HF repo id（与软件的 repoId 一致）；目录名取最后一段
    arch: str
    files: dict[str, str]  # 角色 -> 相对路径
    threads: int
    display: str
    in_app: bool = True    # False：软件中没有（候选或被注释掉的）
    archive: str = ""      # 不在 HF 上时的 tar.bz2 下载地址
    kw: dict = field(default_factory=dict)

    @property
    def dir(self) -> str:
        return self.repo.split("/")[-1]


def _tr(enc, dec, join, tokens="tokens.txt"):
    return {"encoder": enc, "decoder": dec, "joiner": join, "tokens": tokens}


def _w(name):
    return {"encoder": f"{name}-encoder.int8.onnx", "decoder": f"{name}-decoder.int8.onnx", "tokens": f"{name}-tokens.txt"}


_TR_INT8 = _tr("encoder.int8.onnx", "decoder.int8.onnx", "joiner.int8.onnx")
_TR_FP32 = _tr("encoder.onnx", "decoder.onnx", "joiner.onnx")
_GIGA_RNNT = _tr("encoder.int8.onnx", "decoder.onnx", "joiner.onnx")
_CTC_INT8 = {"model": "model.int8.onnx", "tokens": "tokens.txt"}
_E99 = _tr("encoder-epoch-99-avg-1.onnx", "decoder-epoch-99-avg-1.onnx", "joiner-epoch-99-avg-1.onnx")

# 线程数：app 默认 sherpa.threads=4；GetConfig 中 TransducerOffline / Whisper / Moonshine /
# FireRed / NemoCtc 写死为 2
MODELS: dict[str, Model] = {
    # 中文
    "paraformer-zh": Model("csukuangfj/sherpa-onnx-paraformer-zh-2024-03-09", "paraformer", _CTC_INT8, 4, "Paraformer zh"),
    "zipformer-ctc-zh": Model("csukuangfj/sherpa-onnx-zipformer-ctc-zh-int8-2025-07-03", "zipformer_ctc", _CTC_INT8, 4, "Zipformer CTC zh"),
    "zipformer-multi-zh-en": Model("zrjin/icefall-asr-zipformer-multi-zh-en-2023-11-22", "transducer",
                                   _tr("exp/encoder-epoch-34-avg-19.int8.onnx", "exp/decoder-epoch-34-avg-19.onnx",
                                       "exp/joiner-epoch-34-avg-19.int8.onnx", "data/lang_bbpe_2000/tokens.txt"),
                                   2, "Zipformer multi zh-en"),
    "sense-voice": Model("csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2025-09-09", "sense_voice",
                         {"model": "model.int8.onnx", "tokens": "tokens.txt"}, 4, "SenseVoice 2025-09-09 (WSYue)"),
    # 2025-09-09 版实为 ASLP-lab WSYue 粤语微调版，会把日/韩语识别成粤语；原版对照
    "sense-voice-2024": Model("csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17", "sense_voice",
                              {"model": "model.int8.onnx", "tokens": "tokens.txt"}, 4, "SenseVoice 2024-07-17"),
    "qwen3-asr": Model("k2-fsa/sherpa-onnx-qwen3-asr-0.6B-int8-2026-03-25", "qwen3",
                       {"conv_frontend": "conv_frontend.onnx", "encoder": "encoder.int8.onnx",
                        "decoder": "decoder.int8.onnx", "tokenizer": "tokenizer"}, 4, "Qwen3-ASR 0.6B",
                       archive="https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
                               "sherpa-onnx-qwen3-asr-0.6B-int8-2026-03-25.tar.bz2"),
    "funasr-nano": Model("csukuangfj/sherpa-onnx-funasr-nano-int8-2025-12-30", "funasr_nano",
                         {"embedding": "embedding.int8.onnx", "encoder_adaptor": "encoder_adaptor.int8.onnx",
                          "llm": "llm.int8.onnx", "tok_json": "Qwen3-0.6B/tokenizer.json",
                          "tok_merges": "Qwen3-0.6B/merges.txt", "tok_vocab": "Qwen3-0.6B/vocab.json"},
                         4, "FunASR Nano"),
    "fire-red-asr": Model("csukuangfj/sherpa-onnx-fire-red-asr-large-zh_en-2025-02-16", "fire_red",
                          {"encoder": "encoder.int8.onnx", "decoder": "decoder.int8.onnx", "tokens": "tokens.txt"},
                          2, "FireRed ASR large", in_app=False),
    # 英文
    "parakeet-tdt-v3": Model("csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8", "transducer", _TR_INT8, 4,
                             "Parakeet TDT 0.6B v3"),
    "whisper-tiny.en": Model("csukuangfj/sherpa-onnx-whisper-tiny.en", "whisper", _w("tiny.en"), 2, "Whisper tiny.en"),
    "whisper-base.en": Model("csukuangfj/sherpa-onnx-whisper-base.en", "whisper", _w("base.en"), 2, "Whisper base.en"),
    "whisper-small.en": Model("csukuangfj/sherpa-onnx-whisper-small.en", "whisper", _w("small.en"), 2, "Whisper small.en"),
    "moonshine-base-en": Model("csukuangfj/sherpa-onnx-moonshine-base-en-int8", "moonshine",
                               {"preprocessor": "preprocess.onnx", "encoder": "encode.int8.onnx",
                                "uncached_decoder": "uncached_decode.int8.onnx",
                                "cached_decoder": "cached_decode.int8.onnx", "tokens": "tokens.txt"},
                               2, "Moonshine base en"),
    "paraformer-en": Model("csukuangfj/sherpa-onnx-paraformer-en-2024-03-09", "paraformer", _CTC_INT8, 4, "Paraformer en"),
    "nemo-fc-20k": Model("csukuangfj/sherpa-onnx-nemo-fast-conformer-transducer-be-de-en-es-fr-hr-it-pl-ru-uk-20k",
                         "transducer", _TR_FP32, 4, "NeMo FastConformer 10-lang"),
    # 其它现有语种
    "whisper-base": Model("csukuangfj/sherpa-onnx-whisper-base", "whisper", _w("base"), 2, "Whisper base"),
    # 软件里 ka/hy/tl 用的是默认 int8 文件名，但仓库只有 fp32，这里按仓库实际文件测
    "nemo-ka": Model("LukeJacob2023/sherpa-onnx-stt_ka_fastconformer_hybrid_large_pc", "transducer", _TR_FP32, 4, "NeMo FastConformer ka"),
    "nemo-hy": Model("LukeJacob2023/sherpa-onnx-fastconformer-hybrid-arm-as", "transducer", _TR_FP32, 4, "NeMo FastConformer hy"),
    "nemo-tl": Model("LukeJacob2023/sherpa-onnx-stt_tl_fastconformer_hybrid_large", "transducer", _TR_FP32, 4, "NeMo FastConformer tl"),
    "nemo-ar": Model("LukeJacob2023/sherpa-onnx-stt_ar_fastconformer_hybrid_large_pc", "transducer", _TR_FP32, 4, "NeMo FastConformer ar"),
    "wenet-yue": Model("csukuangfj/sherpa-onnx-wenetspeech-yue-u2pp-conformer-ctc-zh-en-cantonese-int8-2025-09-10",
                       "wenet_ctc", _CTC_INT8, 4, "WeNetSpeech-Yue CTC"),
    "mdcc-zipformer": Model("zrjin/icefall-asr-mdcc-zipformer-2024-03-11", "transducer",
                            _tr("exp/encoder-epoch-45-avg-35.int8.onnx", "exp/decoder-epoch-45-avg-35.onnx",
                                "exp/joiner-epoch-45-avg-35.int8.onnx", "data/lang_char/tokens.txt"), 2, "Zipformer MDCC"),
    "nemo-transducer-de": Model("csukuangfj/sherpa-onnx-nemo-transducer-stt_de_fastconformer_hybrid_large_pc-int8",
                                "transducer", _TR_INT8, 4, "NeMo FastConformer RNNT de"),
    "nemo-ctc-de": Model("csukuangfj/sherpa-onnx-nemo-stt_de_fastconformer_hybrid_large_pc-int8", "nemo_ctc", _CTC_INT8, 2,
                         "NeMo FastConformer CTC de"),
    "reazonspeech": Model("reazon-research/reazonspeech-k2-v2", "transducer", _E99, 2, "ReazonSpeech k2 v2"),
    "zipformer-ko": Model("k2-fsa/sherpa-onnx-zipformer-korean-2024-06-24", "transducer", _E99, 2, "Zipformer ko"),
    "nemo-ctc-pt": Model("csukuangfj/sherpa-onnx-nemo-stt_pt_fastconformer_hybrid_large_pc-int8", "nemo_ctc", _CTC_INT8, 2,
                         "NeMo FastConformer CTC pt"),
    "nemo-transducer-pt": Model("csukuangfj/sherpa-onnx-nemo-transducer-stt_pt_fastconformer_hybrid_large_pc-int8",
                                "transducer", _TR_INT8, 4, "NeMo FastConformer RNNT pt"),
    "gigaam-ctc-v3": Model("csukuangfj/sherpa-onnx-nemo-ctc-giga-am-v3-russian-2025-12-16", "nemo_ctc", _CTC_INT8, 2, "GigaAM v3 CTC"),
    "gigaam-ctc-v2": Model("csukuangfj/sherpa-onnx-nemo-ctc-giga-am-v2-russian-2025-04-19", "nemo_ctc", _CTC_INT8, 2, "GigaAM v2 CTC"),
    "gigaam-ctc-v1": Model("csukuangfj/sherpa-onnx-nemo-ctc-giga-am-russian-2024-10-24", "nemo_ctc", _CTC_INT8, 2, "GigaAM v1 CTC"),
    "gigaam-rnnt-v3": Model("csukuangfj/sherpa-onnx-nemo-transducer-giga-am-v3-russian-2025-12-16", "transducer", _GIGA_RNNT, 4, "GigaAM v3 RNNT"),
    "gigaam-rnnt-v2": Model("csukuangfj/sherpa-onnx-nemo-transducer-giga-am-v2-russian-2025-04-19", "transducer", _GIGA_RNNT, 4, "GigaAM v2 RNNT"),
    "gigaam-rnnt-v1": Model("csukuangfj/sherpa-onnx-nemo-transducer-giga-am-russian-2024-10-24", "transducer", _GIGA_RNNT, 4, "GigaAM v1 RNNT"),
    "gigaam-rnnt-punct-v3": Model("csukuangfj/sherpa-onnx-nemo-transducer-punct-giga-am-v3-russian-2025-12-16", "transducer",
                                  _GIGA_RNNT, 4, "GigaAM v3 RNNT (punct)"),
    "vosk-ru": Model("alphacep/vosk-model-ru", "transducer",
                     _tr("am-onnx/encoder.int8.onnx", "am-onnx/decoder.int8.onnx", "am-onnx/joiner.int8.onnx",
                         "lang/tokens.txt"), 4, "Vosk ru"),
    "vosk-small-ru": Model("alphacep/vosk-model-small-ru", "transducer",
                           _tr("am/encoder.int8.onnx", "am/decoder.int8.onnx", "am/joiner.int8.onnx", "lang/tokens.txt"),
                           4, "Vosk small ru"),
    "nemo-ctc-es": Model("csukuangfj/sherpa-onnx-nemo-fast-conformer-ctc-es-1424-int8", "nemo_ctc", _CTC_INT8, 2,
                         "NeMo FastConformer CTC es"),
    "zipformer-th": Model("yfyeung/icefall-asr-gigaspeech2-th-zipformer-2024-06-20", "transducer",
                          _tr("exp/encoder-epoch-12-avg-5.int8.onnx", "exp/decoder-epoch-12-avg-5.onnx",
                              "exp/joiner-epoch-12-avg-5.int8.onnx", "data/lang_bpe_2000/tokens.txt"), 2, "Zipformer GigaSpeech2 th"),
    "zipformer-vi": Model("csukuangfj/sherpa-onnx-zipformer-vi-int8-2025-04-20", "transducer",
                          _tr("encoder-epoch-12-avg-8.int8.onnx", "decoder-epoch-12-avg-8.onnx",
                              "joiner-epoch-12-avg-8.int8.onnx"), 2, "Zipformer vi"),
    # ---- 新增语种候选（软件中尚无）----
    "moonshine-base-uk": Model("csukuangfj2/sherpa-onnx-moonshine-base-uk-quantized-2026-02-27", "moonshine_v2",
                               {"encoder": "encoder_model.ort", "decoder": "decoder_model_merged.ort", "tokens": "tokens.txt"},
                               2, "Moonshine base uk"),
    "zipformer-streaming-8lang": Model("csukuangfj/sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10",
                                       "online_transducer",
                                       _tr("encoder-epoch-75-avg-11-chunk-16-left-128.int8.onnx",
                                           "decoder-epoch-75-avg-11-chunk-16-left-128.onnx",
                                           "joiner-epoch-75-avg-11-chunk-16-left-128.int8.onnx"),
                                       2, "Streaming Zipformer 8-lang"),
    "shenava-rizeh-fa": Model("mah92/sherpa-onnx-nemo-ctc-fa-shenava-rizeh-v1.0-non-streaming-int8-2026-06-26", "nemo_ctc",
                              _CTC_INT8, 2, "Shenava Rizeh fa (32M)", in_app=False),
    "shenava-koochik-fa": Model("mah92/sherpa-onnx-nemo-ctc-fa-shenava-koochik-v1.0-non-streaming-int8-2026-06-26", "nemo_ctc",
                                _CTC_INT8, 2, "Shenava Koochik fa", in_app=False),
    "whisper-small": Model("csukuangfj/sherpa-onnx-whisper-small", "whisper", _w("small"), 2, "Whisper small"),
    # 软件的 "1600+ languages" 分组里已有
    "omnilingual-300m": Model("csukuangfj/sherpa-onnx-omnilingual-asr-1600-languages-300M-ctc-int8-2025-11-12",
                              "omnilingual", _CTC_INT8, 4, "Omnilingual 300M"),
    "omnilingual-1b": Model("csukuangfj/sherpa-onnx-omnilingual-asr-1600-languages-1B-ctc-int8-2025-11-12",
                            "omnilingual", _CTC_INT8, 4, "Omnilingual 1B"),
}


def create_recognizer(key: str, models_root):
    """按 ModelRegistry::GetConfig 的参数构建识别器（CPU）。返回 (recognizer, is_online)。"""
    import sherpa_onnx as so

    m = MODELS[key]
    d = models_root / m.dir
    f = {role: str(d / rel) for role, rel in m.files.items()}
    R, t = so.OfflineRecognizer, m.threads
    if m.arch == "paraformer":
        return R.from_paraformer(paraformer=f["model"], tokens=f["tokens"], num_threads=t), False
    if m.arch == "zipformer_ctc":
        return R.from_zipformer_ctc(model=f["model"], tokens=f["tokens"], num_threads=t), False
    if m.arch == "nemo_ctc":
        return R.from_nemo_ctc(model=f["model"], tokens=f["tokens"], num_threads=t), False
    if m.arch == "omnilingual":
        return R.from_omnilingual_asr_ctc(model=f["model"], tokens=f["tokens"], num_threads=t), False
    if m.arch == "wenet_ctc":
        return R.from_wenet_ctc(model=f["model"], tokens=f["tokens"], num_threads=t), False
    if m.arch == "sense_voice":
        # SenseVoiceFiles 默认 language="auto"、use_itn=true
        return R.from_sense_voice(model=f["model"], tokens=f["tokens"], language="auto", use_itn=True, num_threads=t), False
    if m.arch == "transducer":
        # 软件不设 model_type，由模型元数据自动识别（icefall / NeMo 都走这里）
        return R.from_transducer(encoder=f["encoder"], decoder=f["decoder"], joiner=f["joiner"], tokens=f["tokens"],
                                 num_threads=t, model_type=""), False
    if m.arch == "online_transducer":
        return so.OnlineRecognizer.from_transducer(tokens=f["tokens"], encoder=f["encoder"], decoder=f["decoder"],
                                                   joiner=f["joiner"], num_threads=t), True
    if m.arch == "whisper":
        # C++ 的 OfflineWhisperModelConfig.language 默认空串（自动识别语言），
        # Python 包装默认 "en"，这里必须显式传空串才与软件一致
        return R.from_whisper(encoder=f["encoder"], decoder=f["decoder"], tokens=f["tokens"], language="",
                              num_threads=t), False
    if m.arch == "moonshine":
        return R.from_moonshine(preprocessor=f["preprocessor"], encoder=f["encoder"],
                                uncached_decoder=f["uncached_decoder"], cached_decoder=f["cached_decoder"],
                                tokens=f["tokens"], num_threads=t), False
    if m.arch == "moonshine_v2":
        return R.from_moonshine_v2(encoder=f["encoder"], decoder=f["decoder"], tokens=f["tokens"], num_threads=t), False
    if m.arch == "fire_red":
        return R.from_fire_red_asr(encoder=f["encoder"], decoder=f["decoder"], tokens=f["tokens"], num_threads=t), False
    if m.arch == "qwen3":
        return R.from_qwen3_asr(conv_frontend=f["conv_frontend"], encoder=f["encoder"], decoder=f["decoder"],
                                tokenizer=f["tokenizer"], num_threads=t), False
    if m.arch == "funasr_nano":
        # user_prompt 用 C++ 默认值（全角冒号），Python 包装的默认值是半角
        return R.from_funasr_nano(encoder_adaptor=f["encoder_adaptor"], llm=f["llm"], embedding=f["embedding"],
                                  tokenizer=str(d / "Qwen3-0.6B"), user_prompt="语音转写：", num_threads=t), False
    raise KeyError(m.arch)
