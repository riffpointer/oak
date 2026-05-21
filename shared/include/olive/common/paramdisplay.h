/*
 * Oak Video Editor - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * Shared parameter display type enums used by both engine/ and app/.
 */

#ifndef OLIVE_COMMON_PARAMDISPLAY_H
#define OLIVE_COMMON_PARAMDISPLAY_H

namespace olive
{

/**
 * @brief Display hint for float parameters (used by node definitions in engine/)
 */
enum FloatDisplayType {
  kFloatNormal,
  kFloatDecibel,
  kFloatPercentage
};

/**
 * @brief Display hint for rational/time parameters (used by node definitions in engine/)
 */
enum RationalDisplayType {
  kRationalTime,
  kRationalFloat,
  kRationalRational
};

}

#endif // OLIVE_COMMON_PARAMDISPLAY_H
