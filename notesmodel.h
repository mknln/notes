#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QDir>
#include <QSet>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// List of notes, most recently modified first. Each note is stored as a
// Markdown file named <uuid>.md in the app data directory. Plain-text .txt
// notes from older versions are converted to Markdown when loaded.
//
// Edits are kept in memory and written to disk in batches: the first edit
// after a save starts a short timer, and every note changed in the meantime
// is written when it fires. Typing therefore never waits on disk I/O, and at
// most one save interval of edits can be lost on a crash.
class NotesModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        NoteIdRole = Qt::UserRole + 1,
        TitleRole,
        PreviewRole,
        ModifiedRole,
    };

    explicit NotesModel(QObject *parent = nullptr);
    ~NotesModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString storagePath() const;

    Q_INVOKABLE QString createNote();
    Q_INVOKABLE void removeNote(const QString &id);
    Q_INVOKABLE QString text(const QString &id) const;
    Q_INVOKABLE void setText(const QString &id, const QString &text);
    Q_INVOKABLE QString title(const QString &id) const;
    Q_INVOKABLE int indexOf(const QString &id) const;
    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE void flush();

signals:
    void countChanged();

private:
    struct Note
    {
        QString id;
        QString text; // Markdown
        QString plainText; // For the title and preview
        QDateTime modified;
    };

    void load();
    bool save(const Note &note) const;
    void migrate(const QFileInfo &info);
    QString filePath(const QString &id) const;

    QList<Note> m_notes;
    QDir m_dir;
    QSet<QString> m_dirty;
    QTimer m_saveTimer;
};
