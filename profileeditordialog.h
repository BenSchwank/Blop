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
class QButtonGroup; // New

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
    void onValuesChanged();      // For the complex sliders
    void onSimpleStepClicked(int id); // New: For the 5 buttons
    void toggleMode();           // Toggle

private:
    UiProfile m_profile;

    bool m_isSimpleMode{true};
    QPushButton *m_btnToggleMode;

    QWidget *m_simpleContainer;
    QWidget *m_complexContainer;

    // --- Elements Simple Mode (New) ---
    QButtonGroup *m_btnGroupSimple; // Manages the 5 buttons

    // --- Elements Complex Mode ---
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

    // Factors for the 5 steps: XS, S, M, L, XL
    const QVector<double> m_stepFactors = {0.75, 0.85, 1.0, 1.25, 1.5};

    void setupUi();
    void updateSimpleFromComplex();
};
