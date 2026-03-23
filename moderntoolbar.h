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
class QPushButton;

class ToolbarBtn : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double animScale READ animScale WRITE setAnimScale)

public:
    explicit ToolbarBtn(const QString& iconName, QWidget* parent = nullptr);
    void setIcon(const QString& name);
    void setActive(bool active);
    void setBtnSize(int s);
    QString iconName() const { return m_iconName; }

    void animateSelect();
    void setDrawFloatingBg(bool draw);

    double animScale() const { return m_animScale; }
    void setAnimScale(double s) { m_animScale = s; update(); }

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
    bool m_drawFloatingBg{false};
    int m_size{40};

    double m_animScale{1.0};
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

    void setRadialType(RadialType type);
    RadialType radialType() const { return m_radialType; }

    void setDraggable(bool enable) { m_draggable = enable; }
    void setPreviewMode(bool enable) { m_isPreview = enable; }

    void setScale(double scale);
    double scale() const { return m_scale; }

    void constrainToParent();
    void setTopBound(int top);
    int calculateMinLength();

signals:
    void toolChanged(ToolMode mode);
    void undoRequested();
    void redoRequested();
    void penConfigChanged(QColor c, int w);
    void scaleChanged(double newScale);
    void settingsRequested();
    void rulerToggled(bool active); // NEU: Globaler Toggle für das Lineal

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
    double m_dragStartAngle{0.0};
    double m_scrollStartAngleVal{0.0};

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

    ToolbarBtn* btnDockToggle; // New button for detach/dock

    // Extra features for docked mode
    ToolbarBtn *btnSettings;
    ToolbarBtn *btnSave;
    ToolbarBtn *btnPalette;
    ToolbarBtn *btnBrushSize;
    QList<ToolbarBtn*> m_dockedOnlyButtons;

    QVector<ToolbarBtn*> m_buttons;
    QList<QColor> m_customColors;

    void updateLayout(bool animate = false);
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

    QWidget* m_snapPreview{nullptr};
    QList<int> m_separatorXPositions;  // x-coords of section dividers in horizontal mode
};
