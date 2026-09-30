#pragma once

#include <QHash>
#include <QWidget>

class QKeySequenceEdit;
class QLabel;

class SetupWindow : public QWidget {
    Q_OBJECT
public:
    explicit SetupWindow(QWidget *parent = nullptr);

    void refresh();
    void present();

signals:
    void finished();
    void voiceHotkeyChanged();

private:
    void saveAndClose();

    QLabel *m_status = nullptr;
    QHash<QString, QKeySequenceEdit *> m_toolEdits;
};
