#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QtQml/qqmlregistration.h>

// Formatting commands and state for a TextEdit's selection: bold, italic,
// strikethrough, headings and lists. Bind selectionStart and selectionEnd to
// the editor's so the state properties follow the cursor.

//
// Toggling bold, italic or strikethrough with nothing selected applies to the
// next character typed at the cursor; text typed after it then inherits the
// format as usual.
// This is done by setting the format of the typed character, rather than the
// format the editor types with, which isn't accessible.
//
// If titleHeadingLevel is set, the first line is kept as a heading at that
// level: after every edit, it's reformatted as part of the same undo step.
class TextFormatter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int selectionStart READ selectionStart WRITE setSelectionStart NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEnd READ selectionEnd WRITE setSelectionEnd NOTIFY selectionChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY formatChanged)
    Q_PROPERTY(bool italic READ italic WRITE setItalic NOTIFY formatChanged)
    Q_PROPERTY(bool strikeOut READ strikeOut WRITE setStrikeOut NOTIFY formatChanged)
    Q_PROPERTY(int headingLevel READ headingLevel WRITE setHeadingLevel NOTIFY formatChanged)
    Q_PROPERTY(ListStyle listStyle READ listStyle WRITE setListStyle NOTIFY formatChanged)
    Q_PROPERTY(int titleHeadingLevel READ titleHeadingLevel WRITE setTitleHeadingLevel NOTIFY titleHeadingLevelChanged)
    Q_PROPERTY(bool onTitleLine READ onTitleLine NOTIFY formatChanged)

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
    bool strikeOut() const;
    void setStrikeOut(bool strikeOut);

    // 0 for body text, 1-6 for h1-h6.
    int headingLevel() const;
    void setHeadingLevel(int level);

    // Bulleted lists get a different bullet at each level: see bulletStyle()
    // in the .cpp file.
    ListStyle listStyle() const;
    void setListStyle(ListStyle style);

    // The heading level the first line is kept at, or 0 to leave it alone.
    int titleHeadingLevel() const;
    void setTitleHeadingLevel(int level);
    // Whether the selection starts on the first line while it's kept as a
    // heading, so heading and list formatting don't apply.
    bool onTitleLine() const;

    // Call when Return is pressed. Handles the cases where the editor's
    // default behavior is wrong and returns true if it did: Return on an empty
    // list item moves it up a level, or ends the list at the top level, and
    // Return at the end of a heading starts body text rather than another
    // heading.
    Q_INVOKABLE bool handleReturn();

    // Moves the selected list items, and the items nested under them, one
    // level deeper (delta 1) or shallower (delta -1). Returns false if the
    // selection doesn't start in a list. An item can only be one level deeper
    // than the item before it, as in Markdown, so the first item of a list
    // can't be nested; that and shallower than the top level do nothing.
    Q_INVOKABLE bool shiftListLevel(int delta);

    // Call when Space is pressed. If the text before the cursor in a
    // paragraph is "*" or "-", or "1.", replaces it with a bulleted or
    // numbered list and returns true. Undoing that gets back the typed text.
    Q_INVOKABLE bool handleSpace();

    // Replaces the selection with plain `text`, or inserts it at the cursor,
    // in the format typed text would get there. The editor's own insert()
    // would read it as Markdown.
    Q_INVOKABLE void insertText(const QString &text);

    // Call after replacing the editor's text. The editor keeps typing with the
    // format it had at the cursor in the old text; this makes the next
    // character typed use the new text's format at the cursor instead.
    Q_INVOKABLE void resetTypingFormat();

    // Call after replacing the editor's text with Markdown from markdown():
    // turns the lines that stand for empty lines back into empty lines,
    // gives bulleted lists the bullet for their level, and formats the first
    // line as a heading at titleHeadingLevel, without making that an undo
    // step. Returns whether anything changed.
    Q_INVOKABLE bool prepareLoadedText();

    // The document as Markdown. Use this rather than the editor's text: Qt
    // indents a nested list by its own marker's width per level, which is
    // wrong when it's in a list of the other type (bullets in numbers or
    // numbers in bullets), so the note wouldn't load back the same. This
    // corrects the indentation of list items.
    //
    // Markdown has no empty lines, so each one (other than at the end) is
    // written as a line holding a non-breaking space, which
    // prepareLoadedText() turns back into an empty line. Qt also writes an
    // empty heading without the line break after it, joining it to the next
    // line, so an empty heading is written as one of those lines too.
    //
    // Qt's writer also leaves some characters unescaped, such as ~ and _, so
    // literal text could load back as formatting. Those are escaped here.
    Q_INVOKABLE QString markdown() const;

signals:
    void documentChanged();
    void selectionChanged();
    void formatChanged();
    void titleHeadingLevelChanged();

private:
    QTextDocument *textDocument() const;
    QTextCursor cursor() const;
    QTextCharFormat charFormat() const;
    void mergeCharFormat(const QTextCharFormat &format);
    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void onUndoCommandAdded();

    QPointer<QQuickTextDocument> m_document;
    int m_selectionStart = 0;
    int m_selectionEnd = 0;
    // Format for the next text typed at m_pendingPosition, set by toggling
    // bold, italic or strikethrough with an empty selection or by
    // resetTypingFormat(). Cleared when the cursor moves elsewhere.
    QTextCharFormat m_pendingFormat;
    int m_pendingPosition = -1;
    int m_titleHeadingLevel = 0;
};
