#pragma once
#include <QWidget>
#include <QIcon>
#include <QVector>
#include <QColor>
#include <QPropertyAnimation>
#include <QRegion>
#include "ToolMode.h"
#include "ToolSettings.h"

// --- Helper Button Class ---
class ToolbarBtn : public QWidget {
    Q_OBJECT
    // FIX: float -> qreal
    Q_PROPERTY(qreal pulseScale READ pulseScale WRITE setPulseScale)

public:
    explicit ToolbarBtn(const QString& iconName, QWidget* parent = nullptr);
    void setIcon(const QString& name);
    void setActive(bool active);
    void setBtnSize(int s);
    QString iconName() const { return m_iconName; }

    void animateSelect();

    qreal pulseScale() const { return m_pulseScale; }
    void setPulseScale(qreal s) { m_pulseScale = s; update(); }

    void triggerClick() { animateSelect(); emit clicked(); }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QString m_iconName;
    bool m_active{false};
    bool m_hover{false};
    int m_size{40};

    // FIX: float -> qreal
    qreal m_pulseScale{1.0};
};

// --- Main Toolbar Class ---
class ModernToolbar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QSize size READ size WRITE resize)

public:
    enum Style { Normal, Radial };
    enum RadialType { FullCircle, HalfEdge };
    enum Orientation { Vertical, Horizontal };

    enum class SettingsState {
        Closed, Main, ColorSelect, SizeSelect, EraserMode, LassoMode
    };

    explicit ModernToolbar(QWidget* parent=nullptr);

    void setToolMode(ToolMode mode);
    ToolMode toolMode() const { return mode_; }

    void setStyle(Style style);
    Style currentStyle() const { return m_style; }

    void setRadialType(RadialType type);
    RadialType radialType() const { return m_radialType; }

    void setDraggable(bool enable) { m_draggable = enable; }
    void setPreviewMode(bool enable) { m_isPreview = enable; }

    void setScale(qreal scale);
    qreal scale() const { return m_scale; }

    void constrainToParent();
    void setTopBound(int top);

signals:
    void toolChanged(ToolMode mode);
    void undoRequested();
    void redoRequested();
    void penConfigChanged(QColor c, int w);
    void eraserConfigChanged(EraserMode m);
    void lassoConfigChanged(LassoMode m);
    void scaleChanged(qreal newScale);

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
    Orientation m_orientation{Vertical};

    ToolMode mode_{ToolMode::Pen};
    ToolConfig m_config;
    SettingsState m_settingsState{SettingsState::Closed};

    QPoint dragStartPos_;
    QPoint m_dragOffset;
    QPoint resizeStartPos_;
    QSize startSize_;

    bool m_isDragging{false};
    bool m_isResizing{false};
    bool m_draggable{true};
    bool m_isPreview{false};

    bool m_isScrolling{false};
    bool m_hasScrolled{false};
    double m_dragStartAngle{0.0};
    double m_scrollStartAngleVal{0.0};

    bool m_isDockedLeft{true};
    double m_scrollAngle{0.0};

    qreal m_scale{1.0};
    int m_topBound{0};

    QRegion m_cachedMask;

    ToolbarBtn* m_pressedButton{nullptr};

    // Buttons
    ToolbarBtn* btnPen;
    ToolbarBtn* btnEraser;
    ToolbarBtn* btnHighlighter; // NEW
    ToolbarBtn* btnLasso;
    ToolbarBtn* btnUndo;
    ToolbarBtn* btnRedo;

    QVector<ToolbarBtn*> m_buttons;
    QList<QColor> m_customColors;

    void updateLayout(bool animate = false);
    void snapToEdge();
    void checkOrientation(const QPoint& globalPos);
    void setOrientation(Orientation o, bool animate);
    void reorderButtons();
    ToolbarBtn* getButtonForMode(ToolMode m);
    ToolbarBtn* getRadialButtonAt(const QPoint& pos);

    void paintRadialRing1(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle);
    void paintRadialRing2(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle);
    void handleRadialSettingsClick(const QPoint& pos, int cx, int cy, int innerR, int outerR);
    void showVerticalPopup();

    void updateHitbox();
    int calculateMinLength();
};
