#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "MacroBackend.hpp"
#include "Backend.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QQuickWindow>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {
constexpr ULONG_PTR InputTag = 0x464C494E;
constexpr int MaxSteps = 50000;
constexpr int MaxUndoStepReferences = 200000;
constexpr qint64 MaxFileSize = 64 * 1024 * 1024;
const QString StepMime = QStringLiteral("application/x-flintmacro-steps+json");
MacroBackend* recorder = nullptr;
QString sessionPath() {
    QString folder = qEnvironmentVariable("LOCALAPPDATA");
    if (folder.isEmpty()) folder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    else folder += "/FlintAutoClicker";
    QDir().mkpath(folder);
    return folder + "/macro-session.json";
}
bool reserved(int key) { return key == VK_F8 || key == VK_F9 || key == VK_F10; }
LRESULT CALLBACK keyboardProc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && recorder) {
        const auto* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        if (!(key->flags & LLKHF_INJECTED) && !reserved(key->vkCode))
            recorder->recordKeyboard(key->vkCode, message == WM_KEYDOWN || message == WM_SYSKEYDOWN,
                                     (key->flags & LLKHF_EXTENDED) != 0);
    }
    return CallNextHookEx(nullptr, code, message, data);
}
LRESULT CALLBACK mouseProc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && recorder) {
        const auto* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        if (!(mouse->flags & LLMHF_INJECTED)) {
            int button = 0, wheel = 0;
            bool down = false;
            switch (message) {
            case WM_LBUTTONDOWN: button = 1; down = true; break;
            case WM_LBUTTONUP: button = 1; break;
            case WM_MBUTTONDOWN: button = 2; down = true; break;
            case WM_MBUTTONUP: button = 2; break;
            case WM_RBUTTONDOWN: button = 3; down = true; break;
            case WM_RBUTTONUP: button = 3; break;
            case WM_XBUTTONDOWN: button = HIWORD(mouse->mouseData) == XBUTTON1 ? 4 : 5; down = true; break;
            case WM_XBUTTONUP: button = HIWORD(mouse->mouseData) == XBUTTON1 ? 4 : 5; break;
            case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: wheel = static_cast<short>(HIWORD(mouse->mouseData)); break;
            case WM_MOUSEMOVE: return CallNextHookEx(nullptr, code, message, data);
            default: return CallNextHookEx(nullptr, code, message, data);
            }
            recorder->recordMouse(button, down, mouse->pt.x, mouse->pt.y, wheel, message == WM_MOUSEHWHEEL);
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}
QString keyLabel(int key) {
    if (key >= VK_F1 && key <= VK_F24) return QString("F%1").arg(key - VK_F1 + 1);
    wchar_t text[80]{};
    if (GetKeyNameTextW(MapVirtualKeyW(key, MAPVK_VK_TO_VSC) << 16, text, 80))
        return QString::fromWCharArray(text);
    return QString("Key %1").arg(key);
}
QString buttonLabel(int button) {
    return QStringList{"Keyboard", "Left mouse", "Middle mouse", "Right mouse", "Mouse 4", "Mouse 5"}.value(button);
}
bool parseShortcut(const QString& text, QList<int>& keys) {
    if (text.size() > 100) return false;
    const QString single = text.trimmed().toLower();
    const QMap<QString, int> modifiers{{"ctrl", VK_CONTROL}, {"control", VK_CONTROL}, {"shift", VK_SHIFT},
                                     {"alt", VK_MENU}, {"win", VK_LWIN}, {"left ctrl", VK_LCONTROL},
                                     {"right ctrl", VK_RCONTROL}, {"left shift", VK_LSHIFT},
                                     {"right shift", VK_RSHIFT}, {"left alt", VK_LMENU}, {"right alt", VK_RMENU}};
    if (modifiers.contains(single)) { keys.append(modifiers[single]); return true; }
    const auto sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
    if (sequence.count() != 1) return false;
    const auto combination = sequence[0];
    const int qtKey = combination.key();
    int key = 0;
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) key = 'A' + qtKey - Qt::Key_A;
    else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) key = '0' + qtKey - Qt::Key_0;
    else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) key = VK_F1 + qtKey - Qt::Key_F1;
    else {
        switch (qtKey) {
        case Qt::Key_Space: key = VK_SPACE; break;
        case Qt::Key_Return: case Qt::Key_Enter: key = VK_RETURN; break;
        case Qt::Key_Tab: key = VK_TAB; break;
        case Qt::Key_Escape: key = VK_ESCAPE; break;
        case Qt::Key_Backspace: key = VK_BACK; break;
        case Qt::Key_Delete: key = VK_DELETE; break;
        case Qt::Key_Insert: key = VK_INSERT; break;
        case Qt::Key_Home: key = VK_HOME; break;
        case Qt::Key_End: key = VK_END; break;
        case Qt::Key_PageUp: key = VK_PRIOR; break;
        case Qt::Key_PageDown: key = VK_NEXT; break;
        case Qt::Key_Left: key = VK_LEFT; break;
        case Qt::Key_Right: key = VK_RIGHT; break;
        case Qt::Key_Up: key = VK_UP; break;
        case Qt::Key_Down: key = VK_DOWN; break;
        default:
            if (qtKey > 0 && qtKey < 128) {
                const SHORT translated = VkKeyScanW(static_cast<wchar_t>(qtKey));
                if (translated != -1) {
                    key = LOBYTE(translated);
                    if (HIBYTE(translated) & 1) keys.append(VK_SHIFT);
                }
            }
        }
    }
    if (!key || reserved(key)) return false;
    const auto mods = combination.keyboardModifiers();
    if (mods & Qt::ControlModifier) keys.append(VK_CONTROL);
    if (mods & Qt::AltModifier) keys.append(VK_MENU);
    if ((mods & Qt::ShiftModifier) && !keys.contains(VK_SHIFT)) keys.append(VK_SHIFT);
    if (mods & Qt::MetaModifier) keys.append(VK_LWIN);
    keys.append(key);
    return true;
}
bool extendedKey(int key) {
    return key == VK_RCONTROL || key == VK_RMENU || key == VK_LWIN || key == VK_RWIN ||
           (key >= VK_PRIOR && key <= VK_DOWN) || key == VK_INSERT || key == VK_DELETE || key == VK_DIVIDE;
}
}

MacroBackend::MacroBackend(Backend* clicker, QObject* parent) : QObject(parent), clicker_(clicker) {
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &MacroBackend::clipboardChanged);
    playbackTimer_.setSingleShot(true);
    playbackTimer_.setTimerType(Qt::PreciseTimer);
    connect(&playbackTimer_, &QTimer::timeout, this, &MacroBackend::tick);
    progressTimer_.setSingleShot(true);
    progressTimer_.setInterval(33); // Updating labels at 30 Hz is enough; input timing is independent.
    connect(&progressTimer_, &QTimer::timeout, this, &MacroBackend::progressChanged);
    sessionTimer_.setSingleShot(true);
    sessionTimer_.setInterval(500);
    connect(&sessionTimer_, &QTimer::timeout, this, &MacroBackend::saveSession);
    QFile session(sessionPath());
    if (session.open(QIODevice::ReadOnly) && session.size() <= MaxFileSize)
        readDocument(session.readAll(), true);
    QCoreApplication::instance()->installNativeEventFilter(this);
}
MacroBackend::~MacroBackend() {
    stop();
    saveSession();
    QCoreApplication::instance()->removeNativeEventFilter(this);
    for (int id = 2; id <= 4; ++id) if (window_) UnregisterHotKey(static_cast<HWND>(window_), id);
}
void MacroBackend::attachWindow(QQuickWindow* window) {
    window_ = reinterpret_cast<void*>(window->winId());
    QStringList unavailable;
    for (int id = 2; id <= 4; ++id)
        if (!RegisterHotKey(static_cast<HWND>(window_), id, MOD_NOREPEAT, VK_F8 + id - 2))
            unavailable.append(QString("F%1").arg(id + 6));
    if (!unavailable.isEmpty()) setStatus("Hotkey unavailable: " + unavailable.join(", ") + ". Use the buttons.");
}
bool MacroBackend::nativeEventFilter(const QByteArray& type, void* message, qintptr* result) {
    if (!type.startsWith("windows")) return false;
    auto* msg = static_cast<MSG*>(message);
    if (msg->message != WM_HOTKEY || msg->wParam < 2 || msg->wParam > 4) return false;
    if (msg->wParam != 4 && (editorOpen_ || !clicker_->captureTarget().isEmpty())) {
        if (result) *result = 0;
        return true;
    }
    if (msg->wParam == 4) { stop(); clicker_->stop(); }
    else if (msg->wParam == 2) { if (recording_) stop(); else if (!playing_) emit recordRequested(); }
    else if (!recording_) { if (playing_) stop(); else play(); }
    if (result) *result = 0;
    return true;
}
void MacroBackend::setStatus(const QString& text) { status_ = text; emit statusChanged(); }
MacroBackend::Snapshot MacroBackend::snapshot() const { return {steps_, name_, path_, loops_, loopGapMs_, speed_, customSpeed_}; }
void MacroBackend::restore(const Snapshot& value) {
    const bool sequenceChanged = steps_ != value.steps;
    steps_ = value.steps; name_ = value.name; path_ = value.path; loops_ = value.loops;
    loopGapMs_ = value.loopGapMs; speed_ = value.speed; customSpeed_ = value.customSpeed; changed(sequenceChanged);
}
void MacroBackend::checkpoint() {
    undo_.append(snapshot());
    qsizetype references = 0;
    for (const auto& state : undo_) references += state.steps.size();
    while (undo_.size() > 1 && (undo_.size() > 40 || references > MaxUndoStepReferences)) {
        references -= undo_.first().steps.size(); undo_.removeFirst();
    }
    redo_.clear();
}
void MacroBackend::changed(bool sequenceChanged) {
    dirty_ = true;
    if (sequenceChanged) emit stepsChanged();
    emit documentChanged(); sessionTimer_.start();
}
void MacroBackend::reportProgress(bool immediate) {
    if (immediate) { progressTimer_.stop(); emit progressChanged(); }
    else if (!progressTimer_.isActive()) progressTimer_.start();
}
void MacroBackend::setName(const QString& value) {
    if (busy() || name_ == value.trimmed() || value.trimmed().isEmpty()) return;
    checkpoint(); name_ = value.trimmed().left(100); changed(false);
}
void MacroBackend::setLoops(int value) {
    value = std::clamp(value, 0, 1000000);
    if (busy() || loops_ == value) return;
    checkpoint(); loops_ = value; changed(false);
}
void MacroBackend::setLoopGapMs(int value) {
    value = std::clamp(value, 0, 3600000);
    if (busy() || loopGapMs_ == value) return;
    checkpoint(); loopGapMs_ = value; changed(false);
}
void MacroBackend::setSpeed(double value) {
    if (busy() || !std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 10.0);
    if (speed_ == value && customSpeed_) return;
    checkpoint(); speed_ = value; customSpeed_ = true; changed(false);
}
void MacroBackend::setPresetSpeed(double value) {
    if (busy() || (value != 0.5 && value != 1 && value != 2) || (speed_ == value && !customSpeed_)) return;
    checkpoint(); speed_ = value; customSpeed_ = false; changed(false);
}
QVariantMap MacroBackend::defaultStep(const QString& type) const {
    POINT cursor{}; GetCursorPos(&cursor);
    return {{"type", type}, {"key", "E"}, {"keyCode", int('E')}, {"extended", false},
            {"button", type == "hold" ? 0 : 1}, {"x", int(cursor.x)}, {"y", int(cursor.y)},
            {"fixed", true}, {"keyLocation", false}, {"durationMs", 500}, {"delayMs", 0}, {"count", 1},
            {"intervalMs", 100}, {"amount", 120}, {"horizontal", false}};
}
QVariantMap MacroBackend::stepAt(int index) const { return steps_.value(index).toMap(); }
QVariantList MacroBackend::steps() const {
    QVariantList result;
    result.reserve(steps_.size());
    for (const auto& value : steps_) { auto step = value.toMap(); step["summary"] = summary(step); result.append(step); }
    return result;
}
QString MacroBackend::summary(const QVariantMap& s) const {
    const QString t = s["type"].toString();
    QString result;
    const QString position = QString("%1, %2").arg(s["x"].toInt()).arg(s["y"].toInt());
    if (t == "click") result = QString("%1 click %2%3").arg(buttonLabel(s["button"].toInt()), s["fixed"].toBool() ? "at " + position : "at cursor", s["count"].toInt() > 1 ? QString(" (%1 clicks)").arg(s["count"].toInt()) : "");
    else if (t == "press") result = QString("Press %1 × %2").arg(s["key"].toString()).arg(s["count"].toInt());
    else if (t == "hold") result = QString("Hold %1 for %2 ms").arg(s["button"].toInt() ? buttonLabel(s["button"].toInt()) : s["key"].toString()).arg(s["durationMs"].toInt());
    else if (t == "scroll") result = QString("Scroll %1 %2 notch(es)").arg(s["amount"].toInt() >= 0 ? (s["horizontal"].toBool() ? "right" : "up") : (s["horizontal"].toBool() ? "left" : "down")).arg(std::abs(s["amount"].toInt()) / 120.0);
    else if (t == "wait") result = QString("Wait %1 ms").arg(s["durationMs"].toInt());
    if (s["keyLocation"].toBool() && (t == "press" || (t == "hold" && !s["button"].toInt()))) result += " at " + position;
    return result;
}
bool MacroBackend::validateStep(const QVariantMap& s, QString& error) const {
    const QString type = s["type"].toString();
    if (!QStringList{"click", "press", "hold", "move", "scroll", "wait", "keyDown", "keyUp", "mouseDown", "mouseUp"}.contains(type)) { error = "Unknown action type"; return false; }
    const auto range = [&](const char* key, int min, int max) {
        if (!s.contains(key)) return false;
        bool ok = false; const double number = s[key].toDouble(&ok);
        return ok && std::isfinite(number) && std::floor(number) == number && number >= min && number <= max;
    };
    if (!range("delayMs", 0, 3600000) || !range("durationMs", 0, type == "wait" ? 7200000 : 3600000) || !range("intervalMs", 0, 3600000) ||
        !range("count", 1, 2147483647) || !range("button", 0, 5) || !range("x", -100000, 100000) ||
        !range("y", -100000, 100000) || !range("amount", -120000, 120000)) { error = "An action value is missing or out of range"; return false; }
    if ((type == "click" || type.startsWith("mouse")) && s["button"].toInt() == 0) { error = "Choose a mouse button"; return false; }
    if (type == "press" || (type == "hold" && s["button"].toInt() == 0)) {
        QList<int> keys;
        if (!s["recordedKey"].toBool() && !parseShortcut(s["key"].toString(), keys)) { error = "Use a key or shortcut such as E, Space, or Ctrl+C. F8–F10 control macros."; return false; }
    }
    if ((type.startsWith("key") || s["recordedKey"].toBool()) && (!range("keyCode", 1, 255) || reserved(s["keyCode"].toInt()))) { error = "Invalid or reserved key code"; return false; }
    if (s.contains("overlapMs") && (!range("overlapMs", 0, s["durationMs"].toInt()) || type != "hold")) {
        error = "Invalid hold timing"; return false;
    }
    return true;
}
bool MacroBackend::putStep(int index, const QVariantMap& value) {
    if (busy() || index < -1 || index >= steps_.size() || (index == -1 && steps_.size() >= MaxSteps)) return false;
    auto step = value;
    if (!QStringList{"click", "press", "hold", "scroll", "wait"}.contains(step["type"].toString())) {
        setStatus("Choose Click, Press key, Hold input, Scroll, or Wait"); return false;
    }
    step["delayMs"] = 0;
    if (step.value("recordedKey").toBool() && step["key"].toString() != keyLabel(step["keyCode"].toInt()))
        step.remove("recordedKey");
    if (step.contains("overlapMs"))
        step["overlapMs"] = std::min(step["overlapMs"].toInt(), step["durationMs"].toInt());
    QString error;
    if (!validateStep(step, error)) { setStatus(error); return false; }
    checkpoint();
    step.remove("summary");
    if (index == -1) steps_.append(step); else steps_[index] = step;
    changed(); setStatus("Step saved"); return true;
}
QList<int> MacroBackend::selectedRows(const QVariantList& indices) const {
    QList<int> rows;
    for (const auto& value : indices) {
        bool ok = false; const int row = value.toInt(&ok);
        if (ok && row >= 0 && row < steps_.size()) rows.append(row);
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
}
bool MacroBackend::canPaste() const {
    const auto* data = QGuiApplication::clipboard()->mimeData();
    return data && data->hasFormat(StepMime);
}
void MacroBackend::copySteps(const QVariantList& indices) {
    const auto rows = selectedRows(indices);
    if (rows.isEmpty()) return;
    QVariantList copied;
    for (int row : rows) copied.append(steps_[row]);
    const QByteArray json = QJsonDocument(QJsonObject{{"version", 1}, {"steps", QJsonArray::fromVariantList(copied)}}).toJson(QJsonDocument::Compact);
    auto* data = new QMimeData;
    data->setData(StepMime, json);
    data->setText(QString::fromUtf8(json));
    QGuiApplication::clipboard()->setMimeData(data);
}
void MacroBackend::cutSteps(const QVariantList& indices) {
    if (busy()) return;
    copySteps(indices); removeSteps(indices);
}
void MacroBackend::removeSteps(const QVariantList& indices) {
    if (busy()) return;
    const auto rows = selectedRows(indices);
    if (rows.isEmpty()) return;
    checkpoint();
    for (auto it = rows.crbegin(); it != rows.crend(); ++it) steps_.removeAt(*it);
    changed();
}
QVariantList MacroBackend::pasteSteps(int afterIndex) {
    if (busy() || !canPaste()) return {};
    const QByteArray bytes = QGuiApplication::clipboard()->mimeData()->data(StepMime);
    if (bytes.size() > MaxFileSize) { setStatus("Copied sequence is too large"); return {}; }
    const auto object = QJsonDocument::fromJson(bytes).object();
    auto copied = object["steps"].toArray().toVariantList();
    if (object["version"].toInt() != 1 || copied.isEmpty() || copied.size() + steps_.size() > MaxSteps) return {};
    QString error;
    for (const auto& item : copied) if (!validateStep(item.toMap(), error)) { setStatus("Could not paste: " + error); return {}; }
    copied = normalizeSteps(copied);
    if (copied.size() + steps_.size() > MaxSteps) { setStatus("Too many steps to paste"); return {}; }
    checkpoint();
    QVariantList selection;
    int insertAt = std::clamp(afterIndex + 1, 0, int(steps_.size()));
    for (const auto& item : copied) { steps_.insert(insertAt, item); selection.append(insertAt++); }
    changed(); return selection;
}
QVariantList MacroBackend::moveSteps(const QVariantList& indices, int delta) {
    const auto rows = selectedRows(indices);
    QVariantList result;
    for (int row : rows) result.append(row);
    if (busy() || rows.isEmpty() || (delta != -1 && delta != 1) ||
        rows.first() + delta < 0 || rows.last() + delta >= steps_.size()) return result;
    checkpoint();
    if (delta < 0) for (int row : rows) steps_.swapItemsAt(row, row - 1);
    else for (auto it = rows.crbegin(); it != rows.crend(); ++it) steps_.swapItemsAt(*it, *it + 1);
    result.clear(); for (int row : rows) result.append(row + delta);
    changed(); return result;
}
QVariantList MacroBackend::duplicateSteps(const QVariantList& indices) {
    const auto rows = selectedRows(indices);
    if (busy() || rows.isEmpty() || steps_.size() + rows.size() > MaxSteps) return {};
    QVariantList copied;
    for (int row : rows) copied.append(steps_[row]);
    checkpoint();
    QVariantList result;
    int insertAt = rows.last() + 1;
    for (const auto& item : copied) { steps_.insert(insertAt, item); result.append(insertAt++); }
    changed(); return result;
}
void MacroBackend::undo() { if (canUndo()) { redo_.append(snapshot()); restore(undo_.takeLast()); } }
void MacroBackend::redo() { if (canRedo()) { undo_.append(snapshot()); restore(redo_.takeLast()); } }
void MacroBackend::newMacro() {
    if (busy()) return;
    checkpoint(); steps_.clear(); name_ = "Untitled macro"; path_.clear(); loops_ = 0; loopGapMs_ = 100; speed_ = 1; customSpeed_ = false;
    changed(); dirty_ = false; emit documentChanged(); setStatus("New macro");
}
QByteArray MacroBackend::documentBytes(bool session) const {
    QJsonObject object{{"format", "FlintMacro"}, {"version", 1}, {"name", name_}, {"loops", loops_},
                       {"speed", speed_}, {"loopGapMs", loopGapMs_}, {"customSpeed", customSpeed_}, {"steps", QJsonArray::fromVariantList(steps_)}};
    if (session) { object["path"] = path_; object["dirty"] = dirty_; }
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}
bool MacroBackend::readDocument(const QByteArray& bytes, bool session) {
    QJsonParseError parseError;
    const auto json = QJsonDocument::fromJson(bytes, &parseError);
    const auto obj = json.object();
    if (parseError.error != QJsonParseError::NoError || obj["format"] != "FlintMacro" || obj["version"].toInt() != 1 ||
        !obj["steps"].isArray() || !obj["name"].isString() || obj["name"].toString().trimmed().isEmpty() ||
        obj["loops"].toDouble(-1) < 0 || obj["loops"].toDouble() > 1000000 ||
        obj["loops"].toDouble() != obj["loops"].toInt(-1) || !obj["speed"].isDouble() || obj["speed"].toDouble() < 0 || obj["speed"].toDouble() > 10) {
        setStatus("This is not a supported Flint macro file"); return false;
    }
    if (obj.contains("loopGapMs") && (!obj["loopGapMs"].isDouble() ||
        obj["loopGapMs"].toDouble() != obj["loopGapMs"].toInt(-1) ||
        obj["loopGapMs"].toInt(-1) < 0 || obj["loopGapMs"].toDouble() > 3600000)) {
        setStatus("Invalid loop gap"); return false;
    }
    const auto list = obj["steps"].toArray().toVariantList();
    if (list.size() > MaxSteps) { setStatus("Macro exceeds the 50,000 step limit"); return false; }
    QString error;
    for (const auto& value : list) if (!validateStep(value.toMap(), error)) { setStatus("Could not load: " + error); return false; }
    const auto normalized = normalizeSteps(list);
    if (normalized.size() > MaxSteps) { setStatus("Macro exceeds the 50,000 step limit"); return false; }
    steps_ = normalized; name_ = obj["name"].toString().left(100); loops_ = obj["loops"].toInt(); speed_ = obj["speed"].toDouble();
    loopGapMs_ = obj["loopGapMs"].toInt(100);
    customSpeed_ = obj.contains("customSpeed") ? obj["customSpeed"].toBool() : (speed_ != 0.5 && speed_ != 1 && speed_ != 2);
    dirty_ = session && obj["dirty"].toBool();
    if (session) path_ = obj["path"].toString();
    undo_.clear(); redo_.clear(); emit stepsChanged(); emit documentChanged(); return true;
}
bool MacroBackend::open(const QUrl& url) {
    if (busy() || !url.isLocalFile()) return false;
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxFileSize) { setStatus("Could not open macro (maximum file size: 64 MB)"); return false; }
    if (!readDocument(file.readAll(), false)) return false;
    path_ = url.toLocalFile(); name_ = QFileInfo(path_).completeBaseName(); emit documentChanged(); saveSession(); setStatus("Macro opened"); return true;
}
bool MacroBackend::save(const QUrl& url) {
    if (busy()) return false;
    QString target = url.isEmpty() ? path_ : url.toLocalFile();
    if (target.isEmpty()) { setStatus("Choose a file name first"); return false; }
    QSaveFile file(target);
    auto object = QJsonDocument::fromJson(documentBytes(false)).object();
    object["name"] = QFileInfo(target).completeBaseName();
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { setStatus("Could not save macro: " + file.errorString()); return false; }
    path_ = target; name_ = object["name"].toString(); dirty_ = false; emit documentChanged(); saveSession(); setStatus("Macro saved"); return true;
}
void MacroBackend::saveSession() {
    // A deferred autosave must not serialize a large sequence during playback.
    if (busy()) return;
    sessionTimer_.stop();
    QSaveFile file(sessionPath());
    const auto bytes = documentBytes(true);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()) file.commit();
}
void MacroBackend::startRecording() {
    if (busy()) return;
    clicker_->stop();
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, GetModuleHandleW(nullptr), 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, GetModuleHandleW(nullptr), 0);
    if (!keyboardHook_ || !mouseHook_) {
        if (keyboardHook_) UnhookWindowsHookEx(static_cast<HHOOK>(keyboardHook_));
        if (mouseHook_) UnhookWindowsHookEx(static_cast<HHOOK>(mouseHook_));
        keyboardHook_ = mouseHook_ = nullptr; setStatus("Windows could not start the recorder"); return;
    }
    checkpoint(); recorded_.clear(); recordedKeys_.clear(); recordedButtons_.clear();
    lastRecordTime_ = 0; recordClock_.start();
    recording_ = true; recorder = this; clicker_->setMacroActive(true);
    emit stateChanged(); emit documentChanged(); reportProgress(true);
    setStatus("Recording · F8 or F10 to stop");
}
void MacroBackend::appendRecorded(QVariantMap step) {
    if (!recording_ || recorded_.size() >= MaxSteps) return;
    const auto time = recordClock_.elapsed();
    step["delayMs"] = int(std::min<qint64>(3600000, time - lastRecordTime_));
    recorded_.append(step); lastRecordTime_ = time;
    reportProgress();
    if (recorded_.size() >= MaxSteps - 512) QTimer::singleShot(0, this, [this] { if (recording_) { stop(); setStatus("Recording reached its step limit"); } });
}
void MacroBackend::recordKeyboard(unsigned int key, bool down, bool extended) {
    if (!recording_ || reserved(key) || key == 0 || key > 255) return;
    if (down && recordedKeys_.contains(key)) return; // Ignore keyboard auto-repeat.
    if (!down && !recordedKeys_.contains(key)) return;
    if (down) recordedKeys_[key] = extended; else recordedKeys_.erase(key);
    auto step = defaultStep(down ? "keyDown" : "keyUp");
    step["keyCode"] = int(key); step["key"] = keyLabel(key); step["extended"] = extended; appendRecorded(step);
}
void MacroBackend::recordMouse(int button, bool down, int x, int y, int wheel, bool horizontal) {
    if (!recording_ || (!button && !wheel)) return; // Pointer movement is never an action.
    const HWND target = GetAncestor(WindowFromPoint(POINT{x, y}), GA_ROOT);
    if (button && down && recordedButtons_.contains(button)) return;
    if (button && !down && !recordedButtons_.contains(button)) return;
    if (button) { if (down) recordedButtons_.insert(button); else recordedButtons_.erase(button); }
    auto step = defaultStep(wheel ? "scroll" : (down ? "mouseDown" : "mouseUp"));
    step["x"] = x; step["y"] = y; step["durationMs"] = 0;
    if (button) step["button"] = button;
    step["_ownWindow"] = window_ && target == static_cast<HWND>(window_);
    if (wheel) { step["amount"] = wheel; step["horizontal"] = horizontal; }
    appendRecorded(step);
}

// Convert old recordings to editable holds and explicit waits. Overlap stores how
// much of a hold continues while the next action runs (for example Ctrl+C).
QVariantList MacroBackend::normalizeSteps(const QVariantList& input) const {
    bool needsConversion = false;
    for (const auto& value : input) {
        const auto s = value.toMap(); const auto type = s["type"].toString();
        if (s["delayMs"].toInt() || type == "move" || type.startsWith("key") || type.startsWith("mouse")) needsConversion = true;
    }
    if (!needsConversion) return input;
    struct Timed { QVariantMap step; double start; };
    QVector<Timed> actions;
    std::map<int, int> keys, buttons;
    double time = 0;
    const auto finish = [&](int index) {
        actions[index].step["durationMs"] = int(std::clamp(time - actions[index].start, 1.0, 3600000.0));
    };
    for (const auto& value : input) {
        auto s = value.toMap(); const auto type = s["type"].toString();
        time += s["delayMs"].toInt(); s["delayMs"] = 0; s.remove("_ownWindow"); s.remove("summary");
        if (type == "move" || type == "wait") { time += s["durationMs"].toInt(); continue; }
        if (type.startsWith("key") || type.startsWith("mouse")) {
            const bool keyboard = type.startsWith("key");
            auto& active = keyboard ? keys : buttons;
            const int code = s[keyboard ? "keyCode" : "button"].toInt();
            if (type.endsWith("Down")) {
                if (active.contains(code)) continue;
                active[code] = actions.size(); s["type"] = "hold"; s["durationMs"] = 1;
                if (keyboard) { s["button"] = 0; s["recordedKey"] = true; s["key"] = keyLabel(code); }
                actions.append({s, time});
            } else if (active.contains(code)) { finish(active[code]); active.erase(code); }
            continue;
        }
        actions.append({s, time});
        if (type == "hold") time += s["durationMs"].toInt() - s.value("overlapMs").toInt();
        else if (type == "click" || type == "press") time += double(s["count"].toInt() - 1) * s["intervalMs"].toInt();
    }
    for (const auto& [key, index] : keys) finish(index);
    for (const auto& [button, index] : buttons) finish(index);
    QVariantList result;
    const auto wait = [&](double duration) {
        while (duration >= 1 && result.size() <= MaxSteps) {
            auto step = defaultStep("wait"); step["durationMs"] = int(std::min(duration, 7200000.0));
            result.append(step); duration -= step["durationMs"].toInt();
        }
    };
    double cursor = 0;
    for (int i = 0; i < actions.size(); ++i) {
        auto s = actions[i].step; const auto type = s["type"].toString();
        const double start = actions[i].start;
        wait(start - cursor); cursor = start;
        if (type == "hold") {
            const double end = start + s["durationMs"].toInt();
            const double next = i + 1 < actions.size() ? actions[i + 1].start : std::max(end, time);
            const int overlap = int(std::max(0.0, end - next));
            s.remove("overlapMs"); if (overlap) s["overlapMs"] = overlap;
            cursor = end - overlap;
        } else if (type == "click" || type == "press")
            cursor += double(s["count"].toInt() - 1) * s["intervalMs"].toInt();
        result.append(s);
    }
    wait(time - cursor);
    return result;
}

bool MacroBackend::compile() {
    events_.clear();
    double cursor = 0;
    for (int index = 0; index < steps_.size(); ++index) {
        const auto s = steps_[index].toMap();
        QString error;
        if (!validateStep(s, error)) { setStatus(error); return false; }
        const QString type = s["type"].toString();
        Event base; base.step = index; base.x = s["x"].toInt(); base.y = s["y"].toInt();
        base.button = s["button"].toInt(); base.positioned = s["fixed"].toBool();
        int order = 0;
        const auto add = [&](Event event, double offset = 0) {
            event.at = cursor + offset; event.order = order++; events_.append(event);
        };
        if (type == "wait") { add(base); cursor += s["durationMs"].toInt(); add(base); }
        else if (type == "scroll") {
            base.kind = Event::Wheel; base.amount = s["amount"].toInt(); base.horizontal = s["horizontal"].toBool(); add(base);
        } else {
            const bool hold = type == "hold";
            const double duration = hold ? s["durationMs"].toInt() : 0;
            base.remaining = hold ? 1 : s["count"].toInt();
            base.interval = s["intervalMs"].toInt();
            if (type == "click" || (hold && base.button)) {
                base.kind = Event::Mouse; base.down = true; add(base);
                base.down = false; base.positioned = false; add(base, duration);
            } else {
                QList<int> keys;
                if (s["recordedKey"].toBool()) keys.append(s["keyCode"].toInt());
                else parseShortcut(s["key"].toString(), keys);
                base.kind = Event::Key;
                for (int j = 0; j < keys.size(); ++j) {
                    base.key = keys[j]; base.extended = s["recordedKey"].toBool() ? s["extended"].toBool() : extendedKey(base.key);
                    base.down = true; base.positioned = j == 0 && s["keyLocation"].toBool(); add(base);
                }
                for (int j = keys.size() - 1; j >= 0; --j) {
                    base.key = keys[j]; base.extended = s["recordedKey"].toBool() ? s["extended"].toBool() : extendedKey(base.key);
                    base.down = false; base.positioned = false; add(base, duration);
                }
            }
            cursor += hold ? duration - s.value("overlapMs").toInt() : double(base.remaining - 1) * base.interval;
        }
    }
    return !events_.isEmpty();
}
bool MacroBackend::dispatch(const Event& event) {
    if (event.kind == Event::Wait) return true;
    if (event.positioned) {
        const int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        if (event.x < left || event.y < top || event.x >= left + GetSystemMetrics(SM_CXVIRTUALSCREEN) || event.y >= top + GetSystemMetrics(SM_CYVIRTUALSCREEN)) return false;
        if (!SetCursorPos(event.x, event.y)) return false;
    }
    INPUT input{};
    if (event.kind == Event::Key) {
        input.type = INPUT_KEYBOARD; input.ki.wVk = event.key;
        input.ki.dwFlags = (event.down ? 0 : KEYEVENTF_KEYUP) | (event.extended ? KEYEVENTF_EXTENDEDKEY : 0);
        input.ki.dwExtraInfo = InputTag;
    } else {
        input.type = INPUT_MOUSE; input.mi.dwExtraInfo = InputTag;
        if (event.kind == Event::Wheel) { input.mi.dwFlags = event.horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL; input.mi.mouseData = event.amount; }
        else {
            const DWORD downs[] = {0, MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_XDOWN, MOUSEEVENTF_XDOWN};
            const DWORD ups[] = {0, MOUSEEVENTF_LEFTUP, MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_XUP, MOUSEEVENTF_XUP};
            input.mi.dwFlags = event.down ? downs[event.button] : ups[event.button];
            if (event.button >= 4) input.mi.mouseData = event.button == 4 ? XBUTTON1 : XBUTTON2;
        }
    }
    if (SendInput(1, &input, sizeof(INPUT)) != 1) return false;
    if (event.kind == Event::Key) { if (event.down) heldKeys_[event.key] = event.extended; else heldKeys_.erase(event.key); }
    if (event.kind == Event::Mouse) { if (event.down) heldButtons_.insert(event.button); else heldButtons_.erase(event.button); }
    return true;
}
void MacroBackend::releaseInputs() {
    const auto keys = heldKeys_; const auto buttons = heldButtons_;
    for (const auto& [key, extended] : keys) { Event e; e.kind = Event::Key; e.key = key; e.extended = extended; dispatch(e); }
    for (int button : buttons) { Event e; e.kind = Event::Mouse; e.button = button; dispatch(e); }
    heldKeys_.clear(); heldButtons_.clear();
}
void MacroBackend::play() {
    if (busy()) return;
    if (speed_ <= 0) { setStatus("Choose a speed above 0 to play"); return; }
    if (steps_.isEmpty()) { setStatus("Add or record some steps first"); return; }
    if (!compile()) return;
    clicker_->stop(); clicker_->setMacroActive(true);
    playing_ = true; currentLoop_ = 1; currentStep_ = 0;
    resetPlaybackQueue();
    loopStart_ = 0; playbackClock_.start();
    emit stateChanged(); emit documentChanged(); reportProgress(true); setStatus("Playing - F10 to stop");
    playbackTimer_.start(0);
}
void MacroBackend::resetPlaybackQueue() {
    pending_ = decltype(pending_)(Later{}, std::vector<Event>(events_.cbegin(), events_.cend()));
}
void MacroBackend::tick() {
    if (!playing_) return;
    // Repetitions are scheduled lazily, so a billion clicks use the same memory as one.
    // Bound each batch so Stop and the UI remain responsive even with zero-length actions.
    int processed = 0;
    QElapsedTimer batch; batch.start();
    while (playing_ && processed++ < 100 && batch.elapsed() < 4) {
        if (pending_.empty()) {
            releaseInputs();
            if (loops_ && currentLoop_ >= loops_) { stop(); setStatus("Playback finished"); return; }
            ++currentLoop_; currentStep_ = 0; reportProgress();
            loopStart_ = playbackClock_.elapsed() + std::max(1, loopGapMs_);
            resetPlaybackQueue();
        }
        const double remaining = loopStart_ + pending_.top().at / speed_ - playbackClock_.elapsed();
        if (remaining > 0) { playbackTimer_.start(int(std::min(60000.0, std::ceil(remaining)))); return; }
        auto event = pending_.top(); pending_.pop();
        if (currentStep_ < event.step) { currentStep_ = event.step; reportProgress(); }
        if (!dispatch(event)) { stop(); setStatus("Playback stopped: input blocked or position outside your screens"); return; }
        if (--event.remaining > 0) { event.at += event.interval; ++event.repetition; pending_.push(event); }
    }
    playbackTimer_.start(1); // Yield between batches, including zero-gap repeated actions.
}
void MacroBackend::stop(bool fromButton) {
    playbackTimer_.stop();
    pending_ = {};
    events_ = {}; events_.squeeze();
    if (recording_) {
        recording_ = false; recorder = nullptr;
        UnhookWindowsHookEx(static_cast<HHOOK>(keyboardHook_)); UnhookWindowsHookEx(static_cast<HHOOK>(mouseHook_));
        keyboardHook_ = mouseHook_ = nullptr;
        // The Stop button itself must not become part of the recorded sequence.
        if (fromButton && !recorded_.isEmpty()) {
            bool foundRelease = false;
            for (int i = recorded_.size() - 1; i >= 0; --i) {
                auto s = recorded_[i].toMap();
                const auto type = s["type"].toString();
                if (s["button"].toInt() != 1 || !s["_ownWindow"].toBool()) break;
                if (type == "mouseUp" && !foundRelease) foundRelease = true;
                else if (type != "mouseDown" || !foundRelease) break;
                s["type"] = "move"; s["durationMs"] = 0; recorded_[i] = s;
                if (type == "mouseDown") break;
            }
        }
        if (!recorded_.isEmpty()) {
            auto wait = defaultStep("wait");
            wait["durationMs"] = int(std::min<qint64>(3600000, recordClock_.elapsed() - lastRecordTime_));
            recorded_.append(wait);
        }
        // Close any inputs still held when recording was stopped.
        for (const auto& [key, extended] : recordedKeys_) { auto s = defaultStep("keyUp"); s["keyCode"] = key; s["key"] = keyLabel(key); s["extended"] = extended; recorded_.append(s); }
        for (int button : recordedButtons_) { auto s = defaultStep("mouseUp"); s["button"] = button; s["fixed"] = false; recorded_.append(s); }
        steps_ = normalizeSteps(recorded_); recorded_ = {}; recorded_.squeeze();
        recordedKeys_.clear(); recordedButtons_.clear(); changed();
    }
    releaseInputs(); playing_ = false; currentStep_ = -1; clicker_->setMacroActive(false);
    if (dirty_) sessionTimer_.start();
    emit stateChanged(); emit documentChanged(); reportProgress(true); setStatus("Stopped");
}
void MacroBackend::pickLocation() { POINT point{}; if (GetCursorPos(&point)) emit locationPicked(point.x, point.y); }
