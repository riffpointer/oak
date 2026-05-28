/*
 *  oakengine.so C API — Undo System
 *  UndoStack, UndoCommand, MultiUndoCommand, and concrete command factories.
 */

#include "oak/engine_api.h"

#include "undo/undostack.h"
#include "undo/undocommand.h"
#include "node/nodeundo.h"
#include "node/project/folder/folder.h"
#include "node/project.h"

namespace olive {

/* ------------------------------------------------------------------ */
/*  Custom undo command proxy (for app-level commands)                 */
/* ------------------------------------------------------------------ */
class CustomUndoCommand : public UndoCommand {
public:
  CustomUndoCommand(OakUndoRedoFn redo_fn, OakUndoUndoFn undo_fn, void* userdata)
    : redo_fn_(redo_fn), undo_fn_(undo_fn), userdata_(userdata) {}

  virtual Project *GetRelevantProject() const override
  {
    return nullptr;
  }

protected:
  virtual void redo() override
  {
    if (redo_fn_) redo_fn_(userdata_);
  }

  virtual void undo() override
  {
    if (undo_fn_) undo_fn_(userdata_);
  }

private:
  OakUndoRedoFn redo_fn_;
  OakUndoUndoFn undo_fn_;
  void* userdata_;
};

} // namespace olive

extern "C" {

/* ------------------------------------------------------------------ */
/*  UndoStack                                                          */
/* ------------------------------------------------------------------ */
OakUndoStackHandle oak_undo_stack_create(void)
{
  return reinterpret_cast<OakUndoStackHandle>(new olive::UndoStack());
}

void oak_undo_stack_destroy(OakUndoStackHandle stack)
{
  if (!stack) return;
  delete reinterpret_cast<olive::UndoStack*>(stack);
}

void oak_undo_stack_push(OakUndoStackHandle stack, void* command_opaque, const char* name)
{
  if (!stack || !command_opaque) return;
  auto* cmd = reinterpret_cast<olive::UndoCommand*>(command_opaque);
  reinterpret_cast<olive::UndoStack*>(stack)->push(cmd, QString::fromUtf8(name));
}

void oak_undo_stack_undo(OakUndoStackHandle stack)
{
  if (!stack) return;
  reinterpret_cast<olive::UndoStack*>(stack)->undo();
}

void oak_undo_stack_redo(OakUndoStackHandle stack)
{
  if (!stack) return;
  reinterpret_cast<olive::UndoStack*>(stack)->redo();
}

void oak_undo_stack_clear(OakUndoStackHandle stack)
{
  if (!stack) return;
  reinterpret_cast<olive::UndoStack*>(stack)->clear();
}

bool oak_undo_stack_can_undo(OakUndoStackHandle stack)
{
  if (!stack) return false;
  return reinterpret_cast<olive::UndoStack*>(stack)->CanUndo();
}

bool oak_undo_stack_can_redo(OakUndoStackHandle stack)
{
  if (!stack) return false;
  return reinterpret_cast<olive::UndoStack*>(stack)->CanRedo();
}

/* ------------------------------------------------------------------ */
/*  MultiUndoCommand                                                   */
/* ------------------------------------------------------------------ */
OakUndoCommandHandle oak_undo_command_multi_create(void)
{
  return reinterpret_cast<OakUndoCommandHandle>(new olive::MultiUndoCommand());
}

void oak_undo_command_multi_add_child(OakUndoCommandHandle multi, OakUndoCommandHandle child)
{
  if (!multi || !child) return;
  auto* m = reinterpret_cast<olive::MultiUndoCommand*>(multi);
  auto* c = reinterpret_cast<olive::UndoCommand*>(child);
  m->add_child(c);
}

void oak_undo_command_multi_destroy(OakUndoCommandHandle multi)
{
  if (!multi) return;
  // MultiUndoCommand does NOT own its children by default (they are usually
  // pushed to UndoStack which owns them). But if a multi-command was never
  // pushed, its children leak. For safety, we delete children that weren't
  // pushed. However, the existing code assumes children are owned by the
  // MultiUndoCommand. Let's match existing semantics.
  delete reinterpret_cast<olive::MultiUndoCommand*>(multi);
}

/* ------------------------------------------------------------------ */
/*  Concrete commands                                                  */
/* ------------------------------------------------------------------ */
OakUndoCommandHandle oak_command_node_add_create(OakEngineProjectHandle proj, OakNodeHandle node)
{
  if (!proj || !node) return nullptr;
  auto* p = reinterpret_cast<olive::Project*>(proj);
  auto* n = reinterpret_cast<olive::Node*>(node);
  return reinterpret_cast<OakUndoCommandHandle>(new olive::NodeAddCommand(p, n));
}

OakUndoCommandHandle oak_command_folder_add_child_create(OakNodeHandle folder, OakNodeHandle child)
{
  if (!folder || !child) return nullptr;
  auto* f = reinterpret_cast<olive::Folder*>(folder);
  auto* c = reinterpret_cast<olive::Node*>(child);
  return reinterpret_cast<OakUndoCommandHandle>(new olive::FolderAddChild(f, c));
}

OakUndoCommandHandle oak_command_node_set_position_create(OakNodeHandle node, const float* pos)
{
  if (!node || !pos) return nullptr;
  auto* n = reinterpret_cast<olive::Node*>(node);
  // Node::Position is a QPointF-like struct; we assume x=pos[0], y=pos[1]
  olive::Node::Position p;
  p.position.setX(pos[0]);
  p.position.setY(pos[1]);
  return reinterpret_cast<OakUndoCommandHandle>(new olive::NodeSetPositionCommand(n, n, p));
}

OakUndoCommandHandle oak_command_node_rename_create(OakNodeHandle node, const char* new_name)
{
  if (!node) return nullptr;
  auto* n = reinterpret_cast<olive::Node*>(node);
  return reinterpret_cast<OakUndoCommandHandle>(new olive::NodeRenameCommand(n, QString::fromUtf8(new_name)));
}

/* ------------------------------------------------------------------ */
/*  Custom callback-based command (for app-level commands)             */
/* ------------------------------------------------------------------ */
OakUndoCommandHandle oak_undo_command_custom_create(OakUndoRedoFn redo_fn, OakUndoUndoFn undo_fn, void* userdata)
{
  if (!redo_fn && !undo_fn) return nullptr;
  return reinterpret_cast<OakUndoCommandHandle>(new olive::CustomUndoCommand(redo_fn, undo_fn, userdata));
}

} // extern "C"
