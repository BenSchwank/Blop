#include "introscreen.h"
#include <QApplication>
#include <QScreen>

IntroScreen::IntroScreen(QWidget *parent)
    : QWidget(parent), m_finished(false) {

    // Setup frameless, transparent-ready window
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setStyleSheet("background-color: transparent;");

    // Make it fill the screen
    QScreen *screen = QApplication::primaryScreen();
    setGeometry(screen->geometry());

    // Setup Layout and Video Widget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setStyleSheet("background-color: transparent;");
    layout->addWidget(m_videoWidget);

    // Setup Media Player & Audio Output
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);

    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);
    
    // Connect Signals
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &IntroScreen::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &IntroScreen::onErrorOccurred);
}

IntroScreen::~IntroScreen() {
    // Child objects are automatically destructed by Qt
}

void IntroScreen::startAnimation() {
    // Assuming the video is embedded in the QRC under /assets/Intro Blop.mp4
    m_player->setSource(QUrl("qrc:/assets/Intro Blop.mp4"));
    m_player->play();
}

void IntroScreen::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (m_finished) return;

    // When the video finishes playing
    if (status == QMediaPlayer::EndOfMedia) {
        m_finished = true;
        emit introFinished();
    }
}

void IntroScreen::onErrorOccurred(QMediaPlayer::Error error, const QString &errorString) {
    qWarning() << "Intro Video Error:" << errorString;
    // Fallback if video fails to load/play: just skip intro
    if (!m_finished) {
        m_finished = true;
        emit introFinished();
    }
}
