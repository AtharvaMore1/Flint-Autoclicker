#pragma once

#include <QAbstractNativeEventFilter>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVector>
#include <map>
#include <queue>
#include <set>

class Backend;
class QQuickWindow;

class MacroBackend final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(QVariantList steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY documentChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY documentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY documentChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY documentChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(int loops READ loops WRITE setLoops NOTIFY documentChanged)
    Q_PROPERTY(int loopGapMs READ loopGapMs WRITE setLoopGapMs NOTIFY documentChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY documentChanged)
    Q_PROPERTY(bool customSpeed READ customSpeed NOTIFY documentChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int currentStep READ currentStep NOTIFY progressChanged)
    Q_PROPERTY(qulonglong currentLoop READ currentLoop NOTIFY progressChanged)
    Q_PROPERTY(int recordedCount READ recordedCount NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool editorOpen READ editorOpen WRITE setEditorOpen NOTIFY editorOpenChanged)
public:
    explicit MacroBackend(Backend* clicker, QObject* parent = nullptr);
    ~MacroBackend() override;
    void attachWindow(QQuickWindow* window);
    QVariantList steps() const;
    QString name() const { return name_; }
    QString fileName() const { return path_; }
    bool dirty() const { return dirty_; }
    bool canUndo() const { return !undo_.isEmpty() && !busy(); }
    bool canRedo() const { return !redo_.isEmpty() && !busy(); }
    bool canPaste() const;
    int loops() const { return loops_; }
    int loopGapMs() const { return loopGapMs_; }
    double speed() const { return speed_; }
    bool customSpeed() const { return customSpeed_; }
    bool recording() const { return recording_; }
    bool playing() const { return playing_; }
    bool busy() const { return recording_ || playing_; }
    int currentStep() const { return currentStep_; }
    qulonglong currentLoop() const { return currentLoop_; }
    int recordedCount() const { return recorded_.size(); }
    QString status() const { return status_; }
    bool editorOpen() const { return editorOpen_; }
    void setEditorOpen(bool value) { if (editorOpen_ == value) return; editorOpen_ = value; emit editorOpenChanged(); }
    void setName(const QString& value);
    void setLoops(int value);
    void setLoopGapMs(int value);
    void setSpeed(double value);
    Q_INVOKABLE void setPresetSpeed(double value);
    Q_INVOKABLE QVariantMap defaultStep(const QString& type) const;
    Q_INVOKABLE QVariantMap stepAt(int index) const;
    Q_INVOKABLE bool putStep(int index, const QVariantMap& step);
    Q_INVOKABLE void copySteps(const QVariantList& indices);
    Q_INVOKABLE void cutSteps(const QVariantList& indices);
    Q_INVOKABLE QVariantList pasteSteps(int afterIndex);
    Q_INVOKABLE void removeSteps(const QVariantList& indices);
    Q_INVOKABLE QVariantList moveSteps(const QVariantList& indices, int delta);
    Q_INVOKABLE QVariantList duplicateSteps(const QVariantList& indices);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void newMacro();
    Q_INVOKABLE bool open(const QUrl& file);
    Q_INVOKABLE bool save(const QUrl& file = QUrl());
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void play();
    Q_INVOKABLE void stop(bool fromButton = false);
    Q_INVOKABLE void pickLocation();
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;

    // Called only by the Windows low-level hooks, on the GUI thread.
    void recordKeyboard(unsigned int key, bool down, bool extended);
    void recordMouse(int button, bool down, int x, int y, int wheel, bool horizontal);
signals:
    void stepsChanged();
    void documentChanged();
    void clipboardChanged();
    void editorOpenChanged();
    void stateChanged();
    void progressChanged();
    void statusChanged();
    void recordRequested();
    void locationPicked(int x, int y);
private:
    struct Snapshot { QVariantList steps; QString name, path; int loops, loopGapMs; double speed; bool customSpeed; };
    struct Event {
        enum Kind { Wait, Key, Mouse, Wheel } kind = Wait;
        double at = 0, interval = 0;
        int remaining = 1, repetition = 0, order = 0;
        int step = 0, key = 0, button = 0, x = 0, y = 0, amount = 0;
        bool down = false, extended = false, positioned = false, horizontal = false;
    };
    struct Later {
        bool operator()(const Event& a, const Event& b) const {
            if (a.at != b.at) return a.at > b.at;
            if (a.step != b.step) return a.step > b.step;
            if (a.repetition != b.repetition) return a.repetition > b.repetition;
            return a.order > b.order;
        }
    };
    Snapshot snapshot() const;
    void restore(const Snapshot& value);
    void checkpoint();
    void changed(bool sequenceChanged = true);
    void reportProgress(bool immediate = false);
    void resetPlaybackQueue();
    void setStatus(const QString& text);
    bool validateStep(const QVariantMap& step, QString& error) const;
    bool compile();
    void tick();
    bool dispatch(const Event& event);
    void releaseInputs();
    void appendRecorded(QVariantMap step);
    QVariantList normalizeSteps(const QVariantList& input) const;
    void saveSession();
    bool readDocument(const QByteArray& bytes, bool session);
    QByteArray documentBytes(bool session) const;
    QString summary(const QVariantMap& step) const;
    QList<int> selectedRows(const QVariantList& indices) const;

    Backend* clicker_;
    void* window_ = nullptr;
    void* keyboardHook_ = nullptr;
    void* mouseHook_ = nullptr;
    QVariantList steps_, recorded_;
    QVector<Snapshot> undo_, redo_;
    QString name_ = QStringLiteral("Untitled macro"), path_, status_ = QStringLiteral("Ready");
    bool dirty_ = false, recording_ = false, playing_ = false;
    bool editorOpen_ = false, customSpeed_ = false;
    int loops_ = 0, loopGapMs_ = 100, currentStep_ = -1;
    qulonglong currentLoop_ = 0;
    double speed_ = 1.0, loopStart_ = 0;
    qint64 lastRecordTime_ = 0;
    QElapsedTimer recordClock_, playbackClock_;
    QTimer playbackTimer_, sessionTimer_, progressTimer_;
    QVector<Event> events_;
    std::priority_queue<Event, std::vector<Event>, Later> pending_;
    std::map<int, bool> heldKeys_;
    std::map<int, bool> recordedKeys_;
    std::set<int> heldButtons_, recordedButtons_;
};
