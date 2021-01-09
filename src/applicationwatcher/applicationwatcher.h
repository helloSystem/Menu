#ifndef APPLICATION_WATCHER_H_INCLUDED
#define APPLICATION_WATCHER_H_INCLUDED

#include <QObject>
#include <QThread>

class ApplicationWatcherPrivate;
class ApplicationWatcher : public QObject {
	Q_OBJECT
public:
	ApplicationWatcher(const QString&, QObject *parent = nullptr);
	~ApplicationWatcher();
Q_SIGNALS:
	void insertEntry(const QString &title, const QString &path);
	void clearMenuRequested();
private:
	QThread *m_workerThread;
	ApplicationWatcherPrivate *m_privateImpl;
};

#endif // APPLICATION_WATCHER_H_INCLUDED
