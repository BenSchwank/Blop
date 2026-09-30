#pragma once

#include <QDialog>
#include <QList>

class QAudioOutput;
class QEvent;
class QKeyEvent;
class QLabel;
class QMediaPlayer;
class QShowEvent;
class QResizeEvent;
class BlopWindowControls;

/// Full-screen first-run sequence: logo video, welcome/login, AGB,
/// storage, then a short tour. The library stays covered until finish.
class OnboardingWizard : public QDialog {
  Q_OBJECT
public:
  explicit OnboardingWizard(QWidget *parent = nullptr);

  bool wasCompleted() const { return m_completed; }

protected:
  void showEvent(QShowEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void reject() override;
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  enum Page {
    PageVideo = 0,
    PageWelcome,
    PageAgb,
    PageStorage,
    PageFolder,
    PageNewNote,
    PageLibrary,
    PageSettings
  };

  void buildPages();
  void goTo(int page);
  void onNext();
  void onBack();
  bool applyStorageChoice();
  bool pickCloudFolder();
  void fitToHost();
  void layoutCard();
  void placeWindowControls();
  void requestQuit();
  void startIntroVideo();
  void stopIntroVideo();
  void refreshWelcome();
  bool hasSignedInName() const;
  void syncSteps();

  bool m_completed{false};
  bool m_introDone{false};
  bool m_introStarted{false};
  bool m_introSawFrame{false};
  bool m_fitting{false};
  int m_page{0};
  int m_storageMode{0}; // StoragePrefs::Mode
  QString m_cloudFolder;
  QString m_linkedProvider;

  class QStackedWidget *m_stack{nullptr};
  class QPushButton *m_btnBack{nullptr};
  class QPushButton *m_btnNext{nullptr};
  class QWidget *m_footer{nullptr};
  class QCheckBox *m_agbCheck{nullptr};
  class QButtonGroup *m_storageGroup{nullptr};
  class QLabel *m_folderLabel{nullptr};
  class QLabel *m_stepLabel{nullptr};
  class QLabel *m_welcomeTitle{nullptr};
  class QLabel *m_welcomeStatus{nullptr};
  class QLabel *m_loginStatus{nullptr};
  class QPushButton *m_btnGoogle{nullptr};
  QString m_accountLine;
  class QWidget *m_brand{nullptr};
  class QFrame *m_card{nullptr};
  class QWidget *m_backdrop{nullptr};
  class QPushButton *m_btnSkip{nullptr};
  BlopWindowControls *m_winControls{nullptr};
  QList<QLabel *> m_stepLabels;
  QList<QWidget *> m_stepDots;

  QMediaPlayer *m_player{nullptr};
  QAudioOutput *m_audio{nullptr};
  class IntroStage *m_introView{nullptr};
};
