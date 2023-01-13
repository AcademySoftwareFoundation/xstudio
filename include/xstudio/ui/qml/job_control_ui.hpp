// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>

namespace xstudio::ui::qml {
class JobControl {
  public:
    JobControl(QFutureInterfaceBase *fib) : fib_(fib) {}
    [[nodiscard]] bool shouldRun() const { return !fib_->isCanceled(); }

  private:
    QFutureInterfaceBase *fib_;
};

template <class T> class ControllableJob {
  public:
    virtual ~ControllableJob()     = default;
    virtual T run(JobControl &cjc) = 0;
};


template <typename T> class RunControllableJob : public QFutureInterface<T>, public QRunnable {
  public:
    RunControllableJob(
        ControllableJob<T> *job, QThreadPool *pool = QThreadPool::globalInstance())
        : job_(job), pool_(pool) {}
    ~RunControllableJob() override { delete job_; }

    QFuture<T> start() {
        this->setRunnable(this);
        this->reportStarted();

        auto future = this->future();
        pool_->start(this, 0);

        return future;
    }

    void run() override {
        if (not this->isCanceled()) {
            JobControl control(this);
            result_ = this->job_->run(control);

            if (not this->isCanceled())
                this->reportResult(result_);
        }
        this->reportFinished();
    }

    T result_;
    ControllableJob<T> *job_{nullptr};
    QThreadPool *pool_{nullptr};
};

class JobExecutor {
  public:
    template <class T>
    static QFuture<T>
    run(ControllableJob<T> *job, QThreadPool *pool = QThreadPool::globalInstance()) {
        return (new RunControllableJob<T>(job, pool))->start();
    }
};
} // namespace xstudio::ui::qml
