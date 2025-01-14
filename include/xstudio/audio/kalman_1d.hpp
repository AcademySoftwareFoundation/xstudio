// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cmath>
#include <vector>

namespace xstudio::audio {

/**
 *  @brief A most basic, one tap, 1D kalman filter for fixed time interval measurements
 */

class KalmanFilter1D {

  public:
    /**
     *  @brief Constructor
     *
     *  @details Set all parameters on construction.
     */
    KalmanFilter1D(
        double init_val     = 0.0,
        double err          = 100.0,
        double pnoise       = 0.125,
        double sensor_noise = 4.0)
        : est_val_(init_val),
          est_error_(err),
          process_noise_(pnoise),
          sensor_noise_(sensor_noise) {}

    /**
     *  @brief Copy constructor
     */
    KalmanFilter1D(const KalmanFilter1D &o) = default;

    /**
     *  @brief Assignment operator
     */
    KalmanFilter1D &operator=(const KalmanFilter1D &o) = default;

    /**
     *  @brief Update the state of the model with a measurement
     *
     *  @details Returns the updated estimate
     */
    double update(double measurement) {
        est_error_         = est_error_ + process_noise_;
        double kalman_gain = est_error_ / (est_error_ + sensor_noise_);
        est_val_           = est_val_ + kalman_gain * (measurement - est_val_);
        est_error_         = (1 - kalman_gain) * est_error_;
        return est_val_;
    }

    /**
     *  @brief Access the current estimate
     */
    [[nodiscard]] double current_estimate() const { return est_val_; }

  private:
    double est_val_, est_error_, process_noise_, sensor_noise_;
};

/**
 *  @brief A simple damped mass-on-a-spring simulator. This can be used to
 *  filter a noisy moving signal like playhead position (video frame) to
 *  help with audio playback
 */

class MassOnASpringFilter {

  public:
    /**
     *  @brief Constructor
     *
     *  @details A lower spring strength means a smoother estimate but one that responds
     *  more slowly to changes in the spring velocity.
     */
    MassOnASpringFilter(const double spring_strength = 1.0, const double damping = 0.9)
        : s_(spring_strength / 10000.0), d_(damping) {}

    /**
     *  @brief Reset the last 'measurement' - allows the model to continue smoothly
     *  at the same velocity when the spring position has jumped
     */
    void reset(
        const double measurement,
        const double velocity = std::numeric_limits<double>::lowest()) {
        spring_pos_ = measurement;
        if (velocity != std::numeric_limits<double>::lowest()) {
            mass_velocity_ = velocity;
        }
        mass_pos_ = spring_pos_ - mass_velocity_ * d_ / s_;
    }

    /**
     *  @brief Update the state of the system with a measurement of the spring (anchor)
     *  position
     */
    void update(const double measurement, const double time) {

        if (prev_time_ == -1.0) {
            prev_time_ = time;
            return;
        }

        const double delta_t = time - prev_time_;

        if (delta_t < 50)
            return;
        const double delta_m = (measurement - spring_pos_) / delta_t;
        const double old_pos = mass_pos_;

        for (double t = 0; t < delta_t; ++t) {
            spring_pos_ += delta_m;
            double force = -(mass_pos_ - spring_pos_) * s_ - mass_velocity_ * d_;
            mass_velocity_ += force;
            mass_pos_ += mass_velocity_;
        }
        prev_time_     = time;
        mass_velocity_ = (mass_pos_ - old_pos) / delta_t;
    }

    /**
     * @brief Estimate the position of the mass at some time.
     */
    [[nodiscard]] double estimate(const double time) const {
        return mass_pos_ + (time - prev_time_) * mass_velocity_;
    }

    [[nodiscard]] double velocity() const { return mass_velocity_; }

  private:
    double spring_pos_    = {0};
    double mass_pos_      = {0};
    double mass_velocity_ = {0};
    double prev_time_     = {-1.0};
    const double s_;
    const double d_;
};

} // namespace xstudio::audio
