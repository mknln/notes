#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QDir>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// List of notes: pinned notes first, then the rest, each most recently
// modified first. Each note is stored as a Markdown file named <uuid>.md in
// the app data directory. Plain-text .txt notes from older versions are
// converted to Markdown when loaded. The ids of pinned notes are stored in
// pinned.json in the same directory, so they stay with the notes.
//
// Edits are kept in memory and written to disk in batches: the first edit
// after a save starts a short timer, and every note changed in the meantime
// is written when it fires. Typing therefore never waits on disk I/O, and at
// most one save interval of edits can be lost on a crash.
//
// All notes can be exported to a zip file and imported from one: see
// exportNotes() and importNotes().
class NotesModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QStringList pinnedIds READ pinnedIds NOTIFY pinnedIdsChanged)

public:
    enum Roles {
        NoteIdRole = Qt::UserRole + 1,
        TitleRole,
        PreviewRole,
        ModifiedRole,
        PinnedRole,
    };

    explicit NotesModel(QObject *parent = nullptr);
    ~NotesModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString storagePath() const;
    QStringList pinnedIds() const;

    Q_INVOKABLE QString createNote();
    Q_INVOKABLE void removeNote(const QString &id);
    Q_INVOKABLE QString text(const QString &id) const;
    Q_INVOKABLE void setText(const QString &id, const QString &text);
    Q_INVOKABLE QString title(const QString &id) const;
    Q_INVOKABLE int indexOf(const QString &id) const;
    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE void setPinned(const QString &id, bool pinned);
    Q_INVOKABLE void flush();

    // Writes every note to a zip file, one Markdown file per note named after
    // its title, plus a manifest.json recording each file's note id,
    // modification time and pinned state. Returns an error message, or an
    // empty string on success.
    Q_INVOKABLE QString exportNotes(const QUrl &file) const;

    // Adds the notes in a zip file written by exportNotes(). A note that's
    // already here is only replaced if the zip's copy is newer and differs.
    // Zips from elsewhere work too: each .md file becomes a new note, with
    // the modification time it has in the zip. Returns a map with the counts
    // "added", "updated" and "skipped" (already up to date), or with an
    // "error" message if the file couldn't be read or had no notes.
    Q_INVOKABLE QVariantMap importNotes(const QUrl &file);

signals:
    void countChanged();
    void pinnedIdsChanged();
    // importNotes() replaced the note's text, so an editor showing it should
    // load it again.
    void noteReplaced(const QString &id);

private:
    struct Note
    {
        QString id;
        QString text; // Markdown
        QString plainText; // For the title and preview
        QDateTime modified;
        bool pinned = false;
    };

    static QString titleOf(const Note &note);
    static bool comesBefore(const Note &a, const Note &b);
    int sortedRow(const Note &note, int ignoredRow = -1) const;
    int moveToSortedRow(int row);

    void load();
    bool save(const Note &note) const;
    void keepModifiedTime(const Note &note) const;
    void migrate(const QFileInfo &info);
    QSet<QString> loadPinned() const;
    void savePinned() const;
    QString filePath(const QString &id) const;

    QList<Note> m_notes;
    QDir m_dir;
    QSet<QString> m_dirty;
    QTimer m_saveTimer;
};
