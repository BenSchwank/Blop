#include "cloudwebexplorer.h"

#include "blop_dialogs.h"
#include "blop_theme.h"
#include "uiscale.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifdef BLOP_HAS_WEBENGINE
#include <QDialog>
#include <QDir>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#endif

#ifndef BLOP_HAS_WEBENGINE
#include <QDesktopServices>
#endif

#ifdef BLOP_HAS_WEBENGINE
namespace {

class CloudExplorerPage final : public QWebEnginePage {
public:
  explicit CloudExplorerPage(QWebEngineProfile *profile, QWidget *viewParent)
      : QWebEnginePage(profile, viewParent), m_viewParent(viewParent) {}

protected:
  QWebEnginePage *createWindow(WebWindowType) override {
    auto *dlg = new QDialog(m_viewParent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QStringLiteral("Anmeldung"));
    dlg->resize(UiScale::dp(480), UiScale::dp(640));
    auto *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *view = new QWebEngineView(dlg);
    lay->addWidget(view);
    auto *page = new QWebEnginePage(profile(), view);
    view->setPage(page);
    QObject::connect(page, &QWebEnginePage::windowCloseRequested, dlg,
                     &QDialog::close);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    return page;
  }

private:
  QWidget *m_viewParent{nullptr};
};

QString profileDir(const QString &id) {
  const QString root =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      QStringLiteral("/cloud-web/") + id;
  QDir().mkpath(root);
  return root;
}

} // namespace
#endif

CloudWebExplorer::CloudWebExplorer(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("CloudWebExplorer"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::StrongFocus);
  rebuildChrome();
  if (parent)
    parent->installEventFilter(this);
}

void CloudWebExplorer::rebuildChrome() {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *bar = new QWidget(this);
  bar->setObjectName(QStringLiteral("CloudExplorerBar"));
  bar->setFixedHeight(UiScale::dp(52));
  auto *barLay = new QHBoxLayout(bar);
  barLay->setContentsMargins(UiScale::dp(12), UiScale::dp(8), UiScale::dp(12),
                             UiScale::dp(8));
  barLay->setSpacing(UiScale::dp(8));

  auto mkNav = [bar](const QString &text) {
    auto *b = new QPushButton(text, bar);
    b->setFixedSize(UiScale::dp(36), UiScale::dp(36));
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
    return b;
  };
  m_btnBack = mkNav(QStringLiteral("‹"));
  m_btnForward = mkNav(QStringLiteral("›"));
  m_btnReload = mkNav(QStringLiteral("↻"));

  m_title = new QLabel(bar);
  m_title->setObjectName(QStringLiteral("CloudExplorerTitle"));

  m_urlBar = new QLineEdit(bar);
  m_urlBar->setObjectName(QStringLiteral("CloudExplorerUrl"));
  m_urlBar->setPlaceholderText(QStringLiteral("https://"));
  m_urlBar->setClearButtonEnabled(true);

  m_btnClose = new QPushButton(QStringLiteral("Fertig"), bar);
  m_btnClose->setObjectName(QStringLiteral("CloudExplorerDone"));
  m_btnClose->setCursor(Qt::PointingHandCursor);
  m_btnClose->setMinimumHeight(UiScale::dp(36));

  barLay->addWidget(m_btnBack);
  barLay->addWidget(m_btnForward);
  barLay->addWidget(m_btnReload);
  barLay->addWidget(m_title);
  barLay->addWidget(m_urlBar, 1);
  barLay->addWidget(m_btnClose);
  root->addWidget(bar);

#ifdef BLOP_HAS_WEBENGINE
  m_view = new QWebEngineView(this);
  if (QWebEngineSettings *ws = m_view->settings()) {
    ws->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    ws->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    ws->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    ws->setAttribute(QWebEngineSettings::WebGLEnabled, false);
    ws->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, false);
  }
  root->addWidget(m_view, 1);
  connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl &u) {
    if (m_urlBar && u.isValid() && !m_urlBar->hasFocus())
      m_urlBar->setText(u.toString());
  });
  connect(m_view, &QWebEngineView::loadFinished, this, [this](bool) {
    persistSession();
  });
#else
  auto *fallback = new QLabel(
      QStringLiteral(
          "Cloud-Explorer braucht Qt WebEngine.\n"
          "Die Cloud wird im Systembrowser geöffnet."),
      this);
  fallback->setAlignment(Qt::AlignCenter);
  fallback->setWordWrap(true);
  root->addWidget(fallback, 1);
#endif

  const QString accent = QStringLiteral("#7C5CFC");
  setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QWidget#CloudWebExplorer { background: #12141C; }"
      "QWidget#CloudExplorerBar { background: #1A1829; border-bottom: 1px solid "
      "rgba(120,130,160,0.22); }"
      "QLabel#CloudExplorerTitle { color: #F4F2FF; font-weight: 700; font-size: 14px; "
      "background: transparent; padding-right: 8px; }"
      "QLineEdit#CloudExplorerUrl { background: rgba(255,255,255,0.06); color: #E8E4FF; "
      "border: 1px solid rgba(120,130,160,0.28); border-radius: 10px; "
      "padding: 6px 12px; font-size: 13px; }"
      "QPushButton { background: transparent; color: #E8E4FF; border: none; "
      "font-size: 16px; border-radius: 8px; }"
      "QPushButton:hover { background: rgba(255,255,255,0.08); }"
      "QPushButton#CloudExplorerDone { background: %1; color: white; "
      "font-weight: 700; font-size: 13px; padding: 0 16px; border-radius: 10px; }"
      "QPushButton#CloudExplorerDone:hover { background: #6A4BE8; }")
                                      .arg(accent)));

  connect(m_btnClose, &QPushButton::clicked, this, [this]() {
    persistSession();
    hide();
    emit closed();
  });
  connect(m_urlBar, &QLineEdit::returnPressed, this, [this]() {
    const QUrl u = QUrl::fromUserInput(m_urlBar->text().trimmed());
    if (!u.isValid())
      return;
#ifdef BLOP_HAS_WEBENGINE
    if (m_view)
      m_view->setUrl(u);
#else
    QDesktopServices::openUrl(u);
#endif
  });
#ifdef BLOP_HAS_WEBENGINE
  connect(m_btnBack, &QPushButton::clicked, this, [this]() {
    if (m_view)
      m_view->back();
  });
  connect(m_btnForward, &QPushButton::clicked, this, [this]() {
    if (m_view)
      m_view->forward();
  });
  connect(m_btnReload, &QPushButton::clicked, this, [this]() {
    if (m_view)
      m_view->reload();
  });
#else
  m_btnBack->setEnabled(false);
  m_btnForward->setEnabled(false);
  connect(m_btnReload, &QPushButton::clicked, this, [this]() { goHome(); });
#endif
}

QUrl CloudWebExplorer::homeUrl() const {
  if (!m_entry.webUrl.isEmpty())
    return QUrl::fromUserInput(m_entry.webUrl);
  return QUrl::fromUserInput(CloudStorageStore::defaultWebUrl(m_entry.type));
}

void CloudWebExplorer::goHome() {
  const QUrl url = homeUrl();
  if (m_urlBar)
    m_urlBar->setText(url.toString());
#ifdef BLOP_HAS_WEBENGINE
  if (m_view)
    m_view->setUrl(url);
#else
  if (url.isValid())
    QDesktopServices::openUrl(url);
#endif
}

void CloudWebExplorer::persistSession() {
  if (m_entry.id.isEmpty())
    return;
#ifdef BLOP_HAS_WEBENGINE
  if (m_view && m_view->url().isValid() &&
      m_view->url().scheme().startsWith(QLatin1String("http"))) {
    const QString host = m_view->url().host();
    const bool loginHost = host.contains(QLatin1String("accounts.google")) ||
                           host.contains(QLatin1String("login.live")) ||
                           host.contains(QLatin1String("login.microsoftonline")) ||
                           host.contains(QLatin1String("login.microsoft"));
    if (!loginHost)
      m_entry.webUrl = m_view->url().toString();
  }
#endif
  m_entry.webConnected = true;
  if (m_entry.webUrl.isEmpty())
    m_entry.webUrl = CloudStorageStore::defaultWebUrl(m_entry.type);
  CloudStorageStore::upsert(m_entry);
}

void CloudWebExplorer::openEntry(const CloudStorageEntry &entry) {
  m_entry = entry;
  if (m_title)
    m_title->setText(m_entry.name.isEmpty()
                         ? CloudStorageStore::displayNameForType(m_entry.type)
                         : m_entry.name);

#ifdef BLOP_HAS_WEBENGINE
  if (m_view) {
    const QString dir = profileDir(m_entry.id.isEmpty() ? m_entry.type
                                                        : m_entry.id);
    QWebEnginePage *oldPage = m_view->page();
    QWebEngineProfile *old = m_profile;
    m_profile = new QWebEngineProfile(QStringLiteral("blop-cloud-%1")
                                          .arg(m_entry.id),
                                      m_view);
    m_profile->setPersistentCookiesPolicy(
        QWebEngineProfile::ForcePersistentCookies);
    m_profile->setPersistentStoragePath(dir);
    m_profile->setCachePath(dir + QStringLiteral("/cache"));
    m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    m_profile->setHttpUserAgent(
        QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                       "(KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36"));
    auto *page = new CloudExplorerPage(m_profile, m_view);
    m_view->setPage(page);
    if (oldPage && oldPage != page)
      oldPage->deleteLater();
    if (old)
      old->deleteLater();
  }
#endif
  goHome();
  show();
  raise();
  setFocus();
  QTimer::singleShot(0, this, [this]() {
    raise();
    setFocus();
  });
}

CloudWebExplorer *CloudWebExplorer::showOver(QWidget *host,
                                             CloudStorageEntry entry) {
  if (!host)
    return nullptr;
  QWidget *win = host->window() ? host->window() : host;

  if ((entry.type == QLatin1String("nextcloud") ||
       entry.type == QLatin1String("custom")) &&
      entry.webUrl.isEmpty()) {
    const QString typed = BlopDialogs::promptText(
        win, entry.name.isEmpty() ? QStringLiteral("Cloud-Adresse")
                                  : entry.name,
        QStringLiteral("Web-Adresse (https://…):"),
        QStringLiteral("https://"));
    if (typed.trimmed().isEmpty())
      return nullptr;
    entry.webUrl = QUrl::fromUserInput(typed.trimmed()).toString();
    CloudStorageStore::upsert(entry);
  }

#ifndef BLOP_HAS_WEBENGINE
  const QUrl url = entry.webUrl.isEmpty()
                       ? CloudStorageStore::defaultWebUrl(entry.type)
                       : QUrl::fromUserInput(entry.webUrl);
  if (url.isValid())
    QDesktopServices::openUrl(url);
  entry.webConnected = true;
  CloudStorageStore::upsert(entry);
  BlopDialogs::notify(
      win, CloudStorageStore::displayNameForType(entry.type),
      QStringLiteral(
          "Die Cloud wurde im Browser geöffnet.\n"
          "Melde dich dort an und arbeite wie in einem Datei-Explorer."));
  return nullptr;
#endif

  auto *ex = win->findChild<CloudWebExplorer *>(
      QStringLiteral("CloudWebExplorer"), Qt::FindDirectChildrenOnly);
  if (!ex)
    ex = new CloudWebExplorer(win);
  ex->setGeometry(win->rect());
  ex->openEntry(entry);
  return ex;
}

bool CloudWebExplorer::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Show) &&
      isVisible() && parentWidget()) {
    setGeometry(parentWidget()->rect());
  }
  return QWidget::eventFilter(watched, event);
}

void CloudWebExplorer::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    persistSession();
    hide();
    emit closed();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}
