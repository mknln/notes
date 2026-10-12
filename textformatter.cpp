#include "textformatter.h"

#include <QFontInfo>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextList>

namespace {

// The nesting level of a list item, starting at 1, or 0 if the block isn't in
// a list (or isn't valid).
int listLevel(const QTextBlock &block)
{
    const QTextList *list = block.textList();
    return list ? list->format().indent() : 0;
}

// The bullet for a bulleted list at `level`: a disc, then a circle, then a
// square, repeating, as in most word processors. The Markdown writer marks
// each with a different character (-, * and +) and the reader maps them back,
// so a note keeps its bullets when it's reloaded.
QTextListFormat::Style bulletStyle(int level)
{
    static const QList<QTextListFormat::Style> listStyles = {
        QTextListFormat::ListDisc,
        QTextListFormat::ListCircle,
        QTextListFormat::ListSquare,
    };
    return listStyles.at((qMax(level, 1) - 1) % listStyles.count());
}

bool isBulletStyle(QTextListFormat::Style style)
{
    return style == QTextListFormat::ListDisc || style == QTextListFormat::ListCircle
            || style == QTextListFormat::ListSquare;
}

// Whether `block` is part of the list whose top-level list is `root`: either
// one of its top-level items, or nested. Only meaningful while walking
// through consecutive list items.
bool inTree(const QTextBlock &block, const QTextList *root)
{
    const int level = listLevel(block);
    return level > 1 || (level == 1 && block.textList() == root);
}

// The first item of the list, including nested lists, that `block` is in.
QTextBlock treeStart(QTextBlock block)
{
    while (listLevel(block) > 1 && listLevel(block.previous()) > 0)
        block = block.previous();
    const QTextList *root = block.textList();
    while (inTree(block.previous(), root))
        block = block.previous();
    return block;
}

// Puts a list item at `level` in the list its position calls for. That's the
// list of the nearest earlier item at that level, unless an item at a
// shallower level comes first, in which case the item starts a new list,
// with `style`, or the bullet for `level` if `style` is a bullet.
void relist(const QTextBlock &block, int level, QTextListFormat::Style style)
{
    for (QTextBlock prev = block.previous(); listLevel(prev) >= level; prev = prev.previous()) {
        if (listLevel(prev) == level) {
            if (prev.textList() != block.textList())
                prev.textList()->add(block);
            return;
        }
    }

    const QTextList *current = block.textList();
    if (current && listLevel(block) == level && current->itemNumber(block) == 0)
        return;
    QTextListFormat format;
    format.setStyle(isBulletStyle(style) ? bulletStyle(level) : style);
    format.setIndent(level);
    QTextCursor(block).createList(format);
}

// Re-indents the list items in `markdown`, which Qt wrote from `doc`, so each
// item starts at its parent item's text column. Qt indents an item by its own
// marker's width per level instead, which only works when every level has the
// same kind of marker. The level of each item comes from the document, and an
// item's continuation lines move with it. If the Markdown doesn't line up
// with the document's list items, it's returned unchanged.
QString fixListIndentation(const QTextDocument *doc, const QString &markdown)
{
    QList<int> levels;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (block.textList())
            levels.append(listLevel(block));
    }
    if (levels.isEmpty())
        return markdown;

    // A marker and the spaces after it. Text that would look like one is
    // escaped by the writer, e.g. "1\.".
    static const QRegularExpression itemStart(QStringLiteral("^( *)((?:[-*+]|\\d+[.)])(?: +|$))"));
    static const QRegularExpression fence(QStringLiteral("^ *(```|~~~)"));

    QStringList lines = markdown.split(u'\n');
    QList<int> textColumns; // Of the current item at each level
    int item = 0;
    int shift = 0; // How far the current item's lines move
    bool inFence = false;
    for (QString &line : lines) {
        const bool isFence = fence.match(line).hasMatch();
        const QRegularExpressionMatch match = inFence || isFence ? QRegularExpressionMatch()
                                                                 : itemStart.match(line);
        if (isFence)
            inFence = !inFence;

        qsizetype indent = 0;
        while (indent < line.size() && line.at(indent) == u' ')
            ++indent;
        if (match.hasMatch()) {
            if (item == levels.size())
                return markdown;
            const int level = levels.at(item++);
            if (level > textColumns.size() + 1)
                return markdown;
            textColumns.resize(level - 1);
            const int newIndent = textColumns.isEmpty() ? 0 : textColumns.last();
            shift = newIndent - int(match.capturedLength(1));
            textColumns.append(newIndent + int(match.capturedLength(2)));
        } else if (line.trimmed().isEmpty()) {
            continue;
        } else if (indent == 0) {
            // Unindented text ends the list.
            textColumns.clear();
            shift = 0;
            continue;
        }

        if (shift > 0)
            line.prepend(QString(shift, u' '));
        else if (shift < 0)
            line.remove(0, qMin(qsizetype(-shift), indent));
    }
    return item == levels.size() ? lines.join(u'\n') : markdown;
}

// Characters that are written as backslash escapes wherever they're literal
// text, not code. Qt's Markdown writer escapes some characters only in some
// places, so they could be read back as formatting: it doesn't escape ~ or _
// at all, so "~/notes and ~/docs" would come back with "/notes and " struck
// through, and it only escapes the first * of "**a**". Escaping these
// everywhere covers emphasis, strikethrough, code spans, HTML, links and
// images, entities and block quotes.
constexpr QStringView escapedChars = u"~_*`<>[&";

// While the document is written, each of escapedChars in its text is
// replaced with a private-use character, which the writer leaves alone, and
// the placeholders are then replaced with escapes.
constexpr char16_t firstPlaceholder = 0xE000;

// Whether text with `format` is written as a code span, where escapes would
// be literal. Matches the writer's test.
bool isCode(const QTextCharFormat &format)
{
    return format.fontFixedPitch() || QFontInfo(format.font()).fixedPitch();
}

// Whether `block` is written as a code block, where escapes would be literal.
bool isCode(const QTextBlock &block)
{
    const QTextBlockFormat format = block.blockFormat();
    return format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence)
            || format.hasProperty(QTextFormat::BlockCodeLanguage);
}

// Whether `doc` has text that needs escapes written, and can have them: if
// it already has any of the placeholder characters, they would turn into
// escapes too.
bool needsEscapes(const QTextDocument *doc)
{
    const QString text = doc->toPlainText();
    bool found = false;
    for (const QChar ch : text) {
        if (ch.unicode() >= firstPlaceholder && ch.unicode() < firstPlaceholder + escapedChars.size())
            return false;
        found = found || escapedChars.contains(ch);
    }
    return found;
}

// Replaces each of escapedChars in the text of `doc`, other than in code,
// with its placeholder.
void insertPlaceholders(QTextDocument *doc)
{
    struct Replacement
    {
        int position;
        QChar placeholder;
        QTextCharFormat format;
    };
    // Collected first, because replacing text can merge the fragments being
    // walked through.
    QList<Replacement> replacements;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (isCode(block))
            continue;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (isCode(fragment.charFormat()))
                continue;
            const QString text = fragment.text();
            for (qsizetype i = 0; i < text.size(); ++i) {
                const qsizetype index = escapedChars.indexOf(text.at(i));
                if (index >= 0)
                    replacements.append({ fragment.position() + int(i),
                                          QChar(char16_t(firstPlaceholder + index)),
                                          fragment.charFormat() });
            }
        }
    }

    QTextCursor c(doc);
    for (const Replacement &replacement : std::as_const(replacements)) {
        c.setPosition(replacement.position);
        c.setPosition(replacement.position + 1, QTextCursor::KeepAnchor);
        c.insertText(replacement.placeholder, replacement.format);
    }
}

// Replaces the placeholders from insertPlaceholders() in `markdown` with
// escapes.
QString replacePlaceholders(QString markdown)
{
    for (qsizetype i = 0; i < escapedChars.size(); ++i) {
        const QChar escaped[] = { u'\\', escapedChars.at(i) };
        markdown.replace(QChar(char16_t(firstPlaceholder + i)), QString(escaped, 2));
    }
    return markdown;
}

// The character format of a heading at `level`, or of body text for 0.
// Matches the format QTextDocument::setMarkdown() gives headings, so a heading
// looks the same before and after the note is reloaded. The Markdown writer
// doesn't emit ** for bold inside headings.
QTextCharFormat headingCharFormat(int level)
{
    QTextCharFormat format;
    format.setFontWeight(level > 0 ? QFont::Bold : QFont::Normal);
    format.setProperty(QTextFormat::FontSizeAdjustment, level > 0 ? 4 - level : 0);
    return format;
}

// Makes the blocks the cursor's selection is in headings at `level`, or body
// text for 0.
void applyHeading(QTextCursor c, int level)
{
    // Extend the selection to whole blocks so the heading font covers all of
    // their text, not just the selected part.
    const int end = c.selectionEnd();
    c.setPosition(c.selectionStart());
    c.movePosition(QTextCursor::StartOfBlock);
    c.setPosition(end, QTextCursor::KeepAnchor);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QTextBlockFormat blockFormat;
    blockFormat.setHeadingLevel(level);
    const QTextCharFormat charFormat = headingCharFormat(level);
    c.mergeBlockFormat(blockFormat);
    c.mergeCharFormat(charFormat);
    c.mergeBlockCharFormat(charFormat);
}

// Whether the first block isn't formatted as a heading at `level`. Its text
// can lose the heading font even if the block is still a heading, e.g. when
// the next line is joined to it.
bool needsTitleFormat(const QTextDocument *doc, int level)
{
    const QTextBlock block = doc->begin();
    if (block.blockFormat().headingLevel() != level || block.textList())
        return true;

    const QTextCharFormat heading = headingCharFormat(level);
    const auto matches = [&heading](const QTextCharFormat &format) {
        return format.fontWeight() == heading.fontWeight()
                && format.intProperty(QTextFormat::FontSizeAdjustment)
                == heading.intProperty(QTextFormat::FontSizeAdjustment);
    };
    // The block's own character format is only used while it's empty, and
    // QTextDocument::setMarkdown() doesn't set it for headings.
    if (block.length() == 1 && !matches(block.charFormat()))
        return true;
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        if (!matches(it.fragment().charFormat()))
            return true;
    }
    return false;
}

// Formats the first block as a heading at `level`, taking it out of a list.
void applyTitleFormat(QTextDocument *doc, int level)
{
    const QTextBlock block = doc->begin();
    QTextCursor c(block);
    if (block.textList()) {
        QTextBlockFormat format = block.blockFormat();
        format.setObjectIndex(-1);
        format.setIndent(0);
        c.setBlockFormat(format);
    }
    applyHeading(c, level);
}

} // namespace

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
    if (QTextDocument *doc = textDocument()) {
        connect(doc, &QTextDocument::contentsChange, this, &TextFormatter::onContentsChange);
        connect(doc, &QTextDocument::undoCommandAdded, this, &TextFormatter::onUndoCommandAdded);
    }

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

bool TextFormatter::strikeOut() const
{
    return charFormat().fontStrikeOut();
}

void TextFormatter::setStrikeOut(bool strikeOut)
{
    QTextCharFormat format;
    format.setFontStrikeOut(strikeOut);
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

    c.beginEditBlock();
    applyHeading(c, qBound(0, level, 6));
    c.endEditBlock();
    emit formatChanged();
}

int TextFormatter::titleHeadingLevel() const
{
    return m_titleHeadingLevel;
}

void TextFormatter::setTitleHeadingLevel(int level)
{
    level = qBound(0, level, 6);
    if (level == m_titleHeadingLevel)
        return;
    m_titleHeadingLevel = level;
    emit titleHeadingLevelChanged();
    emit formatChanged();
}

bool TextFormatter::onTitleLine() const
{
    const QTextCursor c = cursor();
    return m_titleHeadingLevel > 0 && !c.isNull()
            && textDocument()->findBlock(c.selectionStart()).blockNumber() == 0;
}

TextFormatter::ListStyle TextFormatter::listStyle() const
{
    const QTextCursor c = cursor();
    const QTextList *list = c.isNull() ? nullptr : c.currentList();
    if (!list)
        return NoList;
    return isBulletStyle(list->format().style()) ? BulletList : NumberedList;
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
        // Switching the style of an existing list changes all of its items
        // at that level, like most editors do, but not the lists nested in it
        // or the list it's nested in. A new list is at the top level.
        QTextList *list = c.currentList();
        const int level = list ? list->format().indent() : 1;
        const auto listStyle = style == BulletList ? bulletStyle(level)
                                                   : QTextListFormat::ListDecimal;
        if (list) {
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
        if (listLevel(block) > 1)
            shiftListLevel(-1);
        else
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
        // The editor can keep typing with the heading's format, e.g. after
        // an empty heading.
        m_pendingFormat = charFormat;
        m_pendingPosition = c.position();
        return true;
    }

    return false;
}

bool TextFormatter::shiftListLevel(int delta)
{
    QTextCursor c = cursor();
    if (c.isNull())
        return false;
    QTextDocument *doc = textDocument();
    const QTextBlock first = doc->findBlock(c.selectionStart());
    if (!first.textList())
        return false;
    const QTextList *root = treeStart(first).textList();

    // The selected items, plus the items nested under them, which move with
    // them.
    const int lastSelected = doc->findBlock(c.selectionEnd()).blockNumber();
    QList<QTextBlock> blocks;
    int minLevel = listLevel(first);
    for (QTextBlock block = first; inTree(block, root); block = block.next()) {
        if (block.blockNumber() > lastSelected && listLevel(block) <= minLevel)
            break;
        blocks.append(block);
        minLevel = qMin(minLevel, listLevel(block));
    }

    if (delta > 0) {
        // Nesting needs an earlier item at the same level to nest under.
        QTextBlock prev = first.previous();
        while (listLevel(prev) > listLevel(first))
            prev = prev.previous();
        if (prev.textList() != first.textList())
            return true;
    } else if (minLevel + delta < 1) {
        return true;
    }

    c.beginEditBlock();
    for (const QTextBlock &block : std::as_const(blocks))
        relist(block, listLevel(block) + delta, block.textList()->format().style());
    // The items after the moved ones may now belong to a different list, e.g.
    // ones that were after an item that moved up a level are now nested in it
    // and should be numbered from 1.
    for (QTextBlock block = blocks.last().next(); listLevel(block) > 1; block = block.next())
        relist(block, listLevel(block), block.textList()->format().style());
    c.endEditBlock();

    emit formatChanged();
    return true;
}

bool TextFormatter::handleSpace()
{
    QTextCursor c = cursor();
    if (c.isNull() || c.hasSelection() || c.currentList() || c.blockFormat().headingLevel() > 0)
        return false;

    const QTextBlock block = c.block();
    const QString marker = block.text().left(c.positionInBlock());
    QTextListFormat::Style style;
    if (marker == u"*" || marker == u"-")
        style = QTextListFormat::ListDisc;
    else if (marker == u"1.")
        style = QTextListFormat::ListDecimal;
    else
        return false;

    // The space is typed as its own undo step, so undoing the list leaves the
    // text as typed.
    c.insertText(QStringLiteral(" "));
    c.beginEditBlock();
    // qInfo() << "block position is" << block.position();
    c.setPosition(block.position());
    c.setPosition(block.position() + marker.size() + 1, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.createList(style);
    c.endEditBlock();

    emit formatChanged();
    return true;
}

void TextFormatter::insertText(const QString &text)
{
    QTextCursor c = cursor();
    if (!c.isNull())
        c.insertText(text);
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

bool TextFormatter::prepareLoadedText()
{
    QTextDocument *doc = textDocument();
    if (!doc)
        return false;

    bool changed = false;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        // Notes written before bullets varied by level have discs at every
        // level.
        if (QTextList *list = block.textList(); list && list->item(0) == block) {
            QTextListFormat format = list->format();
            const auto style = bulletStyle(format.indent());
            if (isBulletStyle(format.style()) && format.style() != style) {
                format.setStyle(style);
                list->setFormat(format);
                changed = true;
            }
        }
        if (block.text() == QChar(QChar::Nbsp) && !block.textList()) {
            QTextCursor c(block);
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            c.removeSelectedText();
            changed = true;
        }
    }
    if (m_titleHeadingLevel > 0 && needsTitleFormat(doc, m_titleHeadingLevel)) {
        applyTitleFormat(doc, m_titleHeadingLevel);
        changed = true;
    }
    if (!changed)
        return false;

    // Clearing the undo history afterwards keeps this from being undone; it
    // only holds the text being replaced anyway.
    doc->clearUndoRedoStacks();
    emit formatChanged();
    return true;
}

void TextFormatter::onUndoCommandAdded()
{
    // Reformatting the first line as part of the edit that changed it means
    // undoing the edit undoes both. Undo and redo don't add commands, so they
    // don't end up here and can restore any state.
    QTextDocument *doc = textDocument();
    if (m_titleHeadingLevel == 0 || !needsTitleFormat(doc, m_titleHeadingLevel))
        return;

    QTextCursor c(doc);
    c.joinPreviousEditBlock();
    applyTitleFormat(doc, m_titleHeadingLevel);
    c.endEditBlock();
    emit formatChanged();
}

QString TextFormatter::markdown() const
{
    const QTextDocument *doc = textDocument();
    if (!doc)
        return {};

    // Empty lines after the last text aren't kept.
    const auto isEmptyLine = [](const QTextBlock &block) {
        return block.length() == 1 && !block.textList();
    };
    int lastText = -1;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (!isEmptyLine(block))
            lastText = block.blockNumber();
    }

    // Empty lines and escaped characters are changed in a copy, so the editor
    // isn't affected.
    QScopedPointer<QTextDocument> copy;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (!isEmptyLine(block))
            continue;
        const bool keep = block.blockNumber() < lastText;
        if (!keep && block.blockFormat().headingLevel() == 0)
            continue;
        if (!copy)
            copy.reset(doc->clone());
        const QTextBlock copied = copy->findBlockByNumber(block.blockNumber());
        QTextCursor c(copied);
        QTextBlockFormat format = copied.blockFormat();
        format.setHeadingLevel(0);
        c.setBlockFormat(format);
        if (keep)
            c.insertText(QString(QChar::Nbsp));
    }
    const bool escape = needsEscapes(doc);
    if (escape) {
        if (!copy)
            copy.reset(doc->clone());
        insertPlaceholders(copy.data());
    }
    if (copy)
        doc = copy.data();
    QString markdown = doc->toMarkdown();
    if (escape)
        markdown = replacePlaceholders(markdown);
    return fixListIndentation(doc, markdown);
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
    c.setPosition(qBound(0, m_selectionStart, last));
    c.setPosition(qBound(0, m_selectionEnd, last), QTextCursor::KeepAnchor);
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
