#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

class QKeyEvent;
class QMouseEvent;
class QQuickWindow;

class Backend final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(int action READ action WRITE setAction NOTIFY settingsChanged)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY settingsChanged)
    Q_PROPERTY(bool randomOffset READ randomOffset WRITE setRandomOffset NOTIFY settingsChanged)
    Q_PROPERTY(int offsetMs READ offsetMs WRITE setOffsetMs NOTIFY settingsChanged)
    Q_PROPERTY(bool repeatForever READ repeatForever WRITE setRepeatForever NOTIFY settingsChanged)
    Q_PROPERTY(int repeatCount READ repeatCount WRITE setRepeatCount NOTIFY settingsChanged)
    Q_PROPERTY(bool fixedLocation READ fixedLocation WRITE setFixedLocation NOTIFY settingsChanged)
    Q_PROPERTY(int locationX READ locationX WRITE setLocationX NOTIFY settingsChanged)
    Q_PROPERTY(int locationY READ locationY WRITE setLocationY NOTIFY settingsChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY settingsChanged)
    Q_PROPERTY(QString actionShortcut READ actionShortcut NOTIFY settingsChanged)
    Q_PROPERTY(bool customIsMouse READ customIsMouse NOTIFY settingsChanged)
    Q_PROPERTY(QString hotkeyShortcut READ hotkeyShortcut NOTIFY settingsChanged)
    Q_PROPERTY(bool holdToClick READ holdToClick WRITE setHoldToClick NOTIFY settingsChanged)
    Q_PROPERTY(QString captureTarget READ captureTarget NOTIFY captureChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(qulonglong completed READ completed NOTIFY completedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int virtualScreenX READ virtualScreenX CONSTANT)
    Q_PROPERTY(int virtualScreenY READ virtualScreenY CONSTANT)
    Q_PROPERTY(int virtualScreenWidth READ virtualScreenWidth CONSTANT)
    Q_PROPERTY(int virtualScreenHeight READ virtualScreenHeight CONSTANT)

public:
    explicit Backend(QObject* parent = nullptr);
    ~Backend() override;

    int action() const;
    int intervalMs() const;
    bool randomOffset() const;
    int offsetMs() const;
    bool repeatForever() const;
    int repeatCount() const;
    bool fixedLocation() const;
    int locationX() const;
    int locationY() const;
    bool dark() const;
    QString actionShortcut() const;
    bool customIsMouse() const;
    QString hotkeyShortcut() const;
    bool holdToClick() const;
    QString captureTarget() const;
    bool running() const;
    qulonglong completed() const;
    QString status() const;
    int virtualScreenX() const;
    int virtualScreenY() const;
    int virtualScreenWidth() const;
    int virtualScreenHeight() const;

    void setAction(int value);
    void setIntervalMs(int value);
    void setRandomOffset(bool value);
    void setOffsetMs(int value);
    void setRepeatForever(bool value);
    void setRepeatCount(int value);
    void setFixedLocation(bool value);
    void setLocationX(int value);
    void setLocationY(int value);
    void setDark(bool value);
    void setHoldToClick(bool value);

    void attachWindow(QQuickWindow* window);
    void setMacroActive(bool active) { macroActive_ = active; }
    Q_INVOKABLE void beginActionCapture();
    Q_INVOKABLE void beginHotkeyCapture();
    Q_INVOKABLE void cancelCapture();
    Q_INVOKABLE void pickLocation();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void toggleFromButton();
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void save();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void settingsChanged();
    void captureChanged();
    void runningChanged();
    void completedChanged();
    void statusChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Settings {
        int action = 0; // 0 left, 1 middle, 2 right, 3 keyboard
        int intervalMs = 100;
        bool randomOffset = false;
        int offsetMs = 0;
        bool repeatForever = true;
        int repeatCount = 100;
        bool fixedLocation = false;
        int x = 0;
        int y = 0;
        bool dark = true;
        unsigned int actionKey = 'E';
        unsigned int actionModifiers = 0;
        int customMouseButton = 0; // 0 keyboard, 1 left, 2 middle, 3 right, 4/5 side buttons
        unsigned int hotkey = 0x75; // F6
        unsigned int hotkeyModifiers = 0;
        bool holdToClick = false;
    };

    void load();
    void beginCapture(const QString& target);
    void finishCapture(QKeyEvent* event);
    void finishMouseCapture(QMouseEvent* event);
    bool registerHotkey(unsigned int key, unsigned int modifiers);
    static bool sendAction(const Settings& settings);
    void setStatus(const QString& text);
    void stopEngine(bool showStoppedStatus);
    bool hotkeyStillPressed() const;

    Settings settings_;
    QPointer<QQuickWindow> window_;
    void* windowHandle_ = nullptr;
    bool hotkeyRegistered_ = false;
    bool macroActive_ = false;
    bool hotkeyHeld_ = false;
    QTimer holdTimer_;
    QString captureTarget_;
    QString status_ = QStringLiteral("Ready");
    qulonglong completed_ = 0;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> workerCompleted_{0};
    std::thread worker_;
    std::mutex waitMutex_;
    std::condition_variable waitCondition_;
    std::uint64_t runId_ = 0;
};
