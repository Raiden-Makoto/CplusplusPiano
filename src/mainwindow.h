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
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void playNote(const QString &note);

private:
    void setupUI();
    void setupAudio();
    void connectKeySignals();
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
    
    QMap<QString, QPushButton*> pianoKeys;
    QMap<QString, QByteArray> audioBuffers;  // Pre-loaded PCM audio data
    QMap<QString, int> audioSampleRates;  // Sample rate for each note
    QMap<QString, int> audioChannels;  // Channel count for each note
    
    // Core Audio
    AudioComponentInstance audioUnit;
    int outputSampleRate;
    int outputChannels;
    
    // Active notes (for mixing)
    struct ActiveNote {
        QString note;
        const qint16 *data;
        int position;
        int length;
        int sampleRate;
        int channels;
    };
    QVector<ActiveNote> activeNotes;
    QMutex activeNotesMutex;
};

#endif // MAINWINDOW_H