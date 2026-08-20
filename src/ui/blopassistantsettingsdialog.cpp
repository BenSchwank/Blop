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
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>

BlopAssistantSettingsDialog::BlopAssistantSettingsDialog(QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Blop Assistant — Einstellungen"));
  setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint |
                 Qt::WindowCloseButtonHint);
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

  auto *ai = new QGroupBox(QStringLiteral("KI"), this);
  auto *af = new QFormLayout(ai);
  af->setSpacing(10);
  m_llmOn = new QCheckBox(QStringLiteral("Frei antworten (Chat-Modell)"), ai);
  af->addRow(m_llmOn);
  m_llmPreset = new QComboBox(ai);
  m_llmPreset->addItem(QStringLiteral("OpenAI"), QStringLiteral("openai"));
  m_llmPreset->addItem(QStringLiteral("OpenRouter"), QStringLiteral("openrouter"));
  m_llmPreset->addItem(QStringLiteral("Groq"), QStringLiteral("groq"));
  m_llmPreset->addItem(QStringLiteral("Ollama (lokal)"), QStringLiteral("ollama"));
  m_llmPreset->addItem(QStringLiteral("Eigene URL"), QStringLiteral("custom"));
  af->addRow(QStringLiteral("Anbieter"), m_llmPreset);
  m_llmKey = new QLineEdit(ai);
  m_llmKey->setEchoMode(QLineEdit::Password);
  m_llmKey->setPlaceholderText(QStringLiteral("API-Key (sk-… oder Groq/OpenRouter)"));
  af->addRow(QStringLiteral("API-Key"), m_llmKey);
  m_llmUrl = new QLineEdit(ai);
  af->addRow(QStringLiteral("Base-URL"), m_llmUrl);
  m_llmModel = new QLineEdit(ai);
  af->addRow(QStringLiteral("Modell"), m_llmModel);
  auto *aiHint = new QLabel(
      QStringLiteral("Ohne Key bleibt nur der lokale Parser. Mit Key "
                     "kannst du alles fragen — Notizen und Apps laufen "
                     "trotzdem über Bestätigung."),
      ai);
  aiHint->setWordWrap(true);
  aiHint->setStyleSheet(QStringLiteral("color: #8E8E8E; font-size: 11px;"));
  af->addRow(QString(), aiHint);
  root->addWidget(ai);

  connect(m_llmPreset, &QComboBox::currentIndexChanged, this, [this]() {
    const QString id = m_llmPreset->currentData().toString();
    if (id == QLatin1String("openai")) {
      m_llmUrl->setText(QStringLiteral("https://api.openai.com/v1"));
      if (m_llmModel->text().isEmpty() ||
          m_llmModel->text().startsWith(QLatin1String("llama")) ||
          m_llmModel->text().startsWith(QLatin1String("openai/")))
        m_llmModel->setText(QStringLiteral("gpt-4o-mini"));
    } else if (id == QLatin1String("openrouter")) {
      m_llmUrl->setText(QStringLiteral("https://openrouter.ai/api/v1"));
      m_llmModel->setText(QStringLiteral("openai/gpt-4o-mini"));
    } else if (id == QLatin1String("groq")) {
      m_llmUrl->setText(QStringLiteral("https://api.groq.com/openai/v1"));
      m_llmModel->setText(QStringLiteral("llama-3.1-8b-instant"));
    } else if (id == QLatin1String("ollama")) {
      m_llmUrl->setText(QStringLiteral("http://127.0.0.1:11434/v1"));
      m_llmModel->setText(QStringLiteral("llama3.1"));
    }
    save();
    emit prefsChanged();
  });

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
  connect(m_llmOn, &QCheckBox::toggled, this, [this]() {
    save();
    emit prefsChanged();
  });
  connect(m_llmKey, &QLineEdit::editingFinished, this, [this]() {
    save();
    emit prefsChanged();
  });
  connect(m_llmUrl, &QLineEdit::editingFinished, this, [this]() {
    save();
    emit prefsChanged();
  });
  connect(m_llmModel, &QLineEdit::editingFinished, this, [this]() {
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
  m_llmOn->setChecked(BlopAssistantPrefs::llmEnabled());
  m_llmKey->setText(BlopAssistantPrefs::llmApiKey());
  m_llmUrl->setText(BlopAssistantPrefs::llmBaseUrl());
  m_llmModel->setText(BlopAssistantPrefs::llmModel());
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
  BlopAssistantPrefs::setLlmEnabled(m_llmOn->isChecked());
  BlopAssistantPrefs::setLlmApiKey(m_llmKey->text().trimmed());
  BlopAssistantPrefs::setLlmBaseUrl(m_llmUrl->text().trimmed());
  BlopAssistantPrefs::setLlmModel(m_llmModel->text().trimmed());
}
