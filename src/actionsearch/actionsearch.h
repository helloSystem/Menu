#ifndef ACTIONSEARCH_H
#define ACTIONSEARCH_H

class QMenuBar;
class QMenu;
#include <QList>
#include <QHash>
class QAction;

class ActionSearch
{
	public:
		ActionSearch() {}
		~ActionSearch() {}
		void clear();
		void update(QMenuBar*);
		QList<QString> getActionNames() {return actions.keys();}
		void execute(QString actionName);

	private:
		QMenuBar* menuBar;

		void readMenuActions(QMenu* menu, QStringList names);

		QHash<QString, QAction*> actions{}; // std::unordered_map
};

#endif
