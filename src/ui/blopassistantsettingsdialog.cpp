#include "blopassistantsettingsdialog.h"

#include "blopassistantprefs.h"
#include "blopassistantvoice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>

BlopAssistantSettingsDialog::BlopAssistantSettingsDialog(QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Blop Assistant — Einstellungen"));
  setWindowFlag(Qt::Window, true);
  setMinimumWidth(420);
  setModal(false);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(18, 16, 18, 16);
  root->setSpacing(14);

  auto *intro = new QLabel(
      QStringLiteral("Die Notch öffnet nur den Chat.\n"
                     "Dieses Fenster kommt aus dem Symbol in der Taskleiste."),
      this);
  intro->setWordWrap(true);
  intro->setStyleSheet(QStringLiteral("color: #B4B4B4; font-size: 12px;"));
  root->addWidget(intro);

  auto *keys = new QGroupBox(QStringLiteral("Tastenkombinationen"), this);
  auto *kf = new QFormLayout(keys);
  kf->setSpacing(10);
  m_openChat = new QKeySequenceEdit(keys);
  m_openChat->setClearButtonEnabled(true);
  m_openChat->setFocusPolicy(Qt::ClickFocus);
  m_pushToTalk = new QKeySequenceEdit(keys);
  m_pushToTalk->setClearButtonEnabled(true);
  m_pushToTalk->setFocusPolicy(Qt::ClickFocus);
  kf->addRow(QStringLiteral("Chat öffnen"), m_openChat);
  kf->addRow(QStringLiteral("Push-to-talk (halten)"), m_pushToTalk);
  auto *keyHint = new QLabel(
      QStringLiteral("Push-to-talk: Taste gedrückt halten, loslassen zum "
                     "Senden. Unabhängig von der Notch."),
      keys);
  keyHint->setWordWrap(true);
  keyHint->setStyleSheet(QStringLiteral("color: #8E8E8E; font-size: 11px;"));
  kf->addRow(QString(), keyHint);
  root->addWidget(keys);

  auto *voice = new QGroupBox(QStringLiteral("Stimme"), this);
  auto *vf = new QFormLayout(voice);
  vf->setSpacing(10);
  m_speak = new QCheckBox(QStringLiteral("Antworten vorlesen"), voice);
  vf->addRow(m_speak);

  auto *rateRow = new QWidget(voice);
  auto *rh = new QHBoxLayout(rateRow);
  rh->setContentsMargins(0, 0, 0, 0);
  m_rate = new QSlider(Qt::Horizontal, rateRow);
  m_rate->setRange(-6, 2);
  m_rateValue = new QLabel(rateRow);
  m_rateValue->setMinimumWidth(36);
  rh->addWidget(m_rate, 1);
  rh->addWidget(m_rateValue, 0);
  vf->addRow(QStringLiteral("Tempo"), rateRow);
  connect(m_rate, &QSlider::valueChanged, this, [this](int v) {
    QString label = QStringLiteral("ruhig");
    if (v >= 0)
      label = QStringLiteral("schnell");
    else if (v >= -2)
      label = QStringLiteral("normal");
    m_rateValue->setText(label);
  });

  m_voice = new QComboBox(voice);
  m_voice->addItem(QStringLiteral("Automatisch (ruhig, Deutsch)"),
                   QStringLiteral("auto"));
  m_voice->addItem(QStringLiteral("Deutsch, weiblich"),
                   QStringLiteral("de-female"));
  m_voice->addItem(QStringLiteral("Deutsch, männlich"),
                   QStringLiteral("de-male"));
  m_voice->addItem(QStringLiteral("Englisch"), QStringLiteral("en"));
  vf->addRow(QStringLiteral("Stimme"), m_voice);

  auto *test = new QPushButton(QStringLiteral("Stimme testen"), voice);
  test->setCursor(Qt::PointingHandCursor);
  connect(test, &QPushButton::clicked, this, [this]() {
    save();
    emit prefsChanged();
    BlopAssistantVoice::speak(
        QStringLiteral("Alles klar. So klinge ich, wenn ich antworte."));
  });
  vf->addRow(QString(), test);
  root->addWidget(voice);

  auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::close);
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::close);
  if (QPushButton *closeBtn = box->button(QDialogButtonBox::Close)) {
    closeBtn->setDefault(true);
    m_closeBtn = closeBtn;
  }
  root->addWidget(box);

  setStyleSheet(QStringLiteral(
      "QDialog { background: #1E1E1E; color: #E8E8E8; }"
      "QGroupBox { color: #E8E8E8; font-weight: 700; border: 1px solid #404040;"
      "  border-radius: 10px; margin-top: 12px; padding: 12px 10px 10px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }"
      "QLabel { color: #E8E8E8; }"
      "QCheckBox { color: #E8E8E8; }"
      "QLineEdit, QKeySequenceEdit, QComboBox {"
      "  background: #262626; color: #E8E8E8; border: 1px solid #404040;"
      "  border-radius: 8px; padding: 6px 8px; }"
      "QPushButton { background: #5B9DFF; color: #0B1220; border: none;"
      "  border-radius: 8px; padding: 7px 12px; font-weight: 700; }"
      "QPushButton:hover { background: #7CB0FF; }"
      "QSlider::groove:horizontal { height: 4px; background: #404040; border-radius: 2px; }"
      "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0;"
      "  background: #5B9DFF; border-radius: 7px; }"));

  load();
  connect(m_openChat, &QKeySequenceEdit::keySequenceChanged, this, [this]() {
    save();
    emit prefsChanged();
  });
  connect(m_pushToTalk, &QKeySequenceEdit::keySequenceChanged, this, [this]() {
    save();
    emit prefsChanged();
  });
  connect(m_speak, &QCheckBox::toggled, this, [this]() {
    save();
    emit prefsChanged();
  });
  connect(m_rate, &QSlider::valueChanged, this, [this]() {
    save();
    emit prefsChanged();
  });
  connect(m_voice, &QComboBox::currentIndexChanged, this, [this]() {
    save();
    emit prefsChanged();
  });
}

void BlopAssistantSettingsDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  if (m_closeBtn)
    m_closeBtn->setFocus(Qt::OtherFocusReason);
}

void BlopAssistantSettingsDialog::load() {
  m_openChat->setKeySequence(BlopAssistantPrefs::openChatSequence());
  m_pushToTalk->setKeySequence(BlopAssistantPrefs::pushToTalkSequence());
  m_speak->setChecked(BlopAssistantPrefs::speakReplies());
  m_rate->setValue(BlopAssistantPrefs::speechRate());
  const int idx =
      m_voice->findData(BlopAssistantPrefs::voiceId());
  m_voice->setCurrentIndex(idx >= 0 ? idx : 0);
}

void BlopAssistantSettingsDialog::save() {
  QKeySequence openSeq = m_openChat->keySequence();
  QKeySequence pttSeq = m_pushToTalk->keySequence();
  if (BlopAssistantPrefs::unusableShortcut(openSeq))
    openSeq = QKeySequence(QStringLiteral("Ctrl+Shift+Space"));
  if (BlopAssistantPrefs::unusableShortcut(pttSeq))
    pttSeq = QKeySequence(QStringLiteral("Ctrl+Shift+T"));
  if (m_openChat->keySequence() != openSeq)
    m_openChat->setKeySequence(openSeq);
  if (m_pushToTalk->keySequence() != pttSeq)
    m_pushToTalk->setKeySequence(pttSeq);
  BlopAssistantPrefs::setOpenChatSequence(openSeq);
  BlopAssistantPrefs::setPushToTalkSequence(pttSeq);
  BlopAssistantPrefs::setSpeakReplies(m_speak->isChecked());
  BlopAssistantPrefs::setSpeechRate(m_rate->value());
  BlopAssistantPrefs::setVoiceId(m_voice->currentData().toString());
}
