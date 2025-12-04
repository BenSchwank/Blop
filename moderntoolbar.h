#pragma once
#include <QWidget>
#include <QIcon>
#include <QVector>
#include <QColor>
#include <QPropertyAnimation>
#include "ToolMode.h"
#include "ToolSettings.h"

class ToolbarBtn : public QWidget {
    Q_OBJECT
public:
    explicit ToolbarBtn(const QString& iconName, QWidget* parent = nullptr);
    void setIcon(const QString& name);
    void setActive(bool active);
    void setBtnSize(int s);
    QString iconName() const { return m_iconName; }
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
};

class ModernToolbar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QSize size READ size WRITE resize) // Für Animation
public:
    enum Style { Normal, Radial }; // "Vertical" heißt jetzt "Normal"
    enum RadialType { FullCircle, HalfEdge };
    enum Orientation { Vertical, Horizontal }; // Neu für Adaptive Toolbar

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
    Orientation m_orientation{Vertical}; // Aktuelle Ausrichtung

    ToolMode mode_{ToolMode::Pen};
    ToolConfig m_config;
    SettingsState m_settingsState{SettingsState::Closed};

    QPoint dragStartPos_;
    QPoint resizeStartPos_;
    QSize startSize_;

    bool m_isDragging{false};
    bool m_isResizing{false};
    bool m_draggable{true};

    bool m_isDockedLeft{true};
    double m_scrollAngle{0.0};

    qreal m_scale{1.0};
    int m_topBound{0};

    ToolbarBtn* btnPen;
    ToolbarBtn* btnEraser;
    ToolbarBtn* btnLasso;
    ToolbarBtn* btnUndo;
    ToolbarBtn* btnRedo;

    QVector<ToolbarBtn*> m_buttons;
    QList<QColor> m_customColors;

    void updateLayout(bool animate = false);
    void snapToEdge();
    void checkOrientation(const QPoint& globalPos); // Prüft Drehung
    void setOrientation(Orientation o);

    void reorderButtons();
    ToolbarBtn* getButtonForMode(ToolMode m);

    void paintRadialRing1(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle);
    void paintRadialRing2(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle);
    void handleRadialSettingsClick(const QPoint& pos, int cx, int cy, int innerR, int outerR);
    void showVerticalPopup();

    int calculateMinLength(); // Min Höhe (oder Breite bei Horizontal)
};
