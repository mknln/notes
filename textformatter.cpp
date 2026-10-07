#include "textformatter.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextList>

#include <algorithm>

TextFormatter::TextFormatter(QObject *parent)
    : QObject(parent)
{
}

QQuickTextDocument *TextFormatter::document() const
{
    return m_document;
}

void TextFormatter::setDocument(QQuickTextDocument *document)
{
    if (document == m_document)
        return;

    if (QTextDocument *doc = textDocument())
        disconnect(doc, nullptr, this, nullptr);
    m_document = document;
    if (QTextDocument *doc = textDocument())
        connect(doc, &QTextDocument::contentsChange, this, &TextFormatter::onContentsChange);

    emit documentChanged();
    emit formatChanged();
}

int TextFormatter::selectionStart() const
{
    return m_selectionStart;
}

void TextFormatter::setSelectionStart(int position)
{
    if (position == m_selectionStart)
        return;
    m_selectionStart = position;
    if (position != m_pendingPosition)
        m_pendingPosition = -1;
    emit selectionChanged();
    emit formatChanged();
}

int TextFormatter::selectionEnd() const
{
    return m_selectionEnd;
}

void TextFormatter::setSelectionEnd(int position)
{
    if (position == m_selectionEnd)
        return;
    m_selectionEnd = position;
    if (position != m_pendingPosition)
        m_pendingPosition = -1;
    emit selectionChanged();
    emit formatChanged();
}

bool TextFormatter::bold() const
{
    return charFormat().fontWeight() >= QFont::Bold;
}

void TextFormatter::setBold(bool bold)
{
    QTextCharFormat format;
    format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
    mergeCharFormat(format);
}

bool TextFormatter::italic() const
{
    return charFormat().fontItalic();
}

void TextFormatter::setItalic(bool italic)
{
    QTextCharFormat format;
    format.setFontItalic(italic);
    mergeCharFormat(format);
}

int TextFormatter::headingLevel() const
{
    const QTextCursor c = cursor();
    return c.isNull() ? 0 : c.blockFormat().headingLevel();
}

void TextFormatter::setHeadingLevel(int level)
{
    QTextCursor c = cursor();
    if (c.isNull())
        return;
    level = std::clamp(level, 0, 6);

    // Extend the selection to whole blocks so the heading font covers all of
    // their text, not just the selected part.
    const int end = c.selectionEnd();
    c.setPosition(c.selectionStart());
    c.movePosition(QTextCursor::StartOfBlock);
    c.setPosition(end, QTextCursor::KeepAnchor);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QTextBlockFormat blockFormat;
    blockFormat.setHeadingLevel(level);
    // Matches the format QTextDocument::setMarkdown() gives headings, so a
    // heading looks the same before and after the note is reloaded. The
    // Markdown writer doesn't emit ** for bold inside headings.
    QTextCharFormat charFormat;
    charFormat.setFontWeight(level > 0 ? QFont::Bold : QFont::Normal);
    charFormat.setProperty(QTextFormat::FontSizeAdjustment, level > 0 ? 4 - level : 0);

    c.beginEditBlock();
    c.mergeBlockFormat(blockFormat);
    c.mergeCharFormat(charFormat);
    c.mergeBlockCharFormat(charFormat);
    c.endEditBlock();
    emit formatChanged();
}

TextFormatter::ListStyle TextFormatter::listStyle() const
{
    const QTextCursor c = cursor();
    const QTextList *list = c.isNull() ? nullptr : c.currentList();
    if (!list)
        return NoList;
    switch (list->format().style()) {
    case QTextListFormat::ListDisc:
    case QTextListFormat::ListCircle:
    case QTextListFormat::ListSquare:
        return BulletList;
    default:
        return NumberedList;
    }
}

void TextFormatter::setListStyle(ListStyle style)
{
    QTextCursor c = cursor();
    if (c.isNull())
        return;

    if (style == NoList) {
        // Merging can't clear a property, so each block's format is replaced
        // with a copy that isn't in a list. QTextList::remove() isn't used
        // because it indents the block to where the list item was.
        QTextDocument *doc = textDocument();
        const QTextBlock last = doc->findBlock(c.selectionEnd());
        c.beginEditBlock();
        for (QTextBlock block = doc->findBlock(c.selectionStart()); block.isValid();
             block = block.next()) {
            if (block.textList()) {
                QTextBlockFormat format = block.blockFormat();
                format.setObjectIndex(-1);
                format.setIndent(0);
                QTextCursor(block).setBlockFormat(format);
            }
            if (block == last)
                break;
        }
        c.endEditBlock();
    } else {
        const auto listStyle = style == BulletList ? QTextListFormat::ListDisc
                                                   : QTextListFormat::ListDecimal;
        // Switching the style of an existing list changes the whole list,
        // like most editors do.
        if (QTextList *list = c.currentList()) {
            QTextListFormat format = list->format();
            format.setStyle(listStyle);
            list->setFormat(format);
        } else {
            c.createList(listStyle);
        }
    }
    emit formatChanged();
}

bool TextFormatter::handleReturn()
{
    QTextCursor c = cursor();
    if (c.isNull() || c.hasSelection())
        return false;

    const QTextBlock block = c.block();
    if (block.textList() && block.length() == 1) {
        setListStyle(NoList);
        return true;
    }

    if (block.blockFormat().headingLevel() > 0 && c.atBlockEnd()) {
        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.setHeadingLevel(0);
        QTextCharFormat charFormat = c.blockCharFormat();
        charFormat.setFontWeight(QFont::Normal);
        charFormat.setProperty(QTextFormat::FontSizeAdjustment, 0);
        c.insertBlock(blockFormat, charFormat);
        return true;
    }

    return false;
}

void TextFormatter::resetTypingFormat()
{
    const QTextCursor c = cursor();
    if (c.isNull())
        return;
    // For a cursor with no selection, this is the format of the character
    // before it, or after it at the start of a block.
    m_pendingFormat = c.charFormat();
    m_pendingPosition = c.position();
    emit formatChanged();
}

QTextDocument *TextFormatter::textDocument() const
{
    return m_document ? m_document->textDocument() : nullptr;
}

QTextCursor TextFormatter::cursor() const
{
    QTextDocument *doc = textDocument();
    if (!doc)
        return {};

    // The bindings from the editor can briefly be out of range while a note
    // is being replaced.
    const int last = doc->characterCount() - 1;
    QTextCursor c(doc);
    c.setPosition(std::clamp(m_selectionStart, 0, last));
    c.setPosition(std::clamp(m_selectionEnd, 0, last), QTextCursor::KeepAnchor);
    return c;
}

QTextCharFormat TextFormatter::charFormat() const
{
    const QTextCursor c = cursor();
    if (c.isNull())
        return {};
    if (!c.hasSelection() && c.position() == m_pendingPosition)
        return m_pendingFormat;
    return c.charFormat();
}

void TextFormatter::mergeCharFormat(const QTextCharFormat &format)
{
    QTextCursor c = cursor();
    if (c.isNull())
        return;

    if (c.hasSelection()) {
        c.mergeCharFormat(format);
    } else {
        if (c.position() != m_pendingPosition)
            m_pendingFormat = c.charFormat();
        m_pendingFormat.merge(format);
        m_pendingPosition = c.position();
    }
    emit formatChanged();
}

void TextFormatter::onContentsChange(int position, int charsRemoved, int charsAdded)
{
    // Typing inserts text without removing any. Anything else, such as
    // loading a different note, discards the pending format. The range check
    // skips the change QTextDocument::clear() reports while it is resetting.
    if (m_pendingPosition >= 0) {
        const QTextCharFormat pending = m_pendingFormat;
        const bool typedAtPending = position == m_pendingPosition && charsRemoved == 0
                && charsAdded > 0
                && position + charsAdded < textDocument()->characterCount();
        m_pendingFormat = {};
        m_pendingPosition = -1;
        if (typedAtPending) {
            QTextCursor c(textDocument());
            c.setPosition(position);
            c.setPosition(position + charsAdded, QTextCursor::KeepAnchor);
            c.setCharFormat(pending);
        }
    }
    emit formatChanged();
}
