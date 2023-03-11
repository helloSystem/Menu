#include <QDebug>

#include "actionsearch.h"

#include <QMenuBar>
#include <QMenu>
#include <QList>
#include <QAction>
#include <QDebug>

void ActionSearch::clear() {
	actions.clear();
}

void ActionSearch::update(QMenuBar *menu)
{
	for (auto menuAction: menu->actions())
	{
		QStringList names;
		readMenuActions(menuAction->menu(),names);

	}
}

void ActionSearch::execute(QString actionName)
{
	if (actions.contains(actionName)) {
		actions[actionName]->trigger();
	}
}

void ActionSearch::readMenuActions(QMenu* menu,QStringList names)
{
	// See https://doc.qt.io/qt-5/qaction.html#menu
	// If a QAction does not have menu in it, then
	// the pointer returned by QAction::menu will be 
	// null.
	if (!menu)
		return;

	names << menu->title();
	for (auto action: menu->actions())
	{

		if (action->menu() != NULL)
		{

			readMenuActions(action->menu(),names);
		}


		QString actionName = action->text().replace("&", "");
		if (!actionName.isEmpty() && action->isEnabled() && !action->menu())
		{
            actionName = names.join(" → ") + " → " + actionName;

            if (action->shortcut().toString() != "") {
                actionName = actionName + " (" + action->shortcut().toString() +")";
            }
			actions.insert(actionName, action);
		}

	}
	names.clear();

}

