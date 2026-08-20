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
class QShowEvent;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Special return code when user wants to open the editor
    static const int EditProfileCode = QDialog::Accepted + 1;

    explicit SettingsDialog(UiProfileManager* profileMgr, QWidget *parent = nullptr);
    ~SettingsDialog();

    void setToolbarConfig(bool isRadial, bool isHalf);
    void embedInWorkspace();

    // Helper so MainWindow knows which profile to edit
    QString profileIdToEdit() const { return m_editId; }

signals:
    /// Emitted when a profile should be edited without closing an embedded
    /// settings workspace tab.
    void profileEditRequested(const QString &profileId);
    void accentColorChanged(QColor color);
    void toolbarStyleChanged(bool radial);
    void logoutRequested();
    /// Emitted when Speicher-Modus or linked cloud folders change.
    void storagePrefsChanged();

    void previewProfileRequested(const UiProfile& p);
    /// Guest Konto actions — MainWindow opens Study login / Google OAuth.
    void studyLoginRequested();
    void studyRegisterRequested();
    void googleLoginRequested();
    /// Open the in-app cloud browser (Drive / Nextcloud / … / custom URL).
    void cloudExplorerRequested(const QString &id, const QString &type,
                                const QString &name, const QString &webUrl);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onProfileContextMenu(const QPoint &pos);
    void onCreateProfile();
    void onProfileClicked(QListWidgetItem* item);

private:
    bool m_dialogIntroDone{false};
    Ui::SettingsDialog *ui;
    UiProfileManager *m_profileManager;
    QListWidget *m_profileList;
    QString m_editId; // Stored ID for editor

    void refreshProfileList();
    void openEditor(const QString &profileId);
    void refreshTheme();
};

#endif // SETTINGSDIALOG_H
//for
