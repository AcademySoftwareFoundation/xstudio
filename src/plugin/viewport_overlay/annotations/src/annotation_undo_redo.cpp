// SPDX-License-Identifier: Apache-2.0

#include "annotation_undo_redo.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::ui::canvas;
using namespace xstudio::ui::viewport;

namespace fs = std::filesystem;

bool AddStroke::redo(Annotation ** annotation) {

    if (!(*annotation)) return false;
    (*annotation)->canvas().append_item(stroke_);
    return true;
}

bool AddStroke::undo(Annotation ** annotation) {

    if (!(*annotation)) return false;

    // find the matching stroke and remove. The assumption here is that
    // every stroke is unique.
    auto p = (*annotation)->canvas().end();
    while (p != (*annotation)->canvas().begin()) {
        p--;
        if (std::holds_alternative<Stroke>(*p)) {
            auto &stroke = std::get<Stroke>(*p);
            if (stroke == stroke_) {
                (*annotation)->canvas().remove_item(p);
                return true;
            }
        }
    }
    return false;
}

bool ModifyOrAddCaption::redo(Annotation ** annotation) {

    if (!(*annotation)) return false;

    auto p = (*annotation)->canvas().end();
    while (p != (*annotation)->canvas().begin()) {
        p--;
        if (std::holds_alternative<Caption>(*p)) {
            auto &caption = std::get<Caption>(*p);
            if (caption.id() == caption_.id()) {
                original_caption_ = caption;
                (*annotation)->canvas().overwrite_item(p, caption_);
                return true;
            }
        }
    }

    (*annotation)->canvas().append_item(caption_);
    return true;

}

bool ModifyOrAddCaption::undo(Annotation ** annotation) {

    if (!(*annotation)) return false;

    // do we have 'original_caption_' with content ? If so, we're undoing
    // a modification
    auto p = (*annotation)->canvas().end();
    while (p != (*annotation)->canvas().begin()) {
        p--;
        if (std::holds_alternative<Caption>(*p)) {
            auto &caption = std::get<Caption>(*p);
            if (caption.id() == original_caption_.id()) {
                (*annotation)->canvas().overwrite_item(p, original_caption_);
                return true;
            }
        }
    }

    // otherwise we are undo-ing an add
    p = (*annotation)->canvas().end();
    while (p != (*annotation)->canvas().begin()) {
        p--;
        if (std::holds_alternative<Caption>(*p)) {
            auto &caption = std::get<Caption>(*p);
            if (caption.id() == caption_.id()) {
                (*annotation)->canvas().remove_item(p);
                return true;
            }
        }
    }
    return false;

}

bool DeleteCaption::redo(Annotation ** annotation) {

    if (!(*annotation)) return false;

    auto p = (*annotation)->canvas().end();
    size_t idx = (*annotation)->canvas().size();
    while (p != (*annotation)->canvas().begin()) {
        p--;
        idx--;
        if (std::holds_alternative<Caption>(*p)) {
            auto &caption = std::get<Caption>(*p);
            if (caption.id() == caption_id_) {
                caption_idx_ = idx;
                caption_ = caption;
                (*annotation)->canvas().remove_item(p);
                return true;
            }
        }
    }
    return false;

}

bool DeleteCaption::undo(Annotation ** annotation) {

    if (!(*annotation)) return false;

    auto p = (*annotation)->canvas().begin();
    size_t idx = caption_idx_;
    while (p != (*annotation)->canvas().end() && idx) {
        idx--;
    }

    if (p == (*annotation)->canvas().end()) {
        (*annotation)->canvas().append_item(caption_);
    } else {
        (*annotation)->canvas().insert_item(p, caption_);
    }
    return true;
}

