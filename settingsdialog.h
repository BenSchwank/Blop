#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QColor>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void setTouchMode(bool enabled);
    void setGridValues(int itemSize, int spacing);

    // Status setzen
    void setToolbarConfig(bool isRadial, bool isHalf, int scalePercent);

signals:
    void accentColorChanged(QColor color);
    void uiModeChanged(bool touchMode);
    void itemSizeChanged(int value);
    void gridSpacingChanged(int value);

    void toolbarStyleChanged(bool radial);

    // NEU: Skalierungssignal
    void toolbarScaleChanged(int percent);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onColorClicked();
    void onColorButtonContextMenu(const QPoint &pos);

private:
    Ui::SettingsDialog *ui;
    void setupColorButton(class QPushButton* btn, QColor initialColor);
};

#endif // SETTINGSDIALOG_H
