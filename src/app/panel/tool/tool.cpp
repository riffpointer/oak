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

#include "tool.h"

#include "core.h"
#include "widget/toolbar/toolbar.h"

namespace olive
{

ToolPanel::ToolPanel()
	: PanelWidget(QStringLiteral("ToolPanel"))
{
	Toolbar *t = new Toolbar(this);

	t->SetTool(App::instance()->tool());
	t->SetSnapping(App::instance()->snapping());

	SetWidgetWithPadding(t);

	connect(t, &Toolbar::ToolChanged, App::instance(), &App::SetTool);
	connect(App::instance(), &App::ToolChanged, t, &Toolbar::SetTool);

	connect(t, &Toolbar::SnappingChanged, App::instance(), &App::SetSnapping);
	connect(App::instance(), &App::SnappingChanged, t, &Toolbar::SetSnapping);

	connect(t, &Toolbar::SelectedTransitionChanged, App::instance(),
			&App::SetSelectedTransitionObject);

	Retranslate();
}

void ToolPanel::Retranslate()
{
	SetTitle(tr("Tools"));
}

}
