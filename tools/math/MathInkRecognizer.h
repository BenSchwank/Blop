#pragma once

#include <QImage>
#include <QString>

/// Offline handwritten math expression recognizer using ONNX Runtime.
///
/// Uses a pix2tex-style encoder–decoder architecture:
///   1. Encoder: image → feature tensor
///   2. Decoder: autoregressive token generation (greedy)
///   3. Token de-mapping → LaTeX string
///   4. LaTeX → Blop syntax via LatexToBlopConverter
///
/// The recognizer is a process-wide singleton.  Model files are loaded lazily
/// on first call to recognize().  If the model files are missing the
/// recognizer gracefully degrades (isAvailable() == false).
///
/// Required model files (relative to QCoreApplication::applicationDirPath()):
///   assets/models/encoder.onnx
///   assets/models/decoder.onnx
///   assets/models/tokens.json
class MathInkRecognizer {
public:
    static MathInkRecognizer &instance();

    /// true when all three model files have been loaded successfully.
    bool isAvailable() const;

    /// Recognize handwritten math from an ink image (white bg, black strokes).
    /// Returns a Blop-syntax expression (e.g. "x^2+sin(x)") or an empty
    /// string on failure / low-confidence / missing model.
    QString recognize(const QImage &inkImage) const;

    /// Model file directory override (default = appDir/assets/models).
    void setModelDirectory(const QString &dir);

private:
    MathInkRecognizer();
    ~MathInkRecognizer();
    MathInkRecognizer(const MathInkRecognizer &) = delete;
    MathInkRecognizer &operator=(const MathInkRecognizer &) = delete;

    struct Impl;
    Impl *d;
};
