// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/canvas/canvas_undo_redo.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

using namespace xstudio::ui::canvas;
using namespace xstudio;


void UndoRedoAdd::redo(Canvas *canvas) { canvas->items_.push_back(item_); }

void UndoRedoAdd::undo(Canvas *canvas) {

    if (canvas->items_.size()) {
        canvas->items_.pop_back();
    }
}

void UndoRedoDel::redo(Canvas *canvas) {

    if (canvas->items_.size()) {
        canvas->items_.pop_back();
    }
}

void UndoRedoDel::undo(Canvas *canvas) { canvas->items_.push_back(item_); }

void UndoRedoClear::redo(Canvas *canvas) { canvas->items_.clear(); }

void UndoRedoClear::undo(Canvas *canvas) { canvas->items_ = items_; }