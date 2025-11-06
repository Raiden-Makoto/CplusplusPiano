#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QByteArray>
#include <QFile>
#include <QMutex>
#include <QVector>
#include <QKeyEvent>
#include <QCheckBox>
#include <QHBoxLayout>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void playNote(const QString &note);

private:
    void setupUI();
    void setupAudio();
    void connectKeySignals();
    void highlightKey(const QString &note);
    QString getAudioFilePath(const QString &note);
    QByteArray loadWavPcmData(const QString &filePath, int &sampleRate, int &channels);
    static OSStatus audioRenderCallback(void *inRefCon,
                                       AudioUnitRenderActionFlags *ioActionFlags,
                                       const AudioTimeStamp *inTimeStamp,
                                       UInt32 inBusNumber,
                                       UInt32 inNumberFrames,
                                       AudioBufferList *ioData);
    
    QWidget *centralWidget;
    QWidget *pianoKeysContainer;  // Container for overlapping white and black keys
    QVBoxLayout *mainLayout;
    QHBoxLayout *whiteKeysLayout;
    
    // Pedal indicators
    QCheckBox *unaCordaIndicator;
    QCheckBox *damperPedalIndicator;
    
    QMap<QString, QPushButton*> pianoKeys;
    QMap<QString, QString> keyOriginalStyles;  // Store original styles for fade-back
    QMap<QString, QTimer*> keyFadeTimers;  // Timers for fade-back animation
    QMap<QString, int> keyFadeSteps;  // Track fade animation steps
    QMap<QString, QByteArray> audioBuffers;  // Pre-loaded PCM audio data
    QMap<QString, int> audioSampleRates;  // Sample rate for each note
    QMap<QString, int> audioChannels;  // Channel count for each note
    
    // Core Audio
    AudioComponentInstance audioUnit;
    int outputSampleRate;
    int outputChannels;
    
    // Active notes (for mixing)
    struct ActiveNote {
        const qint16 *data;
        int position;
        int length;
        int sampleRate;
        int channels;
        bool isSustained;  // True if note is being sustained by damper pedal
        double sustainVolume;  // Current volume multiplier for sustained notes (for fade-out)
        int framesPlayed;  // Number of frames played so far (for 1-second cutoff)
    };
    QVector<ActiveNote> activeNotes;
    QMutex activeNotesMutex;
    
    // Pending notes queue (for rapid key presses)
    // New notes are added here first, then moved to activeNotes in the audio callback
    QVector<ActiveNote> pendingNotes;
    QMutex pendingNotesMutex;
    
    // Pre-allocated buffer for mixing to avoid allocations in callback
    QVector<qint32> mixBuffer;
    
    // Una corda (soft pedal) state
    bool unaCordaActive;
    QMutex unaCordaMutex;
    
    // Damper pedal (sustain) state
    bool damperPedalActive;
    QMutex damperPedalMutex;
    
    // Low-pass filter state for muffled tone (per channel)
    QVector<double> lowPassFilterState;
};

#endif // MAINWINDOW_H