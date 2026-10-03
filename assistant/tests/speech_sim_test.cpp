#include "platform/SpeechInput.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

namespace {

struct Outcome {
    int code = 2;
    QString heard;
    QString fail;
};

Outcome listen(const QByteArray &text) {
    qputenv("BLOP_SPEECH_SIM_TEXT", text);
    auto *input = new SpeechInput;
    Outcome outcome;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&loop]() { loop.exit(2); });
    QObject::connect(input, &SpeechInput::recognized, &loop,
                     [&loop, &outcome](const QString &value) {
                         outcome.heard = value;
                         loop.exit(0);
                     });
    QObject::connect(input, &SpeechInput::failed, &loop,
                     [&loop, &outcome](const QString &value) {
                         outcome.fail = value;
                         loop.exit(1);
                     });
    timeout.start(3000);
    input->start();
    QTimer::singleShot(250, input, &SpeechInput::stop);
    outcome.code = loop.exec();
    delete input;
    return outcome;
}

} // namespace

int main(int argc, char *argv[]) {
    qputenv("BLOP_SPEECH_SIM", "1");
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const Outcome said = listen(QByteArrayLiteral("Hallo wie geht es dir"));
    if (said.code != 0 || said.heard != QStringLiteral("Hallo wie geht es dir")) {
        out << "FAIL satz code=" << said.code << " heard=\"" << said.heard << "\" fail=\""
            << said.fail << "\"\n";
        return 1;
    }
    out << "OK satz: " << said.heard << "\n";

    const Outcome quiet = listen(QByteArray());
    if (quiet.code != 1 || quiet.fail != QStringLiteral("Nichts erkannt.")) {
        out << "FAIL stille code=" << quiet.code << " heard=\"" << quiet.heard << "\" fail=\""
            << quiet.fail << "\"\n";
        return 1;
    }
    out << "OK stille: " << quiet.fail << "\n";
    return 0;
}
