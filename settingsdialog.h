#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QColor>
#include "uiprofilemanager.h"

namespace Ui {
class SettingsDialog;
}

class QListWidget;
class QListWidgetItem;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Special return code when user wants to open the editor
    static const int EditProfileCode = QDialog::Accepted + 1;

    explicit SettingsDialog(UiProfileManager* profileMgr, QWidget *parent = nullptr);
    ~SettingsDialog();

    void setToolbarConfig(bool isRadial, bool isHalf);

    // Helper so MainWindow knows which profile to edit
    QString profileIdToEdit() const { return m_editId; }

signals:
    void accentColorChanged(QColor color);
    void toolbarStyleChanged(bool radial);

    void previewProfileRequested(const UiProfile& p);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onProfileContextMenu(const QPoint &pos);
    void onCreateProfile();
    void onProfileClicked(QListWidgetItem* item);

private:
    Ui::SettingsDialog *ui;
    UiProfileManager *m_profileManager;
    QListWidget *m_profileList;
    QString m_editId; // Stored ID for editor

    void refreshProfileList();
    void openEditor(const QString &profileId);
};

#endif // SETTINGSDIALOG_H
//for
