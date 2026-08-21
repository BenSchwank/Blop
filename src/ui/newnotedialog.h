#ifndef NEWNOTEDIALOG_H
#define NEWNOTEDIALOG_H

#include <QColor>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QPoint>
#include <QStringList>

class QListWidget;
class QMouseEvent;
class QShowEvent;

class NewNoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewNoteDialog(QWidget *parent = nullptr);

    QString getNoteName() const;
    bool isInfiniteFormat() const;
    int backgroundType() const { return m_backgroundType; }
    QColor paperColor() const { return m_paperColor; }
    QStringList selectedTags() const;

protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void rebuildTagList();

    QLineEdit *m_nameInput{nullptr};
    QPushButton *m_btnCreate{nullptr};
    QPushButton *m_btnCancel{nullptr};

    QPushButton *m_btnFormatInfinite{nullptr};
    QPushButton *m_btnFormatA4{nullptr};
    QButtonGroup *m_groupFormat{nullptr};
    QButtonGroup *m_groupLayout{nullptr};
    QListWidget *m_tagList{nullptr};
    QLineEdit *m_tagInput{nullptr};

    int m_backgroundType{2};
    QColor m_paperColor{QColor(252, 250, 245)};

    QPoint m_dragPos;
    bool m_dialogIntroDone{false};
};

#endif // NEWNOTEDIALOG_H
