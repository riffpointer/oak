/*
 * Oak Video Editor - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 */

#include "appcallbacks.h"

namespace olive
{

static AppCallbacks g_callbacks;

void SetAppCallbacks(const AppCallbacks* callbacks)
{
	if (callbacks) {
		g_callbacks = *callbacks;
	}
}

const AppCallbacks* GetAppCallbacks()
{
	return &g_callbacks;
}

}
