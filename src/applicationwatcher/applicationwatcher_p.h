#ifndef APPLICATION_WATCHER_PRIVATE_INCLUDED
#define APPLICATION_WATCHER_PRIVATE_INCLUDED

#include <QObject>
#include <QFileSystemModel>
#include <QVector>

class ApplicationWatcherPrivate : public QObject {
	Q_OBJECT
	struct Entry {
		QString title,
			path;
		
	};
public:
	ApplicationWatcherPrivate(const QString&, QObject *parent = nullptr);
	~ApplicationWatcherPrivate();
private Q_SLOTS:
	void handleRowsInserted(const QModelIndex&, int, int);
	void handleRowsRemoved(const QModelIndex&, int, int);
Q_SIGNALS:
	void insertEntry(const QString &title, const QString &path);
	void clearMenuRequested();
private:
	QVector<Entry> m_entries;	
	QFileSystemModel *m_fsModel;
};

#endif // APPLICATION_WATCHER_PRIVATE_INCLUDED
