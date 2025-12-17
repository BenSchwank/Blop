#ifndef NEWNOTEDIALOG_H
#define NEWNOTEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QPoint>

class NewNoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewNoteDialog(QWidget *parent = nullptr);

    QString getNoteName() const;
    bool isInfiniteFormat() const;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void setupUi();

    QLineEdit *m_nameInput;
    QPushButton *m_btnCreate;
    QPushButton *m_btnCancel;

    // Format Auswahl
    QPushButton *m_btnFormatInfinite;
    QPushButton *m_btnFormatA4;
    QButtonGroup *m_groupFormat;

    // Helper für Dragging (Frameless Window)
    QPoint m_dragPos;
};

#endif // NEWNOTEDIALOG_H
