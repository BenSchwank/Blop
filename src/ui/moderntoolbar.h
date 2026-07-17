#pragma once
#include <QList>
#include <QPointer>
#include <QWidget>
#include <QIcon>
#include <QVector>
#include <QColor>
#include <QPropertyAnimation>
#include <QRegion>
#include <QTimer>
#include "ToolMode.h"
#include "ToolSettings.h"

// --- Helper Button Class ---
class QPushButton;
class QPainter;

// Draws one of the toolbar glyphs ("pen", "eraser", ...) into a 64x64 logical
// coordinate space. Shared by ToolbarBtn and BloomMenu petals.
void blopDrawToolbarGlyph64(QPainter *p, const QString &name,
                            const QColor &color);

class ToolbarBtn : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double animScale READ animScale WRITE setAnimScale)
    Q_PROPERTY(double liftOffset READ liftOffset WRITE setLiftOffset)
    // v3.17.4: holdProgress driven by a QPropertyAnimation instead of a
    // 16 ms QTimer. On Android the timer-driven update() loop competed
    // with the composer; the animation runs on Qt's animation clock and
    // is frame-aligned.
    Q_PROPERTY(double holdProgress READ holdProgress WRITE setHoldProgress)

public:
    explicit ToolbarBtn(const QString& iconName, QWidget* parent = nullptr);
    void setIcon(const QString& name);
    void setActive(bool active);
    void setBtnSize(int s);
    void setAccentColor(const QColor& c) { m_accentColor = c; update(); }
    QString iconName() const { return m_iconName; }

    void animateSelect();
    void setDrawFloatingBg(bool draw);

    double animScale() const { return m_animScale; }
    void setAnimScale(double s) { m_animScale = s; update(); }

    // v3.16.0: "active lift" — the active tool floats ~6dp above the pill
    // (Samsung-Notes style), driven by a QPropertyAnimation in setActive.
    double liftOffset() const { return m_liftOffset; }
    void setLiftOffset(double v) { m_liftOffset = v; update(); }

    double holdProgress() const { return m_holdProgress; }
    void setHoldProgress(double v);

    void triggerClick();

signals:
    void clicked();
    void longPressed();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QString m_iconName;
    bool m_active{false};
    bool m_hover{false};
    bool m_drawFloatingBg{false};
    int m_size{40};

    double m_animScale{1.0};
    bool m_pressing{false};
    bool m_longPressTriggered{false};
    double m_holdProgress{0.0};
    QColor m_accentColor{QColor("#7C5CFC")};
    double m_liftOffset{0.0};
    QPointer<QPropertyAnimation> m_liftAnim;
    QPointer<QPropertyAnimation> m_holdAnim;
};

// --- Main Toolbar Class ---
class ModernToolbar : public QWidget {
    Q_OBJECT

public:
    enum Style { Normal, Radial };
    enum RadialType { FullCircle, HalfEdge };
    enum Orientation { Vertical, Horizontal };

    explicit ModernToolbar(QWidget* parent=nullptr);

    void setToolMode(ToolMode mode);
    ToolMode toolMode() const { return mode_; }

    // Docking logic
    void setDockMode(bool docked);
    bool isDockedMode() const { return m_isDockedMode; }

    void setStyle(Style style);
    Style currentStyle() const { return m_style; }

    void setOrientation(Orientation o, bool animate = false);
    Orientation orientation() const { return m_orientation; }

    void setRadialType(RadialType type);
    RadialType radialType() const { return m_radialType; }

    void setDraggable(bool enable) { m_draggable = enable; }
    void setPreviewMode(bool enable) { m_isPreview = enable; }

    void setScale(double scale);
    double scale() const { return m_scale; }
    void setAccentColor(const QColor& color);

    void constrainToParent();
    void setTopBound(int top);
    void requestAdaptiveReflow();
    int calculateMinLength();

signals:
    void toolChanged(ToolMode mode);
    void undoRequested();
    void redoRequested();
    void penConfigChanged(QColor c, int w);
    void scaleChanged(double newScale);
    void rulerToggled(bool active); // NEU: Globaler Toggle für das Lineal
    void backToOverviewRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void leaveEvent(QEvent*) override;
    void showEvent(QShowEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    Style m_style{Normal};
    RadialType m_radialType{FullCircle};
    Orientation m_orientation{Horizontal};

    ToolMode mode_{ToolMode::Pen};
    ToolConfig m_config;
    bool m_rulerActive{false}; // NEU: Lineal Toggle-State

    QPoint dragStartPos_;
    QPoint m_dragOffset;
    QPoint resizeStartPos_;
    QSize startSize_;

    bool m_isDragging{false};
    bool m_isResizing{false};
    bool m_draggable{true};
    bool m_isPreview{false};
    bool m_isDockedMode{false};

    bool m_isScrolling{false};
    bool m_hasScrolled{false};
    bool m_isDockedCenterDragging{false};
    double m_dragStartAngle{0.0};
    double m_scrollStartAngleVal{0.0};
    int m_dockedCenterDragLastX{0};
    int m_dockedCenterScrollPx{0};
    int m_dockedCenterOverflowPx{0};
    int m_dockedCenterAreaStartX{0};
    int m_dockedCenterAreaEndX{0};

    bool m_isDockedLeft{true};
    double m_scrollAngle{0.0};

    double m_scale{1.0};
    int m_topBound{0};

    QRegion m_cachedMask;

    ToolbarBtn* m_pressedButton{nullptr};

    // Buttons
    ToolbarBtn* btnPen;
    ToolbarBtn* btnPencil;
    ToolbarBtn* btnHighlighter;
    ToolbarBtn* btnEraser;
    ToolbarBtn* btnLasso;
    ToolbarBtn* btnRuler;
    ToolbarBtn* btnShape;
    ToolbarBtn* btnStickyNote;
    ToolbarBtn* btnText;
    ToolbarBtn* btnImage;
    ToolbarBtn* btnHand;

    ToolbarBtn* btnUndo;
    ToolbarBtn* btnRedo;
    ToolbarBtn *btnBackOverview{nullptr};

    ToolbarBtn* btnDockToggle; // New button for detach/dock

    // Extra features for docked mode (Seite/Notiz-Optionen nur über Notiz-⋯-Menü)
    ToolbarBtn *btnSave;
    ToolbarBtn *btnPalette;
    ToolbarBtn *btnBrushSize;
    QList<ToolbarBtn*> m_dockedOnlyButtons;

    QVector<ToolbarBtn*> m_buttons;
    QList<QColor> m_customColors;

    void updateLayout(bool animate = false);
    bool supportsAdaptiveDockedScroll() const;
    int effectiveButtonSize(int w, int h) const;
    int effectiveGap() const;
    QList<ToolbarBtn*> leftChromeButtons() const;
    void snapToEdge();
    void checkOrientation(const QPoint& globalPos);
    void reorderButtons();
    ToolbarBtn* getButtonForMode(ToolMode m);
    ToolbarBtn* getRadialButtonAt(const QPoint& pos);

    void updateHitbox();

    // Neues Menü
    void showSettingsPopup();
    
    void toggleRadialSettings();
    bool m_showRadialSettings{false};
    QList<QPushButton*> m_radialSettingsBtns;
    QColor m_accentColor{QColor("#7C5CFC")};

    QWidget* m_snapPreview{nullptr};
    QList<int> m_separatorXPositions;  // x-coords of section dividers in horizontal mode

    QPointer<QWidget> m_toolSettingsSheet;

    // v3.16.0 morph pieces:
    // m_activeIndicator: 28dp x 3dp accent-Lila pill that slides under the
    //   currently active tool button (Material 3-style sliding indicator).
    //   Hidden in Radial style.
    // m_activeIndicatorAnim: position animation driving it on tool changes.
    QPointer<QWidget> m_activeIndicator;
    QPointer<QPropertyAnimation> m_activeIndicatorAnim;
    void updateActiveIndicator(bool animate = true);

    // Helper that performs the Q_PROPERTY(liftOffset) animation on the
    // active button (and resets every other button to 0).
    void applyActiveLift(ToolbarBtn *target);

    QTimer m_radialCollapseTimer;
    bool m_radialColdFan{false};
};
