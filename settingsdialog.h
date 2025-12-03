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

signals:
    void accentColorChanged(QColor color);
    void uiModeChanged(bool touchMode);
    void itemSizeChanged(int value);
    void gridSpacingChanged(int value);

protected:
    // FEHLERBEHEBUNG: Diese Zeile hat gefehlt
    void showEvent(QShowEvent *event) override;

private slots:
    void onColorClicked();

private:
    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
