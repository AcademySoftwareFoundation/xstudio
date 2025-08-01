// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

#include "xstudio/ui/canvas/canvas.hpp"


namespace xstudio {
namespace ui {
    namespace canvas {

        class UndoRedoAdd : public CanvasUndoRedo {

          public:
            UndoRedoAdd(const Canvas::Item &item) : item_(item) {}

            void redo(Canvas *) override;
            void undo(Canvas *) override;

            Canvas::Item item_;
        };

        class UndoRedoDel : public CanvasUndoRedo {

          public:
            UndoRedoDel(const Canvas::Item &item) : item_(item) {}

            void redo(Canvas *) override;
            void undo(Canvas *) override;

            Canvas::Item item_;
        };

        class UndoRedoClear : public CanvasUndoRedo {

          public:
            UndoRedoClear(const Canvas::ItemVec &items) : items_(items) {}

            void redo(Canvas *) override;
            void undo(Canvas *) override;

            Canvas::ItemVec items_;
        };

    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio