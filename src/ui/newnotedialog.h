#ifndef NEWNOTEDIALOG_H
#define NEWNOTEDIALOG_H

#include <QButtonGroup>
#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QLineEdit>
#include <QPoint>
#include <QPushButton>
#include <QStringList>

class QHBoxLayout;
class QLabel;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;
class BlopModal;

/// Single-sheet Neue-Notiz composer (format chips + options + footer).
class NewNoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewNoteDialog(QWidget *parent = nullptr);

    QString getNoteName() const;
    /// 0 = Unendlich, 1 = DIN A4, 2 = Struktur
    int createFormat() const;
    bool isInfiniteFormat() const;
    bool isStrukturFormat() const;
    int backgroundType() const { return m_backgroundType; }
    QColor paperColor() const { return m_paperColor; }
    QStringList selectedTags() const;

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    struct SheetTokens {
        QColor surface;
        QColor surfaceAlt;
        QColor ink;
        QColor muted;
        QColor border;
        QColor hover;
        QColor accentSoft;
        QString inputQss;
        QString primaryBtnQss;
        QString secondaryBtnQss;
        bool dark{false};
    };

    void setupUi();
    SheetTokens tokens() const;
    void applyChrome();
    void rebuildTagList();
    void refreshTemplateIcons();
    void refreshPaperSwatch();
    void refreshLivePreview();
    void setFormat(int formatId);
    void applyHostModalSize(bool animate);
    void updateFormatChips();
    void setLayoutSectionVisible(bool visible);
    BlopModal *hostModal() const;

    QWidget *m_root{nullptr};
    QFrame *m_composer{nullptr};
    QWidget *m_formatChipBar{nullptr};
    QHBoxLayout *m_formatChipLay{nullptr};
    QLabel *m_composerTitle{nullptr};
    QLabel *m_livePreview{nullptr};

    QLineEdit *m_nameInput{nullptr};
    QPushButton *m_btnCreate{nullptr};
    QPushButton *m_btnCancel{nullptr};

    QWidget *m_layoutSection{nullptr};
    QWidget *m_paperSection{nullptr};
    QLabel *m_strukturHint{nullptr};
    QButtonGroup *m_groupLayout{nullptr};
    QButtonGroup *m_formatChipGroup{nullptr};
    QButtonGroup *m_tagGroup{nullptr};
    QWidget *m_tagChipHost{nullptr};
    QHBoxLayout *m_tagChipLay{nullptr};
    QLineEdit *m_tagInput{nullptr};
    QPushButton *m_paperSwatch{nullptr};

    int m_backgroundType{2};
    QColor m_paperColor{QColor(252, 250, 245)};
    int m_selectedFormat{1};

    QPoint m_dragPos;
    bool m_dialogIntroDone{false};
};

#endif // NEWNOTEDIALOG_H
