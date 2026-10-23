// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "application.h"

#include "base/i18n.h"
#include "ui/application.h"

int main(int argc, char *argv[])
{
    bayu::ui::Application app(argc, argv);
    app.setApplicationId("bayu-services");
    bayu::i18n::setDomain("bayu-core");
    Application services;
    return app.run();
}
