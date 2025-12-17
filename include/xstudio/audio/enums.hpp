// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace audio {
// maps to options in preference /core/audio/audio_scrub_duration
enum ScrubBehaviour { 
    OneFrame, 
    OnePt25Frames,
    OnePt5Frames,
    TwoFrames,
    ThreeFrames,
    OneFrameAt24Fps,
    OneFrameAt30Fps,
    OneFrameAt60Fps,
    Custom
 };
} // namespace audio
} // namespace xstudio