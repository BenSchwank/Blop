#pragma once
#include <QWidget>
#include <QIcon>
#include <QVector>
#include <QColor>
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
public:
    enum Style { Vertical, Radial };
    enum RadialType { FullCircle, HalfEdge };

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

    // NEU: Grenze oben setzen (damit Toolbar nicht über Tabs ragt)
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

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    Style m_style{Vertical};
    RadialType m_radialType{FullCircle};
    ToolMode mode_{ToolMode::Pen};
    ToolConfig m_config;
    SettingsState m_settingsState{SettingsState::Closed};

    QPoint dragStartPos_;
    bool m_isDragging{false};
    bool m_isResizing{false};
    bool m_draggable{true};

    bool m_isDockedLeft{true};
    double m_scrollAngle{0.0};

    qreal m_scale{1.0};
    int m_topBound{0}; // Speichert die Grenze oben

    ToolbarBtn* btnPen;
    ToolbarBtn* btnEraser;
    ToolbarBtn* btnLasso;
    ToolbarBtn* btnUndo;
    ToolbarBtn* btnRedo;

    QVector<ToolbarBtn*> m_buttons;
    QList<QColor> m_customColors;

    void updateLayout();
    void snapToEdge();

    void paintRadialRing1(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle);
    void paintRadialRing2(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle);
    void handleRadialSettingsClick(const QPoint& pos, int cx, int cy, int innerR, int outerR);
    void showVerticalPopup();
};
