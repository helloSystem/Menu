#include "applicationwatcher.h"
#include "applicationwatcher_p.h"

ApplicationWatcher::ApplicationWatcher(const QString &rootPath, QObject *parent)
	: QObject(parent) {
	m_workerThread = new QThread;
	m_workerThread->start();

	m_privateImpl = new ApplicationWatcherPrivate(rootPath);
	m_privateImpl->moveToThread(m_workerThread);	

	connect(m_privateImpl, &ApplicationWatcherPrivate::insertEntry,
		this, &ApplicationWatcher::insertEntry,
		Qt::DirectConnection);
	connect(m_privateImpl, &ApplicationWatcherPrivate::clearMenuRequested,
		this, &ApplicationWatcher::clearMenuRequested,
		Qt::DirectConnection);
}

ApplicationWatcher::~ApplicationWatcher() {
	m_workerThread->quit();
	m_workerThread->wait();

	m_privateImpl->deleteLater();
	m_workerThread->deleteLater();
}
