// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

CAF_PUSH_WARNINGS
#include <QDebug>
#include <QPoint>
#include <QUrl>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/frame_time.hpp"

namespace xstudio {
namespace utility {
    class MediaReference;
}
namespace ui {
    namespace qml {

        class TimesliderMarker : public QObject {
            Q_OBJECT
            Q_PROPERTY(int x READ x NOTIFY xChanged)
            Q_PROPERTY(int y READ y NOTIFY yChanged)
            Q_PROPERTY(QString c READ c NOTIFY cChanged)
            QML_ELEMENT

          public:
            explicit TimesliderMarker(
                const int x, const int y, const QString c = "", QObject *parent = nullptr)
                : QObject(parent), x_(x), y_(y), c_(std::move(c)) {}
            ~TimesliderMarker() override = default;
            [[nodiscard]] int x() const { return x_; }
            [[nodiscard]] int y() const { return y_; }
            [[nodiscard]] QString c() const { return c_; }

          signals:
            void xChanged();
            void yChanged();
            void cChanged();

          private:
            int x_;
            int y_;
            QString c_;
        };

        class PlayheadUI : public QMLActor {

            Q_OBJECT
            Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
            Q_PROPERTY(int loopMode READ loopMode WRITE setLoopMode NOTIFY loopModeChanged)
            Q_PROPERTY(
                QVariant loopModeOptions READ loopModeOptions NOTIFY loopModeOptionsChanged)
            Q_PROPERTY(bool native READ native WRITE setNative NOTIFY nativeChanged)
            Q_PROPERTY(int playRateMode READ playRateMode WRITE setPlayRateMode NOTIFY
                           playRateModeChanged)
            Q_PROPERTY(bool forward READ forward WRITE setForward NOTIFY forwardChanged)
            Q_PROPERTY(double playheadRate READ playheadRate WRITE setPlayheadRate NOTIFY
                           playheadRateChanged)
            Q_PROPERTY(double rate READ rate NOTIFY rateChanged)
            Q_PROPERTY(float velocityMultiplier READ velocityMultiplier WRITE
                           setVelocityMultiplier NOTIFY velocityMultiplierChanged)

            Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
            Q_PROPERTY(int frames READ frames NOTIFY framesChanged)

            Q_PROPERTY(double second READ second NOTIFY secondChanged)
            Q_PROPERTY(double seconds READ seconds NOTIFY secondsChanged)

            Q_PROPERTY(int mediaFrame READ mediaFrame NOTIFY mediaFrameChanged)
            Q_PROPERTY(double mediaSecond READ mediaSecond NOTIFY mediaSecondChanged)

            // logical frame - absolute
            Q_PROPERTY(
                int mediaLogicalFrame READ mediaLogicalFrame NOTIFY mediaLogicalFrameChanged)
            Q_PROPERTY(double mediaLogicalSecond READ mediaLogicalSecond NOTIFY
                           mediaLogicalSecondChanged)

            Q_PROPERTY(QString timecodeStart READ timecodeStart NOTIFY timecodeStartChanged)
            Q_PROPERTY(QString timecode READ timecode NOTIFY timecodeChanged)
            Q_PROPERTY(int timecodeFrames READ timecodeFrames NOTIFY timecodeFramesChanged)
            Q_PROPERTY(QString timecodeEnd READ timecodeEnd NOTIFY timecodeEndChanged)
            Q_PROPERTY(
                QString timecodeDuration READ timecodeDuration NOTIFY timecodeDurationChanged)

            Q_PROPERTY(QUuid mediaUuid READ mediaUuid NOTIFY mediaUuidChanged)

            Q_PROPERTY(QString uuid READ uuid NOTIFY uuidChanged)
            Q_PROPERTY(QString name READ name NOTIFY nameChanged)
            Q_PROPERTY(int loopStart READ loopStart WRITE setLoopStart NOTIFY loopStartChanged)
            Q_PROPERTY(int loopEnd READ loopEnd WRITE setLoopEnd NOTIFY loopEndChanged)
            Q_PROPERTY(bool useLoopRange READ useLoopRange WRITE setUseLoopRange NOTIFY
                           useLoopRangeChanged)
            Q_PROPERTY(int keyPlayheadIndex READ keyPlayheadIndex WRITE setKeyPlayheadIndex
                           NOTIFY keyPlayheadIndexChanged)
            Q_PROPERTY(
                QString compareLayerName READ compareLayerName NOTIFY compareLayerNameChanged)
            Q_PROPERTY(int sourceOffsetFrames READ sourceOffsetFrames WRITE
                           setSourceOffsetFrames NOTIFY sourceOffsetFramesChanged)
            Q_PROPERTY(QList<QPoint> cachedFrames READ cachedFrames NOTIFY cachedFramesChanged)
            Q_PROPERTY(QList<TimesliderMarker *> bookmarkedFrames READ bookmarkedFrames NOTIFY
                           bookmarkedFramesChanged)

            Q_PROPERTY(bool isNull READ isNull NOTIFY isNullChanged)

          public:
            explicit PlayheadUI(QObject *parent = nullptr);
            ~PlayheadUI() override = default;

            void init(caf::actor_system &system) override;
            void set_backend(caf::actor backend);

            caf::actor backend() { return backend_; }

            [[nodiscard]] QUuid mediaUuid() const { return media_uuid_; }

            [[nodiscard]] bool playing() const { return playing_; }
            void setPlaying(const bool playing);

            [[nodiscard]] int loopMode() const { return looping_; }
            void setLoopMode(const int looping);

            [[nodiscard]] QVariant loopModeOptions() const { return loop_mode_options_; }

            [[nodiscard]] bool forward() const { return forward_; }
            void setForward(const bool forward);

            [[nodiscard]] bool native() const {
                return play_rate_mode_ == utility::TimeSourceMode::REMAPPED;
            }
            void setNative(const bool native);

            [[nodiscard]] int playRateMode() const { return static_cast<int>(play_rate_mode_); }
            void setPlayRateMode(const int tsm);

            [[nodiscard]] double playheadRate() const { return playhead_rate_; }
            void setPlayheadRate(const double playhead_rate);

            [[nodiscard]] double rate() const { return rate_; }

            [[nodiscard]] bool isNull() const { return !bool(backend_); }
            // void setRate(const double rate);

            [[nodiscard]] QList<QPoint> cachedFrames() const { return cache_detail_; }
            [[nodiscard]] QList<TimesliderMarker *> bookmarkedFrames() const {
                return bookmark_detail_ui_;
            }

            [[nodiscard]] float velocityMultiplier() const { return velocity_multiplier_; }
            void setVelocityMultiplier(const float velocity_multiplier);

            [[nodiscard]] double seconds() const { return seconds_; }
            [[nodiscard]] double second() const { return second_; }
            [[nodiscard]] double mediaSecond() const { return media_second_; }
            [[nodiscard]] double mediaLogicalSecond() const { return media_logical_second_; }

            void setFrame(const int frame);
            [[nodiscard]] int frames() const { return frames_; }
            [[nodiscard]] int frame() const { return frame_; }
            [[nodiscard]] int mediaFrame() const { return media_frame_; }
            [[nodiscard]] int mediaLogicalFrame() const { return media_logical_frame_; }

            [[nodiscard]] int loopStart() const { return loop_start_; }
            [[nodiscard]] int loopEnd() const { return loop_end_; }
            [[nodiscard]] int useLoopRange() const { return use_loop_range_; }

            [[nodiscard]] int keyPlayheadIndex() const { return key_playhead_index_; }
            void setKeyPlayheadIndex(int i);

            [[nodiscard]] int sourceOffsetFrames() const { return source_offset_frames_; }
            void setSourceOffsetFrames(int i);

            [[nodiscard]] QString uuid() const {
                return QString::fromStdString(to_string(uuid_));
            }

            [[nodiscard]] QString timecodeStart() const { return timecode_start_; }
            [[nodiscard]] QString timecode() const { return timecode_; }
            [[nodiscard]] int timecodeFrames() const { return timecode_frames_; }
            [[nodiscard]] QString timecodeEnd() const { return timecode_end_; }
            [[nodiscard]] QString timecodeDuration() const { return timecode_duration_; }
            QString compareLayerName();
            [[nodiscard]] QString name() const { return name_; }

          signals:
            void uuidChanged();
            void frameChanged();
            void mediaFrameChanged();
            void mediaLogicalFrameChanged();
            void framesChanged();
            void secondChanged();
            void mediaSecondChanged();
            void mediaLogicalSecondChanged();
            void secondsChanged();
            void velocityMultiplierChanged();
            void playingChanged();
            void forwardChanged();
            void loopModeChanged();
            void loopModeOptionsChanged();
            void nativeChanged();
            void playheadRateChanged();
            void rateChanged();
            void playRateModeChanged();
            void playlistSelectionThingChanged();
            void backendChanged();
            void loopStartChanged();
            void loopEndChanged();
            void useLoopRangeChanged();
            void nameChanged();
            void cachedFramesChanged();
            void bookmarkedFramesChanged();

            void timecodeStartChanged();
            void timecodeChanged();
            void timecodeFramesChanged();
            void timecodeEndChanged();
            void timecodeDurationChanged();

            void keyPlayheadIndexChanged();
            void compareLayerNameChanged();
            void sourceOffsetFramesChanged();

            void isNullChanged();

            void mediaUuidChanged(QUuid);


          public slots:

            void initSystem(QObject *system_qobject);
            void step(int step_frames);
            void skip(const bool forwards);
            void setLoopStart(const int loop_start);
            void setLoopEnd(const int loop_end);
            void setUseLoopRange(const bool use_loop_range);
            bool jumpToNextSource();
            bool jumpToPreviousSource();
            void jumpToSource(const QUuid media_uuid);
            void setFitMode(const QString mode);

            void connectToUI();
            void disconnectFromUI();
            void resetReadaheadRequests();

            [[nodiscard]] int nextBookmark(const int search_from_frame = -1) const;
            [[nodiscard]] int previousBookmark(int search_from_frame = -1) const;
            [[nodiscard]] QVariantList getNearestBookmark(const int search_from_frame) const;

          private:
            void media_changed();
            void rebuild_cache();
            void rebuild_bookmarks();

          private:
            caf::actor backend_;
            caf::actor backend_events_;
            utility::Uuid uuid_;
            bool playing_;
            int looping_;
            QVariant loop_mode_options_;

            utility::TimeSourceMode play_rate_mode_;
            bool forward_;

            double rate_;
            utility::FrameRate fr_rate_;

            double playhead_rate_;
            utility::FrameRate fr_playhead_rate_;

            float velocity_multiplier_;
            int frames_{0};
            double seconds_{0.0};
            int frame_{0};
            int media_frame_{0};
            int media_logical_frame_{0};
            int loop_start_;
            int loop_end_;
            bool use_loop_range_;
            double second_{0.0};
            double media_second_{0.0};
            double media_logical_second_{0.0};
            int key_playhead_index_;
            QString timecode_start_;
            QString timecode_;
            int timecode_frames_{0};
            QString timecode_end_;
            QString timecode_duration_;
            QString name_;
            int compare_mode_;
            int source_offset_frames_;
            QVariant compare_mode_options_;

            QList<QPoint> cache_detail_;
            QList<TimesliderMarker *> bookmark_detail_ui_;
            std::vector<std::tuple<utility::Uuid, std::string, int, int>> bookmark_detail_;
            QUuid media_uuid_;
        };
    } // namespace qml
} // namespace ui
} // namespace xstudio