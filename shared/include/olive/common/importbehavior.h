/*
 * Oak Video Editor - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * Shared import behavior enum used by config and UI.
 */

#ifndef OLIVE_COMMON_IMPORTBEHAVIOR_H
#define OLIVE_COMMON_IMPORTBEHAVIOR_H

namespace olive
{

enum DropWithoutSequenceBehavior {
  kDWSAsk,
  kDWSAuto,
  kDWSManual,
  kDWSDisable
};

}

#endif // OLIVE_COMMON_IMPORTBEHAVIOR_H
