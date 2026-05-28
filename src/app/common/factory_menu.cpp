/***

  Olive - Non-Linear Video Editor
  Copyright (C) 2022 Olive Team
  Modifications Copyright (C) 2025 mikesolar

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#include "node/factory.h"

#include <QCoreApplication>

#include "widget/menu/menu.h"

namespace olive
{

Menu *NodeFactory::CreateMenu(QWidget *parent, bool create_none_item,
							  Node::CategoryID restrict_to,
							  uint64_t restrict_flags)
{
	Menu *menu = new Menu(parent);
	menu->setToolTipsVisible(true);

	if (create_none_item) {
		QAction *none_action = menu->addAction(
			QCoreApplication::translate("NodeFactory", "None"));
		none_action->setData(-1);
		menu->addSeparator();
	}

	for (int i = 0; i < library_.size(); i++) {
		Node *n = library_.at(i);

		if (restrict_to != Node::kCategoryUnknown &&
			!n->Category().contains(restrict_to)) {
			continue;
		}

		if (restrict_flags && !(n->GetFlags() & restrict_flags)) {
			continue;
		}

		if (n->GetFlags() & Node::kDontShowInCreateMenu) {
			continue;
		}

		n->Retranslate();

		Menu *destination = nullptr;
		QString category_name = Node::GetCategoryName(
			n->Category().isEmpty() ? Node::kCategoryUnknown
									: n->Category().first());

		QList<QAction *> menu_actions = menu->actions();
		foreach (QAction *action, menu_actions) {
			if (action->menu() && action->menu()->title() == category_name) {
				destination = static_cast<Menu *>(action->menu());
				break;
			}
		}

		if (!destination) {
			destination = new Menu(category_name, menu);
			menu->InsertAlphabetically(destination);
		}

		QAction *a = destination->InsertAlphabetically(n->Name());
		a->setData(i);
	}

	return menu;
}

Node *NodeFactory::CreateFromMenuAction(QAction *action)
{
	int index = action->data().toInt();

	if (index == -1) {
		return nullptr;
	}

	return library_.at(index)->copy();
}

QString NodeFactory::GetIDFromMenuAction(QAction *action)
{
	int index = action->data().toInt();

	if (index == -1) {
		return QString();
	}

	return library_.at(index)->id();
}

}
