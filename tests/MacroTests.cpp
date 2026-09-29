#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include "Backend.hpp"
#include "MacroBackend.hpp"
#include <QFile>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QClipboard>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QQuickItem* visualItem(QQuickItem* parent, const QString& name) {
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems()) if (auto* item = visualItem(child, name)) return item;
    return nullptr;
}
QList<bool> injectedKeys, injectedButtons;
QList<int> injectedKeyCodes;
QList<DWORD> injectedKeyFlags;
LRESULT CALLBACK blockTestKeys(int code, WPARAM message, LPARAM data) {
    const auto* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
    if (code == HC_ACTION && key->dwExtraInfo == 0x464C494E) {
        injectedKeyCodes.append(key->vkCode);
        injectedKeyFlags.append(key->flags);
        injectedKeys.append(message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
        return 1; // Verify injection without delivering it to another app.
    }
    return CallNextHookEx(nullptr, code, message, data);
}
LRESULT CALLBACK blockTestButtons(int code, WPARAM message, LPARAM data) {
    const auto* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(data);
    if (code == HC_ACTION && mouse->dwExtraInfo == 0x464C494E) {
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) injectedButtons.append(message == WM_LBUTTONDOWN);
        return 1;
    }
    return CallNextHookEx(nullptr, code, message, data);
}
struct InputBlocker {
    HHOOK keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, blockTestKeys, GetModuleHandleW(nullptr), 0);
    HHOOK mouse = SetWindowsHookExW(WH_MOUSE_LL, blockTestButtons, GetModuleHandleW(nullptr), 0);
    ~InputBlocker() { if (keyboard) UnhookWindowsHookEx(keyboard); if (mouse) UnhookWindowsHookEx(mouse); }
};
}

class MacroTests : public QObject {
    Q_OBJECT
private slots:
    void editingAndPersistence() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        auto press = macro.defaultStep("press"); press["key"] = "Ctrl+C"; press["count"] = 5;
        QVERIFY(macro.putStep(-1, press));
        auto wait = macro.defaultStep("wait"); wait["durationMs"] = 17;
        QVERIFY(macro.putStep(-1, wait));
        macro.moveSteps({1}, -1);
        QCOMPARE(macro.stepAt(0)["type"].toString(), "wait");
        macro.undo(); QCOMPARE(macro.stepAt(0)["type"].toString(), "press");
        macro.redo(); QCOMPARE(macro.stepAt(0)["type"].toString(), "wait");
        macro.duplicateSteps({1}); QCOMPARE(macro.steps().size(), 3);
        macro.removeSteps({2}); QCOMPARE(macro.steps().size(), 2);
        macro.setName("Test macro"); macro.setLoops(3); macro.setSpeed(2); macro.setLoopGapMs(42);
        QTemporaryDir files(QDir::currentPath() + "/macro-files-XXXXXX");
        QVERIFY2(files.isValid(), qPrintable(files.errorString()));
        const auto path = QUrl::fromLocalFile(files.filePath("roundtrip.flintmacro"));
        QVERIFY2(macro.save(path), qPrintable(macro.status())); QVERIFY(!macro.dirty());
        const auto savedSteps = macro.steps();
        macro.newMacro(); QVERIFY(macro.open(path));
        QCOMPARE(macro.steps(), savedSteps); QCOMPARE(macro.name(), "roundtrip");
        QCOMPARE(macro.loops(), 3); QCOMPARE(macro.speed(), 2.0); QCOMPARE(macro.loopGapMs(), 42);
        QFile invalid(files.filePath("invalid.flintmacro"));
        QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("{\"version\":999}"); invalid.close();
        QVERIFY(!macro.open(QUrl::fromLocalFile(invalid.fileName())));
        QCOMPARE(macro.steps(), savedSteps);
        press["key"] = "F10"; QVERIFY(!macro.putStep(-1, press));
        wait["durationMs"] = -1; QVERIFY(!macro.putStep(-1, wait));
    }
    void finitePlaybackAndStop() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        auto wait = macro.defaultStep("wait"); wait["durationMs"] = 80;
        QVERIFY(macro.putStep(-1, wait)); macro.setLoops(3); macro.setSpeed(2);
        QElapsedTimer elapsed; elapsed.start(); macro.play(); QVERIFY(macro.playing());
        QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 2000);
        QCOMPARE(macro.currentLoop(), 3); QVERIFY(elapsed.elapsed() >= 120);
        macro.setLoops(0); macro.play(); QTest::qWait(180); QVERIFY(macro.playing());
        macro.stop(); QVERIFY(!macro.playing());
        wait["durationMs"] = 60000; QVERIFY(macro.putStep(0, wait));
        macro.play(); elapsed.restart(); macro.stop();
        QVERIFY(elapsed.elapsed() < 100); QVERIFY(!macro.busy());
    }
    void loopGapAndProgressBudget() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        QCOMPARE(macro.loopGapMs(), 100);
        auto step = macro.defaultStep("wait"); step["durationMs"] = 0;
        QVERIFY(macro.putStep(-1, step)); macro.setLoops(3); macro.setPresetSpeed(2);
        QElapsedTimer elapsed; elapsed.start(); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 2000);
        QVERIFY(elapsed.elapsed() >= 200); // Two gaps, unaffected by 2x speed.
        QCOMPARE(macro.currentLoop(), 3);
        macro.setLoopGapMs(60000); macro.setLoops(1); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 500); // No gap before/after a single run.
        QSignalSpy modelChanges(&macro, &MacroBackend::stepsChanged);
        macro.setLoops(0); macro.setLoopGapMs(0); macro.setPresetSpeed(1);
        QSignalSpy progress(&macro, &MacroBackend::progressChanged);
        macro.play(); QTest::qWait(250); macro.stop();
        QVERIFY(macro.currentLoop() > 1);
        QVERIFY(progress.size() < 20); // Fast playback must not flood QML bindings.
        QCOMPARE(modelChanges.size(), 0); // Settings and playback do not rebuild the sequence list.
        macro.setLoopGapMs(60000); macro.play(); QTest::qWait(10);
        elapsed.restart(); macro.stop(); QVERIFY(elapsed.elapsed() < 100);
    }
    void autoclickerFinalCountAndExtendedKeys() {
        InputBlocker blocker;
        if (!blocker.keyboard || !blocker.mouse) QSKIP("Input interception unavailable");
        Backend clicker; QQuickWindow window; clicker.attachWindow(&window);
        clicker.setAction(0); clicker.setFixedLocation(false); clicker.setIntervalMs(60000);
        clicker.setRepeatForever(true); injectedButtons.clear();
        clicker.start(); QVERIFY(clicker.running());
        QTRY_COMPARE_WITH_TIMEOUT(injectedButtons.size(), 2, 1000);
        clicker.stop(); QCOMPARE(clicker.completed(), qulonglong(1));
        clicker.setAction(3); clicker.beginActionCapture();
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, 0, VK_RIGHT, 0);
        QCoreApplication::sendEvent(&window, &key);
        injectedKeys.clear(); injectedKeyFlags.clear();
        clicker.start(); QVERIFY(clicker.running());
        QTRY_COMPARE_WITH_TIMEOUT(injectedKeys.size(), 2, 1000);
        clicker.stop(); QCOMPARE(clicker.completed(), qulonglong(1));
        QVERIFY(injectedKeyFlags[0] & LLKHF_EXTENDED);
        QVERIFY(injectedKeyFlags[1] & LLKHF_EXTENDED);
        MSG message{}; message.message = WM_KEYDOWN;
        const auto previous = SetMessageExtraInfo(0x464C494E);
        const bool filtered = clicker.nativeEventFilter("windows_generic_MSG", &message, nullptr);
        SetMessageExtraInfo(previous); QVERIFY(filtered);
    }
    void sustainedPlaybackResources() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        auto step = macro.defaultStep("wait"); step["durationMs"] = 0;
        QVERIFY(macro.putStep(-1, step));
        const auto cpuMs = [] {
            FILETIME creation{}, exit{}, kernel{}, user{};
            GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);
            const auto value = [](FILETIME t) { return (quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
            return (value(kernel) + value(user)) / 10000.0;
        };
        const auto privateBytes = [] {
            PROCESS_MEMORY_COUNTERS_EX counters{}; counters.cb = sizeof(counters);
            GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
            return quint64(counters.PrivateUsage);
        };
        // No input injection: measure scheduling alone with the default loop gap.
        QTest::qWait(550); // Finish autosave before sampling.
        const auto memoryBefore = privateBytes();
        const auto cpuBefore = cpuMs(); QElapsedTimer elapsed; elapsed.start();
        macro.play(); QTest::qWait(1000); macro.stop();
        const auto loops = macro.currentLoop();
        QVERIFY(loops >= 2 && loops <= 11);
        qInfo("Default-gap scheduler: %llu loops, %.2f CPU ms / %lld wall ms, private %.1f MiB (delta %.2f MiB)",
              loops, cpuMs() - cpuBefore, elapsed.elapsed(), privateBytes() / 1048576.0,
              (double(privateBytes()) - memoryBefore) / 1048576.0);
        // Long-running zero-gap repeated input still returns control to the event loop.
        auto press = macro.defaultStep("press"); press["key"] = "F24";
        press["count"] = 2147483647; press["intervalMs"] = 0;
        QVERIFY(macro.putStep(0, press));
        InputBlocker blocker; if (!blocker.keyboard) QSKIP("Input interception unavailable");
        macro.play(); QTimer::singleShot(100, &macro, [&] { macro.stop(); });
        elapsed.restart(); QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 1000);
        QVERIFY(elapsed.elapsed() < 500);
    }
    void bulkEditsAndClipboard() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        QCOMPARE(macro.loops(), 0);
        for (int duration : {10, 20, 30, 40}) {
            auto step = macro.defaultStep("wait"); step["durationMs"] = duration;
            QVERIFY(macro.putStep(-1, step));
        }
        QCOMPARE(macro.moveSteps({1, 2}, -1), QVariantList({0, 1}));
        QCOMPARE(macro.stepAt(0)["durationMs"].toInt(), 20);
        QCOMPARE(macro.stepAt(1)["durationMs"].toInt(), 30);
        macro.undo(); QCOMPARE(macro.stepAt(0)["durationMs"].toInt(), 10);
        QCOMPARE(macro.moveSteps({0, 2}, -1), QVariantList({0, 2}));
        QCOMPARE(macro.moveSteps({1, 3}, 1), QVariantList({1, 3}));
        QCOMPARE(macro.moveSteps({0, 2}, 1), QVariantList({1, 3}));
        macro.undo();
        macro.copySteps({1, 2}); QVERIFY(macro.canPaste());
        QCOMPARE(macro.pasteSteps(3), QVariantList({4, 5}));
        QCOMPARE(macro.stepAt(4)["durationMs"].toInt(), 20);
        QCOMPARE(macro.stepAt(5)["durationMs"].toInt(), 30);
        macro.undo(); QCOMPARE(macro.steps().size(), 4);
        macro.cutSteps({0, 2}); QCOMPARE(macro.steps().size(), 2);
        QCOMPARE(macro.stepAt(0)["durationMs"].toInt(), 20);
        macro.undo(); QCOMPARE(macro.steps().size(), 4);
        QCOMPARE(macro.duplicateSteps({0, 2}), QVariantList({3, 4}));
        QCOMPARE(macro.stepAt(3)["durationMs"].toInt(), 10);
        QCOMPARE(macro.stepAt(4)["durationMs"].toInt(), 30);
        macro.undo(); QCOMPARE(macro.steps().size(), 4);
        macro.setSpeed(1.75); QCOMPARE(macro.speed(), 1.75);
        QVERIFY(macro.customSpeed());
        macro.setSpeed(20); QCOMPARE(macro.speed(), 10.0);
        macro.setSpeed(-5); QCOMPARE(macro.speed(), 0.0);
        macro.play(); QVERIFY(!macro.playing()); QVERIFY(macro.status().contains("above 0"));
        macro.setPresetSpeed(2); QCOMPARE(macro.speed(), 2.0); QVERIFY(!macro.customSpeed());
        macro.undo(); QCOMPARE(macro.speed(), 0.0); QVERIFY(macro.customSpeed());
    }
    void positionedKeysRejectOffscreenTarget() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        auto step = macro.defaultStep("press");
        step["key"] = "F24"; step["keyLocation"] = true; step["x"] = -100000; step["y"] = -100000;
        QVERIFY(macro.putStep(-1, step)); macro.setLoops(1); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 1000);
        QVERIFY(macro.status().contains("outside"));
    }
    void recordingPreservesOverlaps() {
        InputBlocker blocker;
        if (!blocker.keyboard || !blocker.mouse) QSKIP("Input interception unavailable");
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        QQuickWindow window; window.show(); window.requestActivate(); macro.attachWindow(&window);
        macro.startRecording();
        QVERIFY2(macro.recording(), qPrintable(macro.status()));
        macro.recordMouse(0, false, 10, 20, 0, false);
        macro.recordKeyboard(VK_LCONTROL, true, false);
        QTest::qWait(20);
        macro.recordKeyboard('C', true, false);
        macro.recordKeyboard('C', true, false); // Ignore auto-repeat.
        macro.recordMouse(0, false, 20, 30, 0, false);
        QTest::qWait(20);
        macro.recordKeyboard('C', false, false);
        QTest::qWait(20);
        macro.recordKeyboard(VK_LCONTROL, false, false);
        macro.recordKeyboard('E', true, false); // Stop closes the hold.
        QTest::qWait(20); macro.stop();
        QVariantList holds;
        for (const auto& value : macro.steps()) {
            const auto s = value.toMap();
            QVERIFY(s["type"] == "hold" || s["type"] == "wait");
            QCOMPARE(s["delayMs"].toInt(), 0);
            if (s["type"] == "hold") holds.append(s);
        }
        QCOMPARE(holds.size(), 3);
        QCOMPARE(holds[0].toMap()["keyCode"].toInt(), VK_LCONTROL);
        QVERIFY(holds[0].toMap()["durationMs"].toInt() >= 60);
        QVERIFY(holds[0].toMap()["overlapMs"].toInt() >= 40);
        QCOMPARE(holds[1].toMap()["keyCode"].toInt(), int('C'));
        QVERIFY(holds[2].toMap()["summary"].toString().startsWith("Hold E for "));
        injectedKeys.clear(); injectedKeyCodes.clear();
        macro.setLoops(1); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 2000);
        QCOMPARE(injectedKeyCodes, QList<int>({VK_LCONTROL, 'C', 'C', VK_LCONTROL, 'E', 'E'}));
        QCOMPARE(injectedKeys, QList<bool>({true, true, false, false, true, false}));
        macro.undo(); macro.undo(); QVERIFY(macro.steps().isEmpty());
    }
    void arbitraryClickCounts() {
        InputBlocker blocker;
        if (!blocker.mouse) QSKIP("Input interception unavailable");
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        auto click = macro.defaultStep("click"); click["fixed"] = false; click["count"] = 5; click["intervalMs"] = 2;
        QVERIFY(macro.putStep(-1, click));
        QVERIFY(macro.steps()[0].toMap()["summary"].toString().contains("5 clicks"));
        injectedButtons.clear(); macro.setLoops(1); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 1000);
        QCOMPARE(injectedButtons.size(), 10);
        for (int i = 0; i < 10; ++i) QCOMPARE(injectedButtons[i], i % 2 == 0);
        click["intervalMs"] = 0; QVERIFY(macro.putStep(0, click));
        injectedButtons.clear(); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!macro.playing(), 1000);
        QCOMPARE(injectedButtons, QList<bool>({true, false, true, false, true, false, true, false, true, false}));
        click["count"] = 2147483647;
        QVERIFY(macro.putStep(0, click));
        QElapsedTimer elapsed; elapsed.start(); macro.play();
        QVERIFY(macro.playing()); QVERIFY(elapsed.elapsed() < 200); macro.stop();
        QVERIFY(!macro.putStep(-1, macro.defaultStep("move")));
    }
    void legacyRecordingsBecomeHoldsAndWaits() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        auto down = macro.defaultStep("keyDown"); down["delayMs"] = 10;
        auto move = macro.defaultStep("move"); move["delayMs"] = 20; move["durationMs"] = 0;
        auto up = macro.defaultStep("keyUp"); up["delayMs"] = 30;
        const QJsonObject document{{"format", "FlintMacro"}, {"version", 1}, {"name", "Legacy"},
            {"loops", 1}, {"speed", 1}, {"steps", QJsonArray::fromVariantList({down, move, up})}};
        QTemporaryDir files(QDir::currentPath() + "/legacy-files-XXXXXX");
        QFile file(files.filePath("legacy.flintmacro")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(document).toJson()); file.close();
        QVERIFY2(macro.open(QUrl::fromLocalFile(file.fileName())), qPrintable(macro.status()));
        QCOMPARE(macro.steps().size(), 2);
        QCOMPARE(macro.stepAt(0)["type"].toString(), "wait");
        QCOMPARE(macro.stepAt(0)["durationMs"].toInt(), 10);
        QCOMPARE(macro.stepAt(1)["type"].toString(), "hold");
        QCOMPARE(macro.stepAt(1)["durationMs"].toInt(), 50);
    }
    void stoppingReleasesHeldInputs() {
        InputBlocker blocker;
        if (!blocker.keyboard || !blocker.mouse) QSKIP("Input interception unavailable");
        // The blocker outlives the engine so cleanup is also intercepted.
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        injectedKeys.clear(); injectedButtons.clear();
        auto hold = macro.defaultStep("hold"); hold["key"] = "F24"; hold["durationMs"] = 60000;
        QVERIFY(macro.putStep(-1, hold)); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!injectedKeys.isEmpty(), 1000);
        QVERIFY(injectedKeys.first()); macro.stop();
        QTRY_COMPARE_WITH_TIMEOUT(injectedKeys.size(), 2, 1000);
        QVERIFY(!injectedKeys.last());
        macro.newMacro(); hold["button"] = 1; hold["fixed"] = false;
        QVERIFY(macro.putStep(-1, hold)); macro.play();
        QTRY_VERIFY_WITH_TIMEOUT(!injectedButtons.isEmpty(), 1000);
        QVERIFY(injectedButtons.first()); macro.stop();
        QTRY_COMPARE_WITH_TIMEOUT(injectedButtons.size(), 2, 1000);
        QVERIFY(!injectedButtons.last());
    }
    void qmlLoadsAndRenders() {
        Backend clicker; MacroBackend macro(&clicker); macro.newMacro();
        clicker.setAction(0); // Check the smaller window, independent of earlier tests' settings.
        for (const QString& type : {"click", "hold", "press", "scroll", "wait"})
            QVERIFY(macro.putStep(-1, macro.defaultStep(type)));
        QQmlApplicationEngine engine;
        QList<QQmlError> errors;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError>& warnings) { errors.append(warnings); });
        engine.rootContext()->setContextProperty("backend", &clicker);
        engine.rootContext()->setContextProperty("macro", &macro);
        engine.load(QUrl::fromLocalFile(QStringLiteral(FLINT_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first()); QVERIFY(window);
        QCOMPARE(window->height(), 720);
        window->setProperty("activeTab", 1); window->show(); window->requestActivate(); QTest::qWait(200);
        auto* panel = window->findChild<QObject*>("macroPanel"); QVERIFY(panel);
        auto* gap = window->findChild<QQuickItem*>("loopGapField"); QVERIFY(gap);
        auto* gapInput = gap->property("contentItem").value<QQuickItem*>(); QVERIFY(gapInput);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, gap->mapToScene(QPointF(35, 13)).toPoint());
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_2); QTest::keyClick(window, Qt::Key_0); QTest::keyClick(window, Qt::Key_0);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(macro.loopGapMs(), 200);
        QVERIFY(!gapInput->hasActiveFocus()); QVERIFY(!gap->hasActiveFocus());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, gap->mapToScene(QPointF(35, 13)).toPoint());
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_3); QTest::keyClick(window, Qt::Key_0); QTest::keyClick(window, Qt::Key_0);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, gap->mapToScene(QPointF(-50, 13)).toPoint());
        QTRY_COMPARE(macro.loopGapMs(), 300);
        QVERIFY(!gapInput->hasActiveFocus()); QVERIFY(!gap->hasActiveFocus());
        macro.setLoopGapMs(100);

        auto* first = visualItem(window->contentItem(), "step_0"); QVERIFY(first);
        auto* third = visualItem(window->contentItem(), "step_2"); QVERIFY(third);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, first->mapToScene(QPointF(70, 20)).toPoint());
        QTest::mouseClick(window, Qt::LeftButton, Qt::ShiftModifier, third->mapToScene(QPointF(70, 20)).toPoint());
        QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_P, Qt::ControlModifier);
        QTRY_COMPARE(macro.steps().size(), 8);
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QTRY_COMPARE(macro.steps().size(), 5);
        QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
        QTRY_COMPARE(macro.steps().size(), 8);
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QTRY_COMPARE(macro.steps().size(), 5);
        QVERIFY(window->grabWindow().save("macro-dark.png"));
        auto* speed = window->findChild<QQuickItem*>("speedField"); QVERIFY(speed);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, speed->mapToScene(QPointF(20, 12)).toPoint());
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (auto key : {Qt::Key_1, Qt::Key_Period, Qt::Key_7, Qt::Key_5}) QTest::keyClick(window, key);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(macro.speed(), 1.75); QVERIFY(macro.customSpeed());
        QTest::qWait(100); QVERIFY(window->grabWindow().save("macro-custom-speed.png"));
        for (const QString& value : {QString("20"), QString("-5")}) {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, speed->mapToScene(QPointF(20, 12)).toPoint());
            QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
            for (QChar ch : value) QTest::keyClick(window, Qt::Key(ch.unicode()));
            QTest::keyClick(window, Qt::Key_Return);
            QTRY_COMPARE(macro.speed(), value == "20" ? 10.0 : 0.0);
            QCOMPARE(speed->property("text").toString(), value == "20" ? "10" : "0");
        }
        auto* presets = window->findChild<QQuickItem*>("speedPresets"); QVERIFY(presets);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, presets->mapToScene(QPointF(75, 14)).toPoint());
        QCOMPARE(macro.speed(), 1.0); QVERIFY(!macro.customSpeed());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, speed->mapToScene(QPointF(20, 12)).toPoint());
        QTest::keyClick(window, Qt::Key_Return); QVERIFY(!macro.customSpeed());
        auto* contextRow = visualItem(window->contentItem(), "step_0"); QVERIFY(contextRow);
        QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, contextRow->mapToScene(QPointF(70, 20)).toPoint());
        auto* menu = window->findChild<QObject*>("editMenu"); QVERIFY(menu);
        QTRY_VERIFY(menu->property("visible").toBool());
        QTest::qWait(100); QVERIFY(window->grabWindow().save("macro-context-menu.png"));
        QTest::keyClick(window, Qt::Key_Escape);
        clicker.setDark(false); QTest::qWait(250);
        QVERIFY(window->grabWindow().save("macro-light.png"));
        QVERIFY(QMetaObject::invokeMethod(panel, "edit", Q_ARG(QVariant, 1), Q_ARG(QVariant, "")));
        QTest::qWait(200); QVERIFY(window->grabWindow().save("macro-editor.png"));
        auto* combo = window->findChild<QQuickItem*>("actionType"); QVERIFY(combo);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, combo->mapToScene(QPointF(30, 15)).toPoint());
        QTest::qWait(150); QVERIFY(window->grabWindow().save("macro-dropdown.png"));
        QTest::keyClick(window, Qt::Key_Escape);
        auto* editor = window->findChild<QObject*>("stepEditor"); QVERIFY(editor);
        QMetaObject::invokeMethod(editor, "close");
        QVERIFY(QMetaObject::invokeMethod(panel, "edit", Q_ARG(QVariant, 4), Q_ARG(QVariant, "")));
        QTest::qWait(150); QVERIFY(window->grabWindow().save("macro-wait.png"));
        QMetaObject::invokeMethod(editor, "close");
        window->setProperty("activeTab", 0); clicker.setDark(true); QTest::qWait(200);
        QVERIFY(window->grabWindow().save("autoclicker-accents.png"));
        window->close(); QVERIFY(!window->isVisible());
        for (const auto& error : errors) qWarning().noquote() << error.toString();
        QVERIFY(errors.isEmpty());
    }
};

int main(int argc, char** argv) {
    QTemporaryDir isolatedSettings(QDir::currentPath() + "/macro-settings-XXXXXX");
    qputenv("LOCALAPPDATA", isolatedSettings.path().toUtf8());
    QQuickWindow::setDefaultAlphaBuffer(true);
    QGuiApplication app(argc, argv);
    MacroTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "MacroTests.moc"
