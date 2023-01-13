// SPDX-License-Identifier: Apache-2.0
#include <QApplication>

namespace xstudio {
namespace ui {
    namespace qt {
        class ApplicationUI : public QApplication {
          public:
            ApplicationUI(int &argc, char **argv);
        };
    } // namespace qt
} // namespace ui
} // namespace xstudio