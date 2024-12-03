#include <nlohmann/json.hpp>

#include "async_request.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/atoms.hpp"

CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
// #include <QSignalSpy>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

class ShotBrowserRequest : public ControllableJob<QString> {
  public:
    ShotBrowserRequest(const nlohmann::json request, const caf::actor backend)
        : ControllableJob(), request_(std::move(request)), backend_(std::move(backend)) {}

    QString run(JobControl &cjc) override {
        auto result = QString();
        try {
            if (not cjc.shouldRun())
                throw std::runtime_error("Cancelled");

            caf::actor_system &system_ = CafSystemObject::get_actor_system();
            scoped_actor sys{system_};

            auto data = request_receive<JsonStore>(
                *sys, backend_, data_source::get_data_atom_v, JsonStore(request_));

            result = QStringFromStd(data.dump());
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    }

  private:
    const nlohmann::json request_;
    const caf::actor backend_;
};

ShotBrowserResponse::ShotBrowserResponse(
    const QPersistentModelIndex index,
    int role,
    const nlohmann::json &request,
    const caf::actor backend,
    QThreadPool *pool)
    : index_(std::move(index)), role_(role), backend_(std::move(backend)) {
    // create a future..
    connect(
        &watcher_,
        &QFutureWatcher<QString>::finished,
        this,
        &ShotBrowserResponse::handleFinished);

    try {
        QFuture<QString> future =
            JobExecutor::run(new ShotBrowserRequest(request, backend_), pool);

        watcher_.setFuture(future);
    } catch (...) {
        deleteLater();
    }
}

void ShotBrowserResponse::handleFinished() {
    if (watcher_.future().resultCount()) {
        auto result = watcher_.result();
        emit received(index_, role_, result);

        deleteLater();
    }
}
