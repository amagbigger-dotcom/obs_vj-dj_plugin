---
name: obs-cpp-qt-patterns
description: Integrate C++ and Qt6 into OBS Studio plugins for settings dialogs, frontend API access, and rich UI components. Covers CMake configuration, optional Qt builds, and platform fallbacks.
---

# OBS C++ and Qt6 Patterns

Integration guide for modern C++20 and Qt6 within native OBS Studio plugins.

## Purpose

Integrate C++ and Qt6 into OBS Studio plugins for settings dialogs, frontend API access, and rich UI components. Covers CMake configuration, optional Qt builds, and platform fallbacks.

## When to Use

- Building custom GUI settings dialogs, dockable panels, or modal windows for OBS.
- Interacting with the OBS main window via `obs-frontend-api`.
- Configuring CMake for optional Qt6 builds (allowing headless or no-Qt builds).
- Bridging C-based `libobs` module callbacks with C++ / Qt objects.

## When NOT to Use

- Cross-compiling from Linux to Windows &rarr; Use `obs-cross-compiling`
- Windows-specific builds & MSVC linking &rarr; Use `obs-windows-building`
- Audio plugin core DSP logic &rarr; Use `obs-audio-plugin-writing`
- Code review & quality audits &rarr; Use `obs-plugin-reviewing`
- General architecture coordination &rarr; Use `obs-plugin-expert` agent

---

## Quick Start: Qt Settings Dialog in 5 Steps

### Step 1: Configure CMake for Qt

```cmake
cmake_minimum_required(VERSION 3.28)
project(my-plugin VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt automoc (REQUIRED for Qt classes with Q_OBJECT)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 (optional)
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt6 found - building with settings dialog")
    set(BUILD_WITH_QT ON)
else()
    message(STATUS "Qt6 not found - building without settings dialog")
    set(BUILD_WITH_QT OFF)
endif()
```

### Step 2: Add Qt Sources Conditionally

```cmake
# Core plugin (C)
add_library(${PROJECT_NAME} MODULE
    src/plugin-main.c
    src/my-source.c
)

# Add Qt frontend (C++) if available
if(BUILD_WITH_QT)
    target_sources(${PROJECT_NAME} PRIVATE src/frontend.cpp src/settings-dialog.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_WITH_QT)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

    # Link OBS frontend API
    find_package(obs-frontend-api QUIET)
    if(obs-frontend-api_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
    endif()
else()
    # Fallback: simple C frontend without dialog
    target_sources(${PROJECT_NAME} PRIVATE src/frontend-simple.c)
endif()
```

### Step 3: Create Settings Dialog Header (`src/settings-dialog.h`)

```cpp
#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QString host() const { return m_hostEdit->text(); }
    int port() const { return m_portSpin->value(); }

    void setHost(const QString &host) { m_hostEdit->setText(host); }
    void setPort(int port) { m_portSpin->setValue(port); }

private:
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
};
```

### Step 4: Implement Settings Dialog (`src/settings-dialog.cpp`)

```cpp
#include "settings-dialog.h"
#include <QHBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Plugin Settings"));
    setMinimumWidth(300);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText(tr("127.0.0.1"));

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(5000);

    auto *okButton = new QPushButton(tr("OK"), this);
    auto *cancelButton = new QPushButton(tr("Cancel"), this);

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("Host:"), m_hostEdit);
    formLayout->addRow(tr("Port:"), m_portSpin);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}
```

### Step 5: Integrate with OBS Frontend API (`src/frontend.cpp`)

```cpp
#include <obs-module.h>
#include <obs-frontend-api.h>

#ifdef BUILD_WITH_QT
#include "settings-dialog.h"
#include <QMainWindow>
#endif

extern "C" {

static void on_frontend_event(enum obs_frontend_event event, void *data)
{
    (void)data;

    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        /* OBS UI is ready - safe to attach docks or menu items */
        break;
    default:
        break;
    }
}

void frontend_init(void)
{
    obs_frontend_add_event_callback(on_frontend_event, NULL);
}

void frontend_cleanup(void)
{
    obs_frontend_remove_event_callback(on_frontend_event, NULL);
}

void frontend_show_settings(void)
{
#ifdef BUILD_WITH_QT
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    SettingsDialog dialog(mainWindow);
    if (dialog.exec() == QDialog::Accepted) {
        /* Save settings */
    }
#endif
}

} /* extern "C" */
```

---

## Best Practices & Guidelines

1. **Always use `extern "C"`** around C-linkage entry points and `libobs` hooks.
2. **Parent your QWidgets** to `obs_frontend_get_main_window()` to ensure modal consistency and proper z-ordering.
3. **Never touch Qt widgets from worker threads**: Dispatch to the main thread via signals/slots or `QMetaObject::invokeMethod`.
4. **Use `CMAKE_AUTOMOC` and `CMAKE_AUTOUIC`** in CMake to avoid manual moc execution.

---

## References

- **OBS Frontend API Reference**: [https://docs.obsproject.com/reference-frontend-api](https://docs.obsproject.com/reference-frontend-api)
- **Qt6 Widgets Documentation**: [https://doc.qt.io/qt-6/qtwidgets-index.html](https://doc.qt.io/qt-6/qtwidgets-index.html)
- **Related Skills**: `obs-plugin-developing`, `obs-windows-building`
- **Related Agent**: `obs-plugin-expert`
