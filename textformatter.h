#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QtQml/qqmlregistration.h>

// Formatting commands and state for a TextEdit's selection: bold, italic,
// headings and lists. Bind selectionStart and selectionEnd to the editor's so
// the state properties follow the cursor.
//
// Toggling bold or italic with nothing selected applies to the next character
// typed at the cursor; text typed after it then inherits the format as usual.
// This is done by setting the format of the typed character, rather than the
// format the editor types with, which isn't accessible.
class TextFormatter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int selectionStart READ selectionStart WRITE setSelectionStart NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEnd READ selectionEnd WRITE setSelectionEnd NOTIFY selectionChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY formatChanged)
    Q_PROPERTY(bool italic READ italic WRITE setItalic NOTIFY formatChanged)
    Q_PROPERTY(int headingLevel READ headingLevel WRITE setHeadingLevel NOTIFY formatChanged)
    Q_PROPERTY(ListStyle listStyle READ listStyle WRITE setListStyle NOTIFY formatChanged)

public:
    enum ListStyle {
        NoList,
        BulletList,
        NumberedList,
    };
    Q_ENUM(ListStyle)

    explicit TextFormatter(QObject *parent = nullptr);

    QQuickTextDocument *document() const;
    void setDocument(QQuickTextDocument *document);

    int selectionStart() const;
    void setSelectionStart(int position);
    int selectionEnd() const;
    void setSelectionEnd(int position);

    bool bold() const;
    void setBold(bool bold);
    bool italic() const;
    void setItalic(bool italic);

    // 0 for body text, 1-6 for h1-h6.
    int headingLevel() const;
    void setHeadingLevel(int level);

    ListStyle listStyle() const;
    void setListStyle(ListStyle style);

    // Call when Return is pressed. Handles the cases where the editor's
    // default behavior is wrong and returns true if it did: Return on an empty
    // list item ends the list, and Return at the end of a heading starts body
    // text rather than another heading.
    Q_INVOKABLE bool handleReturn();

    // Call after replacing the editor's text. The editor keeps typing with the
    // format it had at the cursor in the old text; this makes the next
    // character typed use the new text's format at the cursor instead.
    Q_INVOKABLE void resetTypingFormat();

signals:
    void documentChanged();
    void selectionChanged();
    void formatChanged();

private:
    QTextDocument *textDocument() const;
    QTextCursor cursor() const;
    QTextCharFormat charFormat() const;
    void mergeCharFormat(const QTextCharFormat &format);
    void onContentsChange(int position, int charsRemoved, int charsAdded);

    QPointer<QQuickTextDocument> m_document;
    int m_selectionStart = 0;
    int m_selectionEnd = 0;
    // Format for the next text typed at m_pendingPosition, set by toggling
    // bold or italic with an empty selection or by resetTypingFormat().
    // Cleared when the cursor moves elsewhere.
    QTextCharFormat m_pendingFormat;
    int m_pendingPosition = -1;
};
