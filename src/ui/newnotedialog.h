#ifndef NEWNOTEDIALOG_H
#define NEWNOTEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QPoint>

class QMouseEvent;
class QShowEvent;

class NewNoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewNoteDialog(QWidget *parent = nullptr);

    QString getNoteName() const;
    bool isInfiniteFormat() const;

protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void setupUi();

    QLineEdit *m_nameInput;
    QPushButton *m_btnCreate;
    QPushButton *m_btnCancel;

    // Format Selection
    QPushButton *m_btnFormatInfinite;
    QPushButton *m_btnFormatA4;
    QButtonGroup *m_groupFormat;

    // Helper for dragging (Frameless Window)
    QPoint m_dragPos;
    bool m_dialogIntroDone{false};
};

#endif // NEWNOTEDIALOG_H
