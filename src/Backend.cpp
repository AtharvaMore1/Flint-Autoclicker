#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QAbstractNativeEventFilter>
#include "Backend.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>
#include <random>

namespace {
constexpr int HOTKEY_ID = 1;
constexpr ULONG_PTR FLINT_INPUT_TAG = 0x464C494E; // Identifies our generated input.
constexpr unsigned int MOD_BITS = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;

QString settingsPath() {
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    const QString folder = local.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        : local + QStringLiteral("/FlintAutoClicker");
    QDir().mkpath(folder);
    return folder + QStringLiteral("/settings.ini");
}

QRect virtualBounds() {
    QRect bounds;
    for (QScreen* screen : QGuiApplication::screens()) bounds = bounds.united(screen->geometry());
    return bounds;
}

QString keyName(unsigned int key) {
    if (key >= VK_F1 && key <= VK_F24) return QStringLiteral("F%1").arg(key - VK_F1 + 1);
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return QString(QChar(static_cast<ushort>(key)));
    switch (key) {
        case VK_SPACE: return QStringLiteral("Space");
        case VK_RETURN: return QStringLiteral("Enter");
        case VK_TAB: return QStringLiteral("Tab");
        case VK_ESCAPE: return QStringLiteral("Esc");
        case VK_BACK: return QStringLiteral("Backspace");
        case VK_DELETE: return QStringLiteral("Delete");
        case VK_INSERT: return QStringLiteral("Insert");
        case VK_HOME: return QStringLiteral("Home");
        case VK_END: return QStringLiteral("End");
        case VK_PRIOR: return QStringLiteral("Page Up");
        case VK_NEXT: return QStringLiteral("Page Down");
        case VK_LEFT: return QStringLiteral("Left Arrow");
        case VK_RIGHT: return QStringLiteral("Right Arrow");
        case VK_UP: return QStringLiteral("Up Arrow");
        case VK_DOWN: return QStringLiteral("Down Arrow");
        default: break;
    }
    wchar_t nativeName[64]{};
    const unsigned int scanCode = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
    if (scanCode && GetKeyNameTextW(static_cast<LONG>(scanCode << 16), nativeName, 64) > 0)
        return QString::fromWCharArray(nativeName);
    return QStringLiteral("Key %1").arg(key);
}

QString shortcutName(unsigned int key, unsigned int modifiers) {
    QString result;
    if (modifiers & MOD_CONTROL) result += QStringLiteral("Ctrl + ");
    if (modifiers & MOD_ALT) result += QStringLiteral("Alt + ");
    if (modifiers & MOD_SHIFT) result += QStringLiteral("Shift + ");
    if (modifiers & MOD_WIN) result += QStringLiteral("Win + ");
    return result + keyName(key);
}

unsigned int virtualKey(QKeyEvent* event) {
    if (event->nativeVirtualKey()) return event->nativeVirtualKey();
    const int key = event->key();
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return 'A' + key - Qt::Key_A;
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return '0' + key - Qt::Key_0;
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return VK_F1 + key - Qt::Key_F1;
    return 0;
}

unsigned int modifiers(QKeyEvent* event) {
    unsigned int value = 0;
    if (event->modifiers() & Qt::ControlModifier) value |= MOD_CONTROL;
    if (event->modifiers() & Qt::AltModifier) value |= MOD_ALT;
    if (event->modifiers() & Qt::ShiftModifier) value |= MOD_SHIFT;
    if (event->modifiers() & Qt::MetaModifier) value |= MOD_WIN;
    return value;
}

bool isModifier(unsigned int key) {
    return key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ||
        key == VK_MENU || key == VK_LMENU || key == VK_RMENU ||
        key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT ||
        key == VK_LWIN || key == VK_RWIN;
}
bool extendedKey(unsigned int key) {
    return key == VK_RCONTROL || key == VK_RMENU || key == VK_LWIN || key == VK_RWIN ||
        (key >= VK_PRIOR && key <= VK_DOWN) || key == VK_INSERT || key == VK_DELETE || key == VK_DIVIDE;
}
} // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    load();
    holdTimer_.setInterval(15);
    connect(&holdTimer_, &QTimer::timeout, this, [this] {
        if (hotkeyHeld_ && !hotkeyStillPressed()) stop();
    });
    QCoreApplication::instance()->installEventFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(this);
}

Backend::~Backend() {
    stopEngine(false);
    save();
    QCoreApplication::instance()->removeNativeEventFilter(this);
    if (hotkeyRegistered_ && windowHandle_)
        UnregisterHotKey(static_cast<HWND>(windowHandle_), HOTKEY_ID);
}

void Backend::load() {
    QSettings file(settingsPath(), QSettings::IniFormat);
    file.beginGroup(QStringLiteral("Flint"));
    settings_.action = std::clamp(file.value(QStringLiteral("action"), 0).toInt(), 0, 3);
    settings_.intervalMs = std::clamp(file.value(QStringLiteral("interval"), 100).toInt(), 1, 3600000);
    settings_.randomOffset = file.value(QStringLiteral("random"), false).toBool();
    settings_.offsetMs = std::clamp(file.value(QStringLiteral("offset"), 0).toInt(), 0, settings_.intervalMs - 1);
    settings_.repeatForever = file.value(QStringLiteral("repeatForever"), true).toBool();
    settings_.repeatCount = std::clamp(file.value(QStringLiteral("repeat"), 100).toInt(), 1, 100000000);
    settings_.fixedLocation = file.value(QStringLiteral("fixedLocation"), false).toBool();
    settings_.x = file.value(QStringLiteral("x"), 0).toInt();
    settings_.y = file.value(QStringLiteral("y"), 0).toInt();
    settings_.dark = file.value(QStringLiteral("dark"), true).toBool();
    settings_.actionKey = static_cast<unsigned int>(std::clamp(file.value(QStringLiteral("actionKey"), int('E')).toInt(), 1, 255));
    settings_.actionModifiers = file.value(QStringLiteral("actionModifiers"), 0).toUInt() & MOD_BITS;
    settings_.customMouseButton = std::clamp(file.value(QStringLiteral("customMouseButton"), 0).toInt(), 0, 5);
    settings_.hotkey = static_cast<unsigned int>(std::clamp(file.value(QStringLiteral("hotkey"), int(VK_F6)).toInt(), 1, 255));
    settings_.hotkeyModifiers = file.value(QStringLiteral("hotkeyModifiers"), 0).toUInt() & MOD_BITS;
    settings_.holdToClick = file.value(QStringLiteral("holdToClick"), false).toBool();
}

void Backend::save() {
    QSettings file(settingsPath(), QSettings::IniFormat);
    file.beginGroup(QStringLiteral("Flint"));
    file.setValue(QStringLiteral("action"), settings_.action);
    file.setValue(QStringLiteral("interval"), settings_.intervalMs);
    file.setValue(QStringLiteral("random"), settings_.randomOffset);
    file.setValue(QStringLiteral("offset"), settings_.offsetMs);
    file.setValue(QStringLiteral("repeatForever"), settings_.repeatForever);
    file.setValue(QStringLiteral("repeat"), settings_.repeatCount);
    file.setValue(QStringLiteral("fixedLocation"), settings_.fixedLocation);
    file.setValue(QStringLiteral("x"), settings_.x);
    file.setValue(QStringLiteral("y"), settings_.y);
    file.setValue(QStringLiteral("dark"), settings_.dark);
    file.setValue(QStringLiteral("actionKey"), settings_.actionKey);
    file.setValue(QStringLiteral("actionModifiers"), settings_.actionModifiers);
    file.setValue(QStringLiteral("customMouseButton"), settings_.customMouseButton);
    file.setValue(QStringLiteral("hotkey"), settings_.hotkey);
    file.setValue(QStringLiteral("hotkeyModifiers"), settings_.hotkeyModifiers);
    file.setValue(QStringLiteral("holdToClick"), settings_.holdToClick);
    file.endGroup();
    file.sync();
}

int Backend::action() const { return settings_.action; }
int Backend::intervalMs() const { return settings_.intervalMs; }
bool Backend::randomOffset() const { return settings_.randomOffset; }
int Backend::offsetMs() const { return settings_.offsetMs; }
bool Backend::repeatForever() const { return settings_.repeatForever; }
int Backend::repeatCount() const { return settings_.repeatCount; }
bool Backend::fixedLocation() const { return settings_.fixedLocation; }
int Backend::locationX() const { return settings_.x; }
int Backend::locationY() const { return settings_.y; }
bool Backend::dark() const { return settings_.dark; }
QString Backend::actionShortcut() const {
    switch (settings_.customMouseButton) {
        case 1: return QStringLiteral("Left mouse");
        case 2: return QStringLiteral("Middle mouse");
        case 3: return QStringLiteral("Right mouse");
        case 4: return QStringLiteral("Mouse 4");
        case 5: return QStringLiteral("Mouse 5");
        default: return shortcutName(settings_.actionKey, settings_.actionModifiers);
    }
}
bool Backend::customIsMouse() const { return settings_.customMouseButton != 0; }
QString Backend::hotkeyShortcut() const { return shortcutName(settings_.hotkey, settings_.hotkeyModifiers); }
bool Backend::holdToClick() const { return settings_.holdToClick; }
QString Backend::captureTarget() const { return captureTarget_; }
bool Backend::running() const { return running_.load(); }
qulonglong Backend::completed() const { return completed_; }
QString Backend::status() const { return status_; }
int Backend::virtualScreenX() const { return virtualBounds().x(); }
int Backend::virtualScreenY() const { return virtualBounds().y(); }
int Backend::virtualScreenWidth() const { return virtualBounds().width(); }
int Backend::virtualScreenHeight() const { return virtualBounds().height(); }

void Backend::setAction(int value) {
    value = std::clamp(value, 0, 3);
    if (settings_.action == value) return;
    settings_.action = value; emit settingsChanged(); save();
}
void Backend::setIntervalMs(int value) {
    value = std::clamp(value, 1, 3600000);
    if (settings_.intervalMs == value) return;
    settings_.intervalMs = value;
    settings_.offsetMs = std::min(settings_.offsetMs, value - 1);
    emit settingsChanged(); save();
}
void Backend::setRandomOffset(bool value) {
    if (settings_.randomOffset == value) return;
    settings_.randomOffset = value; emit settingsChanged(); save();
}
void Backend::setOffsetMs(int value) {
    value = std::clamp(value, 0, settings_.intervalMs - 1);
    if (settings_.offsetMs == value) return;
    settings_.offsetMs = value; emit settingsChanged(); save();
}
void Backend::setRepeatForever(bool value) {
    if (settings_.repeatForever == value) return;
    settings_.repeatForever = value; emit settingsChanged(); save();
}
void Backend::setRepeatCount(int value) {
    value = std::clamp(value, 1, 100000000);
    if (settings_.repeatCount == value) return;
    settings_.repeatCount = value; emit settingsChanged(); save();
}
void Backend::setFixedLocation(bool value) {
    if (settings_.fixedLocation == value) return;
    settings_.fixedLocation = value; emit settingsChanged(); save();
}
void Backend::setLocationX(int value) {
    if (settings_.x == value) return;
    settings_.x = value; emit settingsChanged(); save();
}
void Backend::setLocationY(int value) {
    if (settings_.y == value) return;
    settings_.y = value; emit settingsChanged(); save();
}
void Backend::setDark(bool value) {
    if (settings_.dark == value) return;
    settings_.dark = value; emit settingsChanged(); save();
}
void Backend::setHoldToClick(bool value) {
    if (settings_.holdToClick == value) return;
    settings_.holdToClick = value; emit settingsChanged(); save();
}

bool Backend::hotkeyStillPressed() const {
    const auto pressed = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
    const unsigned int mods = settings_.hotkeyModifiers;
    return pressed(static_cast<int>(settings_.hotkey)) &&
        (!(mods & MOD_CONTROL) || pressed(VK_CONTROL)) &&
        (!(mods & MOD_ALT) || pressed(VK_MENU)) &&
        (!(mods & MOD_SHIFT) || pressed(VK_SHIFT)) &&
        (!(mods & MOD_WIN) || pressed(VK_LWIN) || pressed(VK_RWIN));
}

void Backend::attachWindow(QQuickWindow* window) {
    window_ = window;
    windowHandle_ = reinterpret_cast<void*>(window->winId());
    if (!registerHotkey(settings_.hotkey, settings_.hotkeyModifiers))
        setStatus(QStringLiteral("Hotkey unavailable; use Start"));
}

bool Backend::registerHotkey(unsigned int key, unsigned int modifiers) {
    if (!modifiers && key >= VK_F8 && key <= VK_F10) return false;
    if (!windowHandle_) return false;
    const HWND handle = static_cast<HWND>(windowHandle_);
    if (hotkeyRegistered_) {
        UnregisterHotKey(handle, HOTKEY_ID);
        hotkeyRegistered_ = false;
    }
    hotkeyRegistered_ = RegisterHotKey(handle, HOTKEY_ID, modifiers | MOD_NOREPEAT, key) != 0;
    return hotkeyRegistered_;
}

void Backend::beginCapture(const QString& target) {
    if (running_) return;
    if (target == QStringLiteral("hotkey") && hotkeyRegistered_ && windowHandle_) {
        UnregisterHotKey(static_cast<HWND>(windowHandle_), HOTKEY_ID);
        hotkeyRegistered_ = false;
    }
    captureTarget_ = target;
    if (window_) {
        window_->requestActivate();
        window_->contentItem()->setFocus(true);
    }
    emit captureChanged();
}
void Backend::beginActionCapture() { beginCapture(QStringLiteral("action")); }
void Backend::beginHotkeyCapture() { beginCapture(QStringLiteral("hotkey")); }
void Backend::cancelCapture() {
    if (captureTarget_.isEmpty()) return;
    if (captureTarget_ == QStringLiteral("hotkey") && !hotkeyRegistered_)
        registerHotkey(settings_.hotkey, settings_.hotkeyModifiers);
    captureTarget_.clear();
    emit captureChanged();
}

void Backend::finishCapture(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) { cancelCapture(); return; }
    const unsigned int key = virtualKey(event);
    if (!key || isModifier(key)) return;
    const unsigned int mods = modifiers(event);
    if (captureTarget_ == QStringLiteral("action")) {
        settings_.actionKey = key;
        settings_.actionModifiers = mods;
        settings_.customMouseButton = 0;
        emit settingsChanged();
        save();
    } else if (captureTarget_ == QStringLiteral("hotkey")) {
        const unsigned int oldKey = settings_.hotkey;
        const unsigned int oldMods = settings_.hotkeyModifiers;
        if (registerHotkey(key, mods)) {
            settings_.hotkey = key;
            settings_.hotkeyModifiers = mods;
            emit settingsChanged();
            save();
            setStatus(QStringLiteral("Hotkey updated"));
        } else {
            registerHotkey(oldKey, oldMods);
            setStatus(QStringLiteral("That hotkey is unavailable"));
        }
    }
    cancelCapture();
}

void Backend::finishMouseCapture(QMouseEvent* event) {
    int button = 0;
    switch (event->button()) {
        case Qt::LeftButton: button = 1; break;
        case Qt::MiddleButton: button = 2; break;
        case Qt::RightButton: button = 3; break;
        case Qt::BackButton: button = 4; break;
        case Qt::ForwardButton: button = 5; break;
        default: return;
    }
    settings_.customMouseButton = button;
    emit settingsChanged();
    save();
    cancelCapture();
}

bool Backend::eventFilter(QObject* watched, QEvent* event) {
    if (!captureTarget_.isEmpty() && event->type() == QEvent::KeyPress) {
        finishCapture(static_cast<QKeyEvent*>(event));
        return true;
    }
    if (captureTarget_ == QStringLiteral("action") && event->type() == QEvent::MouseButtonPress) {
        finishMouseCapture(static_cast<QMouseEvent*>(event));
        return true;
    }
    return QObject::eventFilter(watched, event);
}

bool Backend::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType.startsWith("windows")) {
        const MSG* msg = static_cast<MSG*>(message);
        // Generated input must not activate our controls or edit focused fields.
        // Physical clicks and keyboard input still reach the UI normally.
        if (static_cast<ULONG_PTR>(GetMessageExtraInfo()) == FLINT_INPUT_TAG &&
            (msg->message == WM_LBUTTONDOWN || msg->message == WM_LBUTTONUP ||
             msg->message == WM_MBUTTONDOWN || msg->message == WM_MBUTTONUP ||
             msg->message == WM_RBUTTONDOWN || msg->message == WM_RBUTTONUP ||
             msg->message == WM_XBUTTONDOWN || msg->message == WM_XBUTTONUP ||
             msg->message == WM_LBUTTONDBLCLK || msg->message == WM_MBUTTONDBLCLK ||
             msg->message == WM_RBUTTONDBLCLK || msg->message == WM_XBUTTONDBLCLK ||
             msg->message == WM_KEYDOWN || msg->message == WM_KEYUP ||
             msg->message == WM_SYSKEYDOWN || msg->message == WM_SYSKEYUP ||
             msg->message == WM_CHAR || msg->message == WM_SYSCHAR)) {
            if (result) *result = 0;
            return true;
        }
        if (msg->message == WM_HOTKEY && msg->wParam == HOTKEY_ID) {
            if (captureTarget_.isEmpty()) {
                if (settings_.holdToClick) {
                    if (!running_) {
                        start();
                        if (running_) {
                            hotkeyHeld_ = true;
                            holdTimer_.start();
                        }
                    }
                } else {
                    toggle();
                }
            }
            if (result) *result = 0;
            return true;
        }
    }
    return false;
}

void Backend::pickLocation() {
    POINT cursor{};
    if (!GetCursorPos(&cursor)) { setStatus(QStringLiteral("Could not read cursor position")); return; }
    settings_.x = cursor.x;
    settings_.y = cursor.y;
    settings_.fixedLocation = true;
    emit settingsChanged();
    save();
    setStatus(QStringLiteral("Location selected"));
}

void Backend::setStatus(const QString& text) {
    if (status_ == text) return;
    status_ = text;
    emit statusChanged();
}

bool Backend::sendAction(const Settings& settings) {
    if (settings.action == 3 && settings.customMouseButton == 0) {
        INPUT inputs[10]{};
        UINT size = 0;
        auto add = [&](unsigned int key, DWORD flags = 0) {
            inputs[size].type = INPUT_KEYBOARD;
            inputs[size].ki.wVk = static_cast<WORD>(key);
            inputs[size].ki.dwFlags = flags | (extendedKey(key) ? KEYEVENTF_EXTENDEDKEY : 0);
            inputs[size].ki.dwExtraInfo = FLINT_INPUT_TAG;
            ++size;
        };
        const unsigned int keys[]{VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN};
        const unsigned int bits[]{MOD_CONTROL, MOD_ALT, MOD_SHIFT, MOD_WIN};
        for (int i = 0; i < 4; ++i) if (settings.actionModifiers & bits[i]) add(keys[i]);
        add(settings.actionKey);
        add(settings.actionKey, KEYEVENTF_KEYUP);
        for (int i = 3; i >= 0; --i) if (settings.actionModifiers & bits[i]) add(keys[i], KEYEVENTF_KEYUP);
        const UINT sent = SendInput(size, inputs, sizeof(INPUT));
        if (sent == size) return true;
        // A partial batch may have pressed a key without delivering its release.
        for (UINT i = 0; i < sent; ++i) {
            if (inputs[i].ki.dwFlags & KEYEVENTF_KEYUP) continue;
            inputs[i].ki.dwFlags |= KEYEVENTF_KEYUP;
            SendInput(1, &inputs[i], sizeof(INPUT));
        }
        return false;
    }
    if (settings.fixedLocation && !SetCursorPos(settings.x, settings.y)) return false;
    const int mouseButton = settings.action == 3 ? settings.customMouseButton : settings.action + 1;
    DWORD down = MOUSEEVENTF_LEFTDOWN, up = MOUSEEVENTF_LEFTUP;
    if (mouseButton == 2) { down = MOUSEEVENTF_MIDDLEDOWN; up = MOUSEEVENTF_MIDDLEUP; }
    if (mouseButton == 3) { down = MOUSEEVENTF_RIGHTDOWN; up = MOUSEEVENTF_RIGHTUP; }
    if (mouseButton >= 4) { down = MOUSEEVENTF_XDOWN; up = MOUSEEVENTF_XUP; }
    INPUT inputs[2]{};
    inputs[0].type = inputs[1].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = down;
    inputs[1].mi.dwFlags = up;
    inputs[0].mi.dwExtraInfo = inputs[1].mi.dwExtraInfo = FLINT_INPUT_TAG;
    if (mouseButton >= 4) {
        inputs[0].mi.mouseData = inputs[1].mi.mouseData = mouseButton == 4 ? XBUTTON1 : XBUTTON2;
    }
    const UINT sent = SendInput(2, inputs, sizeof(INPUT));
    if (sent == 1) SendInput(1, &inputs[1], sizeof(INPUT));
    return sent == 2;
}

void Backend::toggle() { if (running_) stop(); else start(); }
void Backend::toggleFromButton() { toggle(); }

void Backend::start() {
    if (macroActive_) { setStatus(QStringLiteral("Stop the macro first")); return; }
    if (running_ || !windowHandle_) return;
    if (settings_.action == 3 && settings_.customMouseButton == 0 &&
        settings_.actionKey >= VK_F8 && settings_.actionKey <= VK_F10) {
        setStatus(QStringLiteral("F8-F10 are reserved for macro controls")); return;
    }
    if (settings_.action == 3 && settings_.customMouseButton == 0 && settings_.actionKey == settings_.hotkey &&
        settings_.actionModifiers == settings_.hotkeyModifiers) {
        setStatus(QStringLiteral("Choose a different start/stop hotkey"));
        return;
    }
    if (settings_.fixedLocation && (settings_.action != 3 || settings_.customMouseButton != 0)) {
        const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int right = left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int bottom = top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (settings_.x < left || settings_.x >= right || settings_.y < top || settings_.y >= bottom) {
            setStatus(QStringLiteral("Fixed position is outside your screens"));
            return;
        }
    }
    if (worker_.joinable()) worker_.join();
    completed_ = 0;
    workerCompleted_ = 0;
    emit completedChanged();
    running_ = true;
    emit runningChanged();
    const std::uint64_t run = ++runId_;
    const Settings snapshot = settings_;
    setStatus(QStringLiteral("Running"));

    worker_ = std::thread([this, snapshot, run] {
        std::mt19937 engine(std::random_device{}());
        std::uniform_int_distribution<int> jitter(-snapshot.offsetMs, snapshot.offsetMs);
        std::uint64_t count = 0;
        bool failed = false;
        auto lastReport = std::chrono::steady_clock::now();
        while (running_.load() && (snapshot.repeatForever || count < static_cast<std::uint64_t>(snapshot.repeatCount))) {
            if (!sendAction(snapshot)) { failed = true; break; }
            ++count;
            workerCompleted_.store(count, std::memory_order_relaxed);
            const auto now = std::chrono::steady_clock::now();
            if (now - lastReport >= std::chrono::milliseconds(100)) {
                QMetaObject::invokeMethod(this, [this, count, run] {
                    if (run != runId_) return;
                    completed_ = count;
                    emit completedChanged();
                }, Qt::QueuedConnection);
                lastReport = now;
            }
            if (!snapshot.repeatForever && count >= static_cast<std::uint64_t>(snapshot.repeatCount)) break;
            const int delay = snapshot.intervalMs + (snapshot.randomOffset ? jitter(engine) : 0);
            std::unique_lock lock(waitMutex_);
            waitCondition_.wait_for(lock, std::chrono::milliseconds(delay), [this] { return !running_.load(); });
        }
        QMetaObject::invokeMethod(this, [this, count, run, failed] {
            if (run != runId_) return;
            if (worker_.joinable()) worker_.join();
            hotkeyHeld_ = false;
            holdTimer_.stop();
            completed_ = count;
            emit completedChanged();
            running_ = false;
            emit runningChanged();
            setStatus(failed ? QStringLiteral("Input blocked by Windows") : QStringLiteral("Finished"));
        }, Qt::QueuedConnection);
    });
}

void Backend::stopEngine(bool showStoppedStatus) {
    hotkeyHeld_ = false;
    holdTimer_.stop();
    if (!running_ && !worker_.joinable()) return;
    {
        // Change the wait predicate under the same lock used by wait_for. Otherwise
        // Stop can lose its notification just before a long interval begins.
        std::lock_guard lock(waitMutex_);
        running_ = false;
    }
    waitCondition_.notify_all();
    if (worker_.joinable()) worker_.join();
    ++runId_;
    completed_ = workerCompleted_.load(std::memory_order_relaxed);
    emit completedChanged();
    emit runningChanged();
    if (showStoppedStatus) setStatus(QStringLiteral("Stopped"));
}

void Backend::stop() { stopEngine(true); }

void Backend::resetDefaults() {
    stopEngine(false);
    settings_ = Settings{};
    if (!registerHotkey(settings_.hotkey, settings_.hotkeyModifiers))
        setStatus(QStringLiteral("Default hotkey unavailable"));
    else setStatus(QStringLiteral("Defaults restored"));
    emit settingsChanged();
    save();
}
