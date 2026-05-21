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

#ifndef FLOATSLIDER_H
#define FLOATSLIDER_H

#include "base/decimalsliderbase.h"

#include "olive/common/paramdisplay.h"

namespace olive
{

class FloatSlider : public DecimalSliderBase {
	Q_OBJECT
public:
	FloatSlider(QWidget *parent = nullptr);

	using DisplayType = FloatDisplayType;

	double GetValue() const;

	void SetValue(const double &d);

	void SetDefaultValue(const double &d);

	void SetMinimum(const double &d);

	void SetMaximum(const double &d);

	void SetDisplayType(const DisplayType &type);

	static double TransformValueToDisplay(double val, FloatDisplayType display);

	static double TransformDisplayToValue(double val, FloatDisplayType display);

	static QString ValueToString(double val, FloatDisplayType display,
								 int decimal_places,
								 bool autotrim_decimal_places);

protected:
	virtual QString ValueToString(const QVariant &v) const override;

	virtual QVariant StringToValue(const QString &s, bool *ok) const override;

	virtual QVariant
	AdjustDragDistanceInternal(const QVariant &start,
							   const double &drag) const override;

	virtual void ValueSignalEvent(const QVariant &value) override;

signals:
	void ValueChanged(double);

private:
	FloatDisplayType display_type_;
};

}

#endif // FLOATSLIDER_H
