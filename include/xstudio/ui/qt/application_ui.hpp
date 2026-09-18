// SPDX-License-Identifier: Apache-2.0
#include <QApplication>

namespace xstudio::ui::qt {
class ApplicationUI : public QApplication {
  public:
    ApplicationUI(int &argc, char **argv);
};
} // namespace xstudio::ui::qt
