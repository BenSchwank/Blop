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
    /// Prepare dialog as an embedded child (modal card or workspace tab).
    /// When asWorkspaceTab is true, profile edit emits profileEditRequested
    /// instead of closing with EditProfileCode.
    void embedInWorkspace(bool asWorkspaceTab = true);

    /// App-shell mode: hide internal left nav (MainWindow owns sidebar nav).
    void setAppShellMode(bool on);
    bool appShellMode() const { return m_appShellMode; }
    QStringList sectionTitles() const { return m_sectionTitles; }
    void setSectionIndex(int index);
    int sectionIndex() const;
    /// Drop stuck opacity effects so reopen never shows a blank white page.
    void clearSectionPageEffects();

    // Helper so MainWindow knows which profile to edit
    QString profileIdToEdit() const { return m_editId; }

signals:
    /// Emitted when a profile should be edited without closing an embedded
    /// settings workspace tab.
    void profileEditRequested(const QString &profileId);
    void accentColorChanged(QColor color);
    void toolbarStyleChanged(bool radial);
    /// Desktop studio layout: 0=Klassisch, 1=A, 2=B, 3=C, 4=D.
    void studioToolbarVariantChanged(int variant);
    void logoutRequested();
    /// Emitted when Speicher-Modus or linked cloud folders change.
    void storagePrefsChanged();
    /// User asked to run the first-run wizard again.
    void onboardingReplayRequested();

    void previewProfileRequested(const UiProfile& p);
    /// Guest Konto actions — MainWindow opens Study login / Google OAuth.
    void studyLoginRequested();
    void studyRegisterRequested();
    void googleLoginRequested();
    /// Compact burger-nav setting changed (tablet/laptop).
    void uiLayoutPrefsChanged();
    /// Auto-save / sidebar start / confirm-delete and related prefs.
    void appPrefsChanged();
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
    bool m_appShellMode{false};
    Ui::SettingsDialog *ui;
    UiProfileManager *m_profileManager;
    QListWidget *m_profileList;
    QString m_editId; // Stored ID for editor

    QStringList m_sectionTitles;
    class QStackedWidget *m_sectionStack{nullptr};
    QListWidget *m_sectionNav{nullptr};
    QWidget *m_navCol{nullptr};
    QWidget *m_contentCol{nullptr};
    QWidget *m_shellSplit{nullptr};

    void refreshProfileList();
    void openEditor(const QString &profileId);
    void refreshTheme();
    void animateSectionPage(QWidget *page);
};

#endif // SETTINGSDIALOG_H
//for
