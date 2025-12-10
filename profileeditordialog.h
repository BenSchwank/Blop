#pragma once
#include <QDialog>
#include <QPoint>
#include "UiProfile.h"

class QSlider;
class QLabel;
class QCheckBox;
class ModernToolbar;
class QGroupBox;

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
    void onValuesChanged();

private:
    UiProfile m_profile;
    QSlider *m_slIconSize;
    // Kein ItemHeight Slider mehr
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

    void setupUi();
};
