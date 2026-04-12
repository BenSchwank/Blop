#include "MathInkRecognizer.h"
#include "LatexToBlopConverter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>

#ifdef BLOP_HAS_ONNX_OCR
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#endif

// ============================================================================
// Impl
// ============================================================================

struct MathInkRecognizer::Impl {
#ifdef BLOP_HAS_ONNX_OCR
    // --- ONNX Runtime objects ---
    std::unique_ptr<Ort::Env>            env;
    std::unique_ptr<Ort::Session>        encoderSession;
    std::unique_ptr<Ort::Session>        decoderSession;
    Ort::AllocatorWithDefaultOptions     allocator;
    Ort::MemoryInfo                      memInfo = Ort::MemoryInfo::CreateCpu(
                                                     OrtArenaAllocator, OrtMemTypeDefault);

    // --- Token vocabulary ---
    QVector<QString>                     id2token;    // index -> LaTeX token string
    QHash<QString, int64_t>              token2id;    // token string -> index
    int64_t                              bosId = -1;  // <sos> / <s> / [BOS]
    int64_t                              eosId = -1;  // <eos> / </s> / [EOS]
    int64_t                              padId = -1;  // [PAD]

    // --- Model metadata ---
    int     imgChannels = 1;     // grayscale by default
    int     imgHeight   = 128;   // max input H (aspect-ratio preserved, padded)
    int     imgWidth    = 512;   // max input W
    int     maxDecLen   = 256;   // maximum decoder steps
    float   normMean    = 0.5f;  // pixel normalization mean
    float   normStd     = 0.5f;  // pixel normalization std
    bool    needsTgtMask = true; // decoder expects causal mask input (BTTR)
#endif

    bool    loaded      = false;
    bool    loadAttempted = false;
    QString modelDir;
    mutable QMutex mutex;

    // --- helpers ---
    bool tryLoad();

#ifdef BLOP_HAS_ONNX_OCR
    /// Pre-process QImage -> float tensor in NCHW format.
    /// Aspect-ratio-preserving resize + white padding.
    std::vector<float> preprocessImage(const QImage &img) const;

    /// Run encoder on image tensor, return encoder hidden states.
    Ort::Value runEncoder(const std::vector<float> &imgTensor) const;

    /// Greedy autoregressive decode (L2R, with optional causal mask).
    QVector<int64_t> greedyDecode(Ort::Value &encoderOut) const;

    /// Convert token IDs to a LaTeX string.
    QString detokenize(const QVector<int64_t> &ids) const;
#endif
};

// ============================================================================
// Singleton
// ============================================================================

MathInkRecognizer &MathInkRecognizer::instance()
{
    static MathInkRecognizer s;
    return s;
}

MathInkRecognizer::MathInkRecognizer()  : d(new Impl) {}
MathInkRecognizer::~MathInkRecognizer() { delete d; }

bool MathInkRecognizer::isAvailable() const
{
    QMutexLocker lk(&d->mutex);
    if (!d->loadAttempted)
        const_cast<Impl *>(d)->tryLoad();
    return d->loaded;
}

void MathInkRecognizer::setModelDirectory(const QString &dir)
{
    QMutexLocker lk(&d->mutex);
    d->modelDir     = dir;
    d->loaded       = false;
    d->loadAttempted = false;
}

// ============================================================================
// recognize
// ============================================================================

QString MathInkRecognizer::recognize(const QImage &inkImage) const
{
#ifdef BLOP_HAS_ONNX_OCR
    if (!isAvailable())
        return {};
    if (inkImage.isNull())
        return {};

    try {
        QMutexLocker lk(&d->mutex);

        // 1. pre-process
        std::vector<float> tensor = d->preprocessImage(inkImage);

        // 2. encoder
        Ort::Value encOut = d->runEncoder(tensor);

        // 3. decoder (greedy)
        QVector<int64_t> tokenIds = d->greedyDecode(encOut);

        // 4. detokenize -> LaTeX
        QString latex = d->detokenize(tokenIds);
        if (latex.isEmpty())
            return {};

        // 5. LaTeX -> Blop syntax
        QString blop = LatexToBlopConverter::convert(latex);
        qDebug() << "[MathInkRecognizer] LaTeX:" << latex << " -> Blop:" << blop;
        return blop;

    } catch (const Ort::Exception &e) {
        qWarning() << "[MathInkRecognizer] ONNX error:" << e.what();
        return {};
    } catch (const std::exception &e) {
        qWarning() << "[MathInkRecognizer] error:" << e.what();
        return {};
    }
#else
    Q_UNUSED(inkImage);
    return {};
#endif
}

// ============================================================================
// tryLoad
// ============================================================================

bool MathInkRecognizer::Impl::tryLoad()
{
    loadAttempted = true;
    loaded        = false;

#ifdef BLOP_HAS_ONNX_OCR
    // --- Resolve model directory ---
    QString dir = modelDir;
    if (dir.isEmpty()) {
        dir = QCoreApplication::applicationDirPath()
              + QStringLiteral("/assets/models");
    }
    const QDir d(dir);
    const QString encPath   = d.filePath(QStringLiteral("encoder.onnx"));
    const QString decPath   = d.filePath(QStringLiteral("decoder.onnx"));
    const QString tokPath   = d.filePath(QStringLiteral("tokens.json"));

    if (!QFile::exists(encPath) || !QFile::exists(decPath) || !QFile::exists(tokPath)) {
        qInfo() << "[MathInkRecognizer] Model files not found in" << dir
                << "-- offline recognition disabled.";
        return false;
    }

    // --- Load token vocabulary ---
    {
        QFile f(tokPath);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "[MathInkRecognizer] Cannot open" << tokPath;
            return false;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) {
            qWarning() << "[MathInkRecognizer] tokens.json is not a JSON object";
            return false;
        }
        const QJsonObject obj = doc.object();

        // Read "tokens" array: [ "token0", "token1", ... ]
        const QJsonArray arr = obj.value(QStringLiteral("tokens")).toArray();
        if (arr.isEmpty()) {
            qWarning() << "[MathInkRecognizer] tokens.json has no tokens array";
            return false;
        }
        id2token.resize(arr.size());
        for (int i = 0; i < arr.size(); ++i) {
            id2token[i] = arr.at(i).toString();
            token2id.insert(id2token[i], i);
        }

        // Special token IDs (explicit from JSON)
        bosId = obj.value(QStringLiteral("bos_id")).toInteger(-1);
        eosId = obj.value(QStringLiteral("eos_id")).toInteger(-1);
        padId = obj.value(QStringLiteral("pad_id")).toInteger(0);

        // Fallback: try to find special tokens by name
        if (bosId < 0) {
            for (const auto &name : {QStringLiteral("<s>"), QStringLiteral("[BOS]"),
                                     QStringLiteral("<bos>"), QStringLiteral("<sos>")}) {
                if (token2id.contains(name)) { bosId = token2id.value(name); break; }
            }
        }
        if (eosId < 0) {
            for (const auto &name : {QStringLiteral("</s>"), QStringLiteral("[EOS]"),
                                     QStringLiteral("<eos>")}) {
                if (token2id.contains(name)) { eosId = token2id.value(name); break; }
            }
        }
        if (bosId < 0) bosId = 1;
        if (eosId < 0) eosId = 2;

        // Model metadata
        imgChannels = obj.value(QStringLiteral("img_channels")).toInt(1);
        imgHeight   = obj.value(QStringLiteral("img_height")).toInt(128);
        imgWidth    = obj.value(QStringLiteral("img_width")).toInt(512);
        maxDecLen   = obj.value(QStringLiteral("max_seq_len")).toInt(256);
        needsTgtMask = obj.value(QStringLiteral("needs_tgt_mask")).toBool(false);

        // Normalization constants (configurable per model)
        const QJsonObject normObj = obj.value(QStringLiteral("normalization")).toObject();
        normMean = static_cast<float>(normObj.value(QStringLiteral("mean")).toDouble(0.5));
        normStd  = static_cast<float>(normObj.value(QStringLiteral("std")).toDouble(0.5));

        const QString modelName = obj.value(QStringLiteral("model_name")).toString(
            QStringLiteral("unknown"));
        qInfo() << "[MathInkRecognizer] Model:" << modelName
                << "| Loaded" << id2token.size() << "tokens"
                << "| BOS=" << bosId << "EOS=" << eosId
                << "| img=" << imgChannels << "x" << imgHeight << "x" << imgWidth
                << "| norm=" << normMean << "/" << normStd
                << "| tgtMask=" << needsTgtMask;
    }

    // --- Create ONNX environment + sessions ---
    try {
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "MathInkOCR");

        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Convert paths to wide strings for ONNX on Windows
#ifdef _WIN32
        const std::wstring encW = encPath.toStdWString();
        const std::wstring decW = decPath.toStdWString();
        encoderSession = std::make_unique<Ort::Session>(*env, encW.c_str(), opts);
        decoderSession = std::make_unique<Ort::Session>(*env, decW.c_str(), opts);
#else
        const std::string encS = encPath.toStdString();
        const std::string decS = decPath.toStdString();
        encoderSession = std::make_unique<Ort::Session>(*env, encS.c_str(), opts);
        decoderSession = std::make_unique<Ort::Session>(*env, decS.c_str(), opts);
#endif

        loaded = true;
        qInfo() << "[MathInkRecognizer] Encoder + Decoder sessions loaded successfully.";
        return true;

    } catch (const Ort::Exception &e) {
        qWarning() << "[MathInkRecognizer] Failed to load ONNX models:" << e.what();
        encoderSession.reset();
        decoderSession.reset();
        env.reset();
        return false;
    }

#else
    qInfo() << "[MathInkRecognizer] Built without BLOP_HAS_ONNX_OCR -- offline recognition disabled.";
    return false;
#endif
}

// ============================================================================
// ONNX helpers (only when BLOP_HAS_ONNX_OCR is defined)
// ============================================================================

#ifdef BLOP_HAS_ONNX_OCR

// ---------------------------------------------------------------------------
// preprocessImage
// ---------------------------------------------------------------------------

std::vector<float> MathInkRecognizer::Impl::preprocessImage(const QImage &img) const
{
    // 1. Convert to grayscale
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);

    // 2. Aspect-ratio-preserving resize to fit within imgHeight x imgWidth
    gray = gray.scaled(imgWidth, imgHeight, Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);

    // 3. Pad to full imgHeight x imgWidth with white background
    QImage padded(imgWidth, imgHeight, QImage::Format_Grayscale8);
    padded.fill(255);  // white
    {
        QPainter painter(&padded);
        painter.drawImage(0, 0, gray);  // top-left aligned
    }

    // 4. Build NCHW tensor with normalization
    const int H = imgHeight;
    const int W = imgWidth;
    std::vector<float> tensor(static_cast<size_t>(1 * 1 * H * W));

    for (int y = 0; y < H; ++y) {
        const uchar *row = padded.constScanLine(y);
        for (int x = 0; x < W; ++x) {
            const float pixel = static_cast<float>(row[x]) / 255.0f;
            const float normalized = (pixel - normMean) / normStd;
            tensor[static_cast<size_t>(y * W + x)] = normalized;
        }
    }

    return tensor;
}

// ---------------------------------------------------------------------------
// runEncoder
// ---------------------------------------------------------------------------

Ort::Value MathInkRecognizer::Impl::runEncoder(const std::vector<float> &imgTensor) const
{
    // Input shape: [1, C, H, W]
    const std::array<int64_t, 4> inputShape = {1, imgChannels, imgHeight, imgWidth};

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo,
        const_cast<float *>(imgTensor.data()),
        imgTensor.size(),
        inputShape.data(),
        inputShape.size());

    // Query input/output names from the model
    auto inputNameAlloc = encoderSession->GetInputNameAllocated(0, allocator);
    auto outputNameAlloc = encoderSession->GetOutputNameAllocated(0, allocator);
    const char *inputNames[]  = {inputNameAlloc.get()};
    const char *outputNames[] = {outputNameAlloc.get()};

    // Run encoder
    auto results = encoderSession->Run(
        Ort::RunOptions{nullptr},
        inputNames,  &inputTensor, 1,
        outputNames, 1);

    if (results.empty() || !results[0].IsTensor()) {
        throw std::runtime_error("Encoder returned no valid tensor");
    }

    return std::move(results[0]);
}

// ---------------------------------------------------------------------------
// greedyDecode
// ---------------------------------------------------------------------------

QVector<int64_t> MathInkRecognizer::Impl::greedyDecode(Ort::Value &encoderOut) const
{
    QVector<int64_t> generatedIds;
    generatedIds.reserve(maxDecLen);
    generatedIds.append(bosId);

    // Get decoder input/output names
    const size_t numInputs  = decoderSession->GetInputCount();
    const size_t numOutputs = decoderSession->GetOutputCount();

    std::vector<Ort::AllocatedStringPtr> inputNamePtrs;
    std::vector<Ort::AllocatedStringPtr> outputNamePtrs;
    std::vector<const char *> inputNames;
    std::vector<const char *> outputNames;

    for (size_t i = 0; i < numInputs; ++i) {
        inputNamePtrs.push_back(decoderSession->GetInputNameAllocated(i, allocator));
        inputNames.push_back(inputNamePtrs.back().get());
    }
    for (size_t i = 0; i < numOutputs; ++i) {
        outputNamePtrs.push_back(decoderSession->GetOutputNameAllocated(i, allocator));
        outputNames.push_back(outputNamePtrs.back().get());
    }

    for (int step = 0; step < maxDecLen; ++step) {
        const int64_t seqLen = generatedIds.size();

        // Build input_ids tensor  [1, seqLen]
        const std::array<int64_t, 2> idsShape = {1, seqLen};
        Ort::Value idsTensor = Ort::Value::CreateTensor<int64_t>(
            memInfo,
            generatedIds.data(),
            static_cast<size_t>(seqLen),
            idsShape.data(),
            idsShape.size());

        // Detect input order from ONNX model and build inputs accordingly.
        // Also add causal mask for BTTR-style models.
        std::vector<Ort::Value> inputs;
        const bool idsFirst = (numInputs >= 2 && strcmp(inputNames[0], "input_ids") == 0);
        if (idsFirst) {
            inputs.push_back(std::move(idsTensor));
            inputs.push_back(std::move(encoderOut));
        } else {
            inputs.push_back(std::move(encoderOut));
            inputs.push_back(std::move(idsTensor));
        }

        // Build causal mask [seqLen, seqLen] if the model expects it (BTTR)
        std::vector<float> maskData;
        if (needsTgtMask && numInputs >= 3) {
            maskData.resize(static_cast<size_t>(seqLen * seqLen), 0.0f);
            for (int64_t i = 0; i < seqLen; ++i) {
                for (int64_t j = i + 1; j < seqLen; ++j) {
                    maskData[static_cast<size_t>(i * seqLen + j)] = -1e9f;
                }
            }
            const std::array<int64_t, 2> maskShape = {seqLen, seqLen};
            Ort::Value maskTensor = Ort::Value::CreateTensor<float>(
                memInfo,
                maskData.data(),
                maskData.size(),
                maskShape.data(),
                maskShape.size());
            inputs.push_back(std::move(maskTensor));
        }

        auto results = decoderSession->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(), inputs.data(), inputs.size(),
            outputNames.data(), numOutputs);

        // Get encoder output back for next iteration (moved into inputs)
        if (numInputs == 2 && strcmp(inputNames[0], "input_ids") == 0) {
            encoderOut = std::move(inputs[1]);
        } else {
            encoderOut = std::move(inputs[0]);
        }

        if (results.empty() || !results[0].IsTensor())
            break;

        // Output logits shape: [1, seqLen, vocab_size]
        auto shape = results[0].GetTensorTypeAndShapeInfo().GetShape();
        const int64_t vocabSize = shape.back();
        const float *logits = results[0].GetTensorData<float>();

        // We only care about the last position's logits
        const float *lastLogits = logits + (seqLen - 1) * vocabSize;

        // Greedy: argmax
        int64_t bestId = 0;
        float   bestVal = lastLogits[0];
        for (int64_t v = 1; v < vocabSize; ++v) {
            if (lastLogits[v] > bestVal) {
                bestVal = lastLogits[v];
                bestId  = v;
            }
        }

        if (bestId == eosId || bestId == padId)
            break;

        generatedIds.append(bestId);
    }

    return generatedIds;
}

// ---------------------------------------------------------------------------
// detokenize
// ---------------------------------------------------------------------------

QString MathInkRecognizer::Impl::detokenize(const QVector<int64_t> &ids) const
{
    QStringList parts;
    parts.reserve(ids.size());

    for (const int64_t id : ids) {
        // Skip special tokens
        if (id == bosId || id == eosId || id == padId)
            continue;
        if (id < 0 || id >= id2token.size())
            continue;

        const QString &tok = id2token[static_cast<int>(id)];
        // Skip special-looking tokens
        if (tok.startsWith(QLatin1Char('<')) && tok.endsWith(QLatin1Char('>')))
            continue;
        if (tok.startsWith(QLatin1String("[")) && tok.endsWith(QLatin1String("]")))
            continue;

        parts.append(tok);
    }

    // Tokens are individual LaTeX tokens/characters.
    // Join with spaces; the LaTeX->Blop converter handles spacing.
    return parts.join(QLatin1Char(' '));
}

#endif // BLOP_HAS_ONNX_OCR
