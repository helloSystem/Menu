#include <QFileInfo>
#include "applicationwatcher_p.h"

ApplicationWatcherPrivate::ApplicationWatcherPrivate(const QString &rootPath, QObject *parent)
	: QObject(parent) {
	m_fsModel = new QFileSystemModel(this);
	connect(m_fsModel, &QFileSystemModel::rowsInserted, this, &ApplicationWatcherPrivate::handleRowsInserted);
	connect(m_fsModel, &QFileSystemModel::rowsRemoved, this, &ApplicationWatcherPrivate::handleRowsRemoved);
	m_fsModel->setRootPath(rootPath);
}

ApplicationWatcherPrivate::~ApplicationWatcherPrivate() {
	m_fsModel->deleteLater();
}

void ApplicationWatcherPrivate::handleRowsInserted(const QModelIndex &parentObj, int first, int last) {
	for(auto i = first; i <= last; ++i) {
		auto t = parentObj.child(i, /*column=*/0);
		auto fileName = t.data().toString();
		if(!fileName.toLower().endsWith(".app") && !fileName.toLower().endsWith(".appdir")) {
			continue;
		}

		// The correct way to get the right name is to inspect some kind of metadata 
		// in the AppDir or .app bundle, it's virtually impossible to find the Application
		// name from filenames and can even be misleading sometimes.
		QFileInfo fi(t.data().toString());
                QString base = fi.completeBaseName(); 
		Entry e;
		e.title = base;
	        e.path = m_fsModel->rootPath() + "/" + t.data().toString();
		m_entries.append(e);
		emit insertEntry(e.title, e.path);
	}

}

void ApplicationWatcherPrivate::handleRowsRemoved(const QModelIndex &parentObj, int first, int last) {
	/// This could be optimized more.
	/// But the catch is once the entry is deleted from the filesystem we can't get the name 
	/// of the file since it's already deleted so we should search for what's deleted and then
	/// remove that and re-construct the whole menu.
	/// I think there could be something we could do here.

	(void)first;
	(void)last;
	(void)parentObj;	

	/// Validate the entries and the filesystem and remove all that 
	/// does not exist in the Filesystem.
	for(auto i = 0; i < m_entries.size(); ++i) {
		auto e = m_entries.at(i);
		if(!QFileInfo::exists(e.path)) {
			m_entries.removeAt(i);
		}		
	}

	/// After removing the entry, reconstruct the menu.
	emit clearMenuRequested();
	for(auto i = 0; i < m_entries.size(); ++i) {
		auto e = m_entries.at(i);
		emit insertEntry(e.title, e.path);
	}
}
