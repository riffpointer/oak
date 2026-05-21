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

#include "floatslider.h"

#include <cmath>
#include <QDebug>

#include "olive/common/decibel.h"

namespace olive
{

#define super DecimalSliderBase

FloatSlider::FloatSlider(QWidget *parent)
	: super(parent)
	, display_type_(kFloatNormal)
{
	SetValue(0.0);
}

double FloatSlider::GetValue() const
{
	return GetValueInternal().toDouble();
}

void FloatSlider::SetValue(const double &d)
{
	SetValueInternal(d);
}

void FloatSlider::SetDefaultValue(const double &d)
{
	super::SetDefaultValue(d);
}

void FloatSlider::SetMinimum(const double &d)
{
	SetMinimumInternal(d);
}

void FloatSlider::SetMaximum(const double &d)
{
	SetMaximumInternal(d);
}

void FloatSlider::SetDisplayType(const FloatSlider::DisplayType &type)
{
	display_type_ = type;

	switch (display_type_) {
	case kFloatNormal:
		ClearFormat();
		break;
	case kFloatDecibel:
		SetFormat(tr("%1 dB"));
		break;
	case kFloatPercentage:
		SetFormat(tr("%1%"));
		break;
	}
}

double FloatSlider::TransformValueToDisplay(double val, DisplayType display)
{
	switch (display) {
	case kFloatNormal:
		break;
	case kFloatDecibel:
		val = Decibel::fromLinear(val);
		break;
	case kFloatPercentage:
		val *= 100.0;
		break;
	}

	return val;
}

double FloatSlider::TransformDisplayToValue(double val, DisplayType display)
{
	switch (display) {
	case kFloatNormal:
		break;
	case kFloatDecibel:
		val = Decibel::toLinear(val);
		break;
	case kFloatPercentage:
		val *= 0.01;
		break;
	}

	return val;
}

QString FloatSlider::ValueToString(double val, FloatSlider::DisplayType display,
								   int decimal_places,
								   bool autotrim_decimal_places)
{
	// Return negative infinity for zero volume
	if (display == kFloatDecibel && qIsNull(val)) {
		return tr("\xE2\x88\x9E");
	}

	return FloatToString(TransformValueToDisplay(val, display), decimal_places,
						 autotrim_decimal_places);
}

QString FloatSlider::ValueToString(const QVariant &v) const
{
	return ValueToString(v.toDouble() + GetOffset().toDouble(), display_type_,
						 GetDecimalPlaces(), GetAutoTrimDecimalPlaces());
}

QVariant FloatSlider::StringToValue(const QString &s, bool *ok) const
{
	bool valid;
	double val = s.toDouble(&valid);

	// If we were given an `ok` pointer, set it to `valid`
	if (ok) {
		*ok = valid;
	}

	// If valid, transform it from display
	if (valid) {
		val = TransformDisplayToValue(val, display_type_);
	}

	// Return un-offset value
	return val - GetOffset().toDouble();
}

QVariant FloatSlider::AdjustDragDistanceInternal(const QVariant &start,
												 const double &drag) const
{
	switch (display_type_) {
	case kFloatNormal:
		// No change here
		break;
	case kFloatDecibel: {
		double current_db = Decibel::fromLinear(start.toDouble());
		current_db += drag;
		double adjusted_linear = Decibel::toLinear(current_db);

		return adjusted_linear;
	}
	case kFloatPercentage:
		return super::AdjustDragDistanceInternal(start, drag * 0.01);
	}

	return super::AdjustDragDistanceInternal(start, drag);
}

void FloatSlider::ValueSignalEvent(const QVariant &value)
{
	emit ValueChanged(value.toDouble());
}

}
