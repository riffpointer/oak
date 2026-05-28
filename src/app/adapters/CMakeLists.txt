# Olive - Non-Linear Video Editor
# Copyright (C) 2022 Olive Team
# Modifications Copyright (C) 2025 mikesolar
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

set(OLIVE_SOURCES
  ${OLIVE_SOURCES}
  adapters/dlopen_helper.h
  adapters/dlopen_helper.cpp
  adapters/engine_adapter.h
  adapters/engine_adapter.cpp
  PARENT_SCOPE
)
