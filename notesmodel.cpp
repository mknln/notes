#include "notesmodel.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextDocument>
#include <QUuid>

#include <algorithm>

using namespace std::chrono_literals;

namespace {

// Returns the first `count` non-blank lines of `text`, trimmed. Stops scanning
// as soon as it has enough, so long notes stay cheap to summarize.
QStringList leadingLines(const QString &text, int count)
{
    QStringList lines;
    qsizetype start = 0;
    while (lines.size() < count && start <= text.size()) {
        qsizetype end = text.indexOf(u'\n', start);
        if (end < 0)
            end = text.size();
        const QString line = text.sliced(start, end - start).trimmed();
        if (!line.isEmpty())
            lines.append(line);
        start = end + 1;
    }
    return lines;
}

QString markdownToPlainText(const QString &markdown)
{
    QTextDocument doc;
    doc.setMarkdown(markdown);
    return doc.toPlainText();
}

} // namespace

NotesModel::NotesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_dir.setPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + QStringLiteral("/notes"));
    m_dir.mkpath(QStringLiteral("."));

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(750ms);
    connect(&m_saveTimer, &QTimer::timeout, this, &NotesModel::flush);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &NotesModel::flush);

    load();
}

NotesModel::~NotesModel()
{
    flush();
}

int NotesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_notes.size());
}

QVariant NotesModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const Note &note = m_notes.at(index.row());
    switch (role) {
    case NoteIdRole:
        return note.id;
    case Qt::DisplayRole:
    case TitleRole:
        return title(note.id);
    case PreviewRole: {
        const QStringList lines = leadingLines(note.plainText, 2);
        return lines.size() > 1 ? lines.at(1) : tr("No additional text");
    }
    case ModifiedRole:
        return note.modified;
    }
    return {};
}

QHash<int, QByteArray> NotesModel::roleNames() const
{
    return {
        { NoteIdRole, "noteId" },
        { TitleRole, "title" },
        { PreviewRole, "preview" },
        { ModifiedRole, "modified" },
    };
}

QString NotesModel::storagePath() const
{
    return m_dir.absolutePath();
}

QString NotesModel::createNote()
{
    Note note{ QUuid::createUuid().toString(QUuid::WithoutBraces), QString(), QString(),
               QDateTime::currentDateTime() };

    beginInsertRows({}, 0, 0);
    m_notes.prepend(note);
    endInsertRows();
    emit countChanged();

    save(note);
    return note.id;
}

void NotesModel::removeNote(const QString &id)
{
    const int row = indexOf(id);
    if (row < 0)
        return;

    beginRemoveRows({}, row, row);
    m_notes.removeAt(row);
    endRemoveRows();
    emit countChanged();

    m_dirty.remove(id);
    if (!QFile::remove(filePath(id)))
        qWarning("Could not delete note file %s", qPrintable(filePath(id)));
}

QString NotesModel::text(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? QString() : m_notes.at(row).text;
}

void NotesModel::setText(const QString &id, const QString &text)
{
    int row = indexOf(id);
    if (row < 0 || m_notes.at(row).text == text)
        return;

    // Like Mac Notes, the note being edited moves to the top of the list.
    if (row > 0) {
        beginMoveRows({}, row, row, {}, 0);
        m_notes.move(row, 0);
        endMoveRows();
        row = 0;
    }

    Note &note = m_notes[row];
    note.text = text;
    note.plainText = markdownToPlainText(text);
    note.modified = QDateTime::currentDateTime();
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { Qt::DisplayRole, TitleRole, PreviewRole, ModifiedRole });

    m_dirty.insert(id);
    // Don't restart a running timer: continuous typing still gets saved
    // every interval instead of only after a pause.
    if (!m_saveTimer.isActive())
        m_saveTimer.start();
}

QString NotesModel::title(const QString &id) const
{
    const int row = indexOf(id);
    if (row < 0)
        return {};
    const QStringList lines = leadingLines(m_notes.at(row).plainText, 1);
    return lines.isEmpty() ? tr("New Note") : lines.first();
}

int NotesModel::indexOf(const QString &id) const
{
    const auto it = std::find_if(m_notes.cbegin(), m_notes.cend(),
                                 [&id](const Note &note) { return note.id == id; });
    return it == m_notes.cend() ? -1 : int(it - m_notes.cbegin());
}

QString NotesModel::idAt(int row) const
{
    return row >= 0 && row < m_notes.size() ? m_notes.at(row).id : QString();
}

void NotesModel::flush()
{
    m_saveTimer.stop();
    for (const QString &id : std::as_const(m_dirty)) {
        const int row = indexOf(id);
        if (row >= 0)
            save(m_notes.at(row));
    }
    m_dirty.clear();
}

void NotesModel::load()
{
    const QFileInfoList oldFiles = m_dir.entryInfoList({ QStringLiteral("*.txt") }, QDir::Files);
    for (const QFileInfo &info : oldFiles)
        migrate(info);

    const QFileInfoList files = m_dir.entryInfoList({ QStringLiteral("*.md") }, QDir::Files);
    for (const QFileInfo &info : files) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("Could not read note file %s", qPrintable(info.absoluteFilePath()));
            continue;
        }
        const QString text = QString::fromUtf8(file.readAll());
        m_notes.append({ info.completeBaseName(), text, markdownToPlainText(text),
                         info.lastModified() });
    }

    std::sort(m_notes.begin(), m_notes.end(),
              [](const Note &a, const Note &b) { return a.modified > b.modified; });
}

bool NotesModel::save(const Note &note) const
{
    // QSaveFile writes to a temp file and renames it, so a crash mid-write
    // can't leave a truncated note behind.
    QSaveFile file(filePath(note.id));
    if (!file.open(QIODevice::WriteOnly) || file.write(note.text.toUtf8()) < 0 || !file.commit()) {
        qWarning("Could not save note %s: %s", qPrintable(note.id), qPrintable(file.errorString()));
        return false;
    }
    return true;
}

// Converts a plain-text note to Markdown, escaping anything that would
// otherwise be read as formatting. Keeps the modification time so the note
// list order doesn't change.
void NotesModel::migrate(const QFileInfo &info)
{
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Could not read note file %s", qPrintable(info.absoluteFilePath()));
        return;
    }
    QTextDocument doc;
    doc.setPlainText(QString::fromUtf8(file.readAll()));
    file.close();

    const Note note{ info.completeBaseName(), doc.toMarkdown(), QString(), info.lastModified() };
    if (!save(note))
        return;
    QFile saved(filePath(note.id));
    if (!saved.open(QIODevice::ReadWrite)
        || !saved.setFileTime(note.modified, QFileDevice::FileModificationTime))
        qWarning("Could not keep modification time of note %s", qPrintable(note.id));
    saved.close();
    if (!file.remove())
        qWarning("Could not remove migrated note file %s", qPrintable(file.fileName()));
}

QString NotesModel::filePath(const QString &id) const
{
    return m_dir.filePath(id + QStringLiteral(".md"));
}
