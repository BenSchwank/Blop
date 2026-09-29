#ifndef NEWNOTEDIALOG_H
#define NEWNOTEDIALOG_H

#include <QColor>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QFrame>
#include <QPoint>
#include <QStringList>
#include <QVector>

class QListWidget;
class QMouseEvent;
class QShowEvent;
class QScrollArea;
class QLabel;
class QResizeEvent;
class QToolButton;
class QHBoxLayout;
class BlopModal;

/// Vertical tile: preview, title, hint, blurb — for horizontal pick row.
class NewNoteFormatCard : public QFrame
{
    Q_OBJECT
public:
    explicit NewNoteFormatCard(int formatId, const QString &title,
                               const QString &hint, const QString &blurb,
                               QWidget *parent = nullptr);

    int formatId() const { return m_formatId; }
    void setSelected(bool on);
    void setPreview(const QPixmap &pm);
    void applyChrome(const QString &surface, const QString &ink,
                     const QString &muted, const QString &border,
                     const QString &hover, const QString &accent,
                     const QString &soft, int radius);

signals:
    void clicked(int formatId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void refreshStyle();

    int m_formatId{-1};
    bool m_selected{false};
    bool m_hovered{false};
    QLabel *m_icon{nullptr};
    QLabel *m_title{nullptr};
    QLabel *m_hint{nullptr};
    QLabel *m_blurb{nullptr};
    QString m_qssSurface;
    QString m_qssInk;
    QString m_qssMuted;
    QString m_qssBorder;
    QString m_qssHover;
    QString m_qssAccent;
    QString m_qssSoft;
    int m_radius{12};
};

/// Pick: 3 horizontal tiles in a medium modal → modal grows → composer.
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
    struct DeckTokens {
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
        QString scrollQss;
        bool dark{false};
    };

    void setupUi();
    DeckTokens tokens() const;
    void applyChrome();
    void rebuildTagList();
    void applyTagFilter();
    void refreshTemplateIcons();
    void refreshDeckPreviews();
    void refreshPaperSwatch();
    void refreshLivePreview();
    void setFormat(int formatId);
    void selectDeckFormat(int formatId);
    void collapseToDeck();
    void applyHostModalSize(bool expanded, bool animate);
    void updateDeckVisualState();
    void updateFormatChips();
    void setLayoutSectionVisible(bool visible, bool animate);
    NewNoteFormatCard *makeDeckCard(int formatId, const QString &title,
                                    const QString &hint, const QString &blurb);
    BlopModal *hostModal() const;

    QWidget *m_root{nullptr};
    QWidget *m_pickPage{nullptr};
    QLabel *m_deckHint{nullptr};
    QLabel *m_deckSubhint{nullptr};
    QPushButton *m_btnPickCancel{nullptr};
    QHBoxLayout *m_pickRow{nullptr};
    QVector<NewNoteFormatCard *> m_deckCards;

    QFrame *m_composer{nullptr};
    QWidget *m_formatChipBar{nullptr};
    QHBoxLayout *m_formatChipLay{nullptr};
    QLabel *m_composerTitle{nullptr};
    QPushButton *m_btnBackToDeck{nullptr};
    QLabel *m_livePreview{nullptr};

    QLineEdit *m_nameInput{nullptr};
    QPushButton *m_btnCreate{nullptr};
    QPushButton *m_btnCancel{nullptr};

    NewNoteFormatCard *m_btnFormatInfinite{nullptr};
    NewNoteFormatCard *m_btnFormatA4{nullptr};
    NewNoteFormatCard *m_btnFormatStruktur{nullptr};
    QWidget *m_layoutSection{nullptr};
    QWidget *m_paperSection{nullptr};
    QLabel *m_strukturHint{nullptr};
    QButtonGroup *m_groupLayout{nullptr};
    QButtonGroup *m_formatChipGroup{nullptr};
    QListWidget *m_tagList{nullptr};
    QLineEdit *m_tagInput{nullptr};
    QLineEdit *m_tagSearch{nullptr};
    QPushButton *m_paperSwatch{nullptr};
    QScrollArea *m_scroll{nullptr};

    int m_backgroundType{2};
    QColor m_paperColor{QColor(252, 250, 245)};
    int m_selectedFormat{-1};
    bool m_deckExpanded{false};

    QPoint m_dragPos;
    bool m_dialogIntroDone{false};
};

#endif // NEWNOTEDIALOG_H
