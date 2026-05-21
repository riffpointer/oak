/*
 * Oak Video Editor - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * AppCallbacks: allows app/ to register function pointers that engine/ can call
 * to access app-level global state (undo stack, etc.) without including app/ headers.
 */

#ifndef APPCALLBACKS_H
#define APPCALLBACKS_H

namespace olive
{

class UndoStack;
class ViewerOutput;

struct AppCallbacks {
	UndoStack* (*get_undo_stack)(void) = nullptr;
	ViewerOutput* (*get_active_viewer_output)(void) = nullptr;

	void* (*progress_start)(const char* message) = nullptr;
	void (*progress_update)(void* handle, double value) = nullptr;
	void (*progress_end)(void* handle) = nullptr;
	bool (*progress_is_cancelled)(void* handle) = nullptr;

	void (*warn_cache_full)(void) = nullptr;
};

void oak_set_app_callbacks(const AppCallbacks* callbacks);
const AppCallbacks* oak_get_app_callbacks();

}

#endif // APPCALLBACKS_H
