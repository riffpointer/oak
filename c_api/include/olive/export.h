/*
 * Olive - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cross-platform symbol visibility/export macros for Olive C API.
 */

#ifndef OLIVE_EXPORT_H
#define OLIVE_EXPORT_H

#ifdef _WIN32
#  ifdef OLIVE_BUILDING_CORE
#    define OLIVE_CORE_API __declspec(dllexport)
#  else
#    define OLIVE_CORE_API __declspec(dllimport)
#  endif
#else
#  ifdef OLIVE_BUILDING_CORE
#    define OLIVE_CORE_API __attribute__((visibility("default")))
#  else
#    define OLIVE_CORE_API
#  endif
#endif

#endif // OLIVE_EXPORT_H
