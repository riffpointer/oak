/*
 * Oak Video Editor - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 */

#include "appcallbacks.h"

namespace olive
{

static AppCallbacks g_callbacks;

void oak_set_app_callbacks(const AppCallbacks* callbacks)
{
	if (callbacks) {
		g_callbacks = *callbacks;
	}
}

const AppCallbacks* oak_get_app_callbacks()
{
	return &g_callbacks;
}

}
