#pragma once
#include <QDialog>
#include <QPoint>
#include "uiprofile.h"

class QSlider;
class QLabel;
class QCheckBox;
class ModernToolbar;
class QGroupBox;
class QPushButton;
class QButtonGroup; // Neu

class ProfileEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProfileEditorDialog(UiProfile profile, QWidget *parent = nullptr);
    UiProfile getProfile() const { return m_profile; }

signals:
    void previewRequested(UiProfile p);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onValuesChanged();      // Für die komplexen Slider
    void onSimpleStepClicked(int id); // Neu: Für die 5 Buttons
    void toggleMode();           // Umschalten

private:
    UiProfile m_profile;

    bool m_isSimpleMode{true};
    QPushButton *m_btnToggleMode;

    QWidget *m_simpleContainer;
    QWidget *m_complexContainer;

    // --- Elemente Einfacher Modus (Neu) ---
    QButtonGroup *m_btnGroupSimple; // Verwaltet die 5 Buttons

    // --- Elemente Komplexer Modus ---
    QSlider *m_slIconSize;
    QSlider *m_slButtonSize;
    QSlider *m_slGridSpace;
    QSlider *m_slToolbarScale;
    QCheckBox *m_chkSnap;

    QLabel *m_lblIconVal;
    QLabel *m_lblBtnVal;
    QLabel *m_lblGridVal;
    QLabel *m_lblToolbarVal;

    ModernToolbar* m_previewToolbar;
    QGroupBox* m_previewBox;

    QPoint m_dragPos;

    // Faktoren für die 5 Schritte: XS, S, M, L, XL
    const QVector<double> m_stepFactors = {0.75, 0.85, 1.0, 1.25, 1.5};

    void setupUi();
    void updateSimpleFromComplex();
};
