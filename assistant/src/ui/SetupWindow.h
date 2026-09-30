#pragma once

#include <QHash>
#include <QList>
#include <QWidget>

class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;

class SetupWindow : public QWidget {
    Q_OBJECT
public:
    explicit SetupWindow(QWidget *parent = nullptr);

    void refresh();
    void present();

signals:
    void finished();
    void voiceHotkeyChanged();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void saveAndClose();
    void showPage(int index);
    void applyFilter(const QString &text);

    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageLead = nullptr;
    QLabel *m_googleValue = nullptr;
    QLabel *m_sessionValue = nullptr;
    QLineEdit *m_navSearch = nullptr;
    QLineEdit *m_pageSearch = nullptr;
    QList<QPushButton *> m_navButtons;
    QList<QPushButton *> m_railButtons;
    QList<QWidget *> m_rows;
    QHash<QString, QKeySequenceEdit *> m_toolEdits;
    int m_page = 0;
    bool m_filterLock = false;
};
