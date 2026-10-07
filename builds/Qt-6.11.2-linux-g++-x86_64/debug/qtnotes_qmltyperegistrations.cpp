/****************************************************************************
** Generated QML type registration code
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtQml/qqml.h>
#include <QtQml/qqmlmoduleregistration.h>

#if __has_include(<notesmodel.h>)
#  include <notesmodel.h>
#endif
#if __has_include(<textformatter.h>)
#  include <textformatter.h>
#endif


#if !defined(QT_STATIC)
#define Q_QMLTYPE_EXPORT Q_DECL_EXPORT
#else
#define Q_QMLTYPE_EXPORT
#endif
Q_QMLTYPE_EXPORT void qml_register_types_Notes()
{
    qmlRegisterModule("Notes", 254, 0);
    QT_WARNING_PUSH QT_WARNING_DISABLE_DEPRECATED
    qmlRegisterTypesAndRevisions<NotesModel>("Notes", 254);
    QMetaType::fromType<QAbstractItemModel *>().id();
    qmlRegisterEnum<QAbstractItemModel::LayoutChangeHint>("QAbstractItemModel::LayoutChangeHint");
    qmlRegisterEnum<QAbstractItemModel::CheckIndexOption>("QAbstractItemModel::CheckIndexOption");
    QMetaType::fromType<QAbstractListModel *>().id();
    qmlRegisterTypesAndRevisions<TextFormatter>("Notes", 254);
    qmlRegisterEnum<TextFormatter::ListStyle>("TextFormatter::ListStyle");
    QT_WARNING_POP
    qmlRegisterModule("Notes", 254, 254);
}

static const QQmlModuleRegistration notesRegistration("Notes", qml_register_types_Notes);
