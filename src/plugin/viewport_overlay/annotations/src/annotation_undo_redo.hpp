// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "annotation.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class UndoableAction {

          public:
            UndoableAction() = default;

            virtual bool redo(Annotation **annotation) = 0;
            virtual bool undo(Annotation **annotation) = 0;

            bool __redo(Annotation **annotation) {
                bool rt = redo(annotation);
                if (concat_action_)
                    rt |= concat_action_->__redo(annotation);
                return rt;
            }

            bool __undo(Annotation **annotation) {
                bool rt = false;
                if (concat_action_)
                    rt |= concat_action_->__undo(annotation);
                rt |= undo(annotation);
                return rt;
            }

            // we can use this to chain together a number of undo/redo-ables
            // into a single undo/redo (from the user perspective)
            std::unique_ptr<UndoableAction> concat_action_;

            bool needs_existing_annotation = {true};
        };
        typedef std::unique_ptr<UndoableAction> UndoableActionPtr;

        class UndoRedoList : public std::vector<std::pair<UndoableActionPtr, utility::Uuid>> {
          public:
            // using std::vector<std::pair<UndoableActionPtr, utility::Uuid>> = Base;

            UndoRedoList() { position_ = rbegin(); }

            void concat_action(UndoableAction *action, const utility::Uuid bookmark_id) {

                // this must be called after an add_action has just happened
                if (!size())
                    return;
                // get to the last concat_action_ action in the chain
                std::unique_ptr<UndoableAction> *concat = &(back().first->concat_action_);
                while (*concat) {
                    concat = &((*concat)->concat_action_);
                }
                concat->reset(action);
            }

            void add_action(UndoableAction *action, const utility::Uuid bookmark_id) {

                // if some undos have been done, our position in the history is
                // not at the 'head'. Erase the redoable items between our positino
                // in the history and the head
                auto d = std::distance(rbegin(), position_);
                while (d > 0) {
                    pop_back();
                    d--;
                }

                emplace_back(std::make_pair(UndoableActionPtr(action), bookmark_id));
                position_ = rbegin();
            }

            utility::Uuid get_bookmark_id_for_next_undo() const {
                if (position_ == rend())
                    return utility::Uuid();
                return (*position_).second;
            }

            utility::Uuid get_bookmark_id_for_next_redo() const {
                if (position_ == rbegin())
                    return utility::Uuid();
                auto p = position_;
                p--;
                return (*p).second;
            }

            void step_forward() {
                if (position_ != rbegin())
                    position_--;
            }

            void step_backward() {
                if (position_ != rend())
                    position_++;
            }

            UndoableAction *get_next_redo() {
                if (position_ == rbegin())
                    return nullptr;
                position_--;
                auto result = (*position_).first.get();
                return result;
            }

            UndoableAction *get_next_undo() {
                if (position_ == rend())
                    return nullptr;
                auto result = (*position_).first.get();
                position_++;
                return result;
            }


          private:
            reverse_iterator position_;
        };
        typedef std::unique_ptr<UndoRedoList> UndoRedoListPtr;

        class PerUserUndoRedo : private std::map<utility::Uuid, UndoRedoListPtr> {
          public:
            PerUserUndoRedo() = default;

            template <typename T, typename... TT>
            void undoable_action(
                const bool concat,
                const utility::Uuid &user_id,
                utility::Uuid bookmark_id,
                Annotation &annotation,
                TT &&...args) {

                // create the undo/redo action
                auto action = new T(args...);

                if (concat) {
                    user_undo_history(user_id).concat_action(action, bookmark_id);
                } else {
                    user_undo_history(user_id).add_action(action, bookmark_id);
                }
                Annotation *a = &annotation;
                // now we 'do' the action
                action->redo(&a);
            }

            template <typename T, typename... TT>
            void undoable_action(
                const utility::Uuid &user_id,
                utility::Uuid bookmark_id,
                Annotation &annotation,
                TT &&...args) {

                // create the undo/redo action
                auto action = new T(args...);

                user_undo_history(user_id).add_action(action, bookmark_id);

                Annotation *a = &annotation;
                // now we 'do' the action
                action->redo(&a);
            }

            utility::Uuid get_bookmark_id_for_next_undo(const utility::Uuid &user_id) {
                return user_undo_history(user_id).get_bookmark_id_for_next_undo();
            }

            utility::Uuid get_bookmark_id_for_next_redo(const utility::Uuid &user_id) {
                return user_undo_history(user_id).get_bookmark_id_for_next_redo();
            }

            bool undo(const utility::Uuid &user_id, Annotation **annotation) {
                auto action = user_undo_history(user_id).get_next_undo();
                if (action) {
                    if (action->__undo(annotation))
                        return true;
                    // undo failed - restore position in history
                    user_undo_history(user_id).step_forward();
                }
                return false;
            }

            bool redo(const utility::Uuid &user_id, Annotation **annotation) {
                auto action = user_undo_history(user_id).get_next_redo();
                if (action) {
                    if (action->__redo(annotation))
                        return true;
                    // undo failed - restore position in history
                    user_undo_history(user_id).step_backward();
                }
                return false;
            }

          private:
            UndoRedoList &user_undo_history(const utility::Uuid &user_id) {
                if (find(user_id) == end()) {
                    (*this)[user_id].reset(new UndoRedoList());
                }
                return *((*this)[user_id]);
            }
        };

        class AddStroke : public UndoableAction {

          public:
            AddStroke(const canvas::Stroke &stroke) : stroke_{stroke} {}
            bool redo(Annotation **annotation) override;
            bool undo(Annotation **annotation) override;
            canvas::Stroke stroke_;
        };

        class ModifyOrAddCaption : public UndoableAction {

          public:
            ModifyOrAddCaption(const canvas::Caption &caption) : caption_{caption} {}
            bool redo(Annotation **annotation) override;
            bool undo(Annotation **annotation) override;
            canvas::Caption caption_, original_caption_;
        };

        class DeleteCaption : public UndoableAction {

          public:
            DeleteCaption(const utility::Uuid &caption_id) : caption_id_{caption_id} {}
            bool redo(Annotation **annotation) override;
            bool undo(Annotation **annotation) override;
            utility::Uuid caption_id_;
            canvas::Caption caption_;
            size_t caption_idx_ = 0;
        };


    } // namespace viewport
} // namespace ui
} // namespace xstudio
