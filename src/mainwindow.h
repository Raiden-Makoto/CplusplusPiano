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
#include <portaudio.h>

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
    static int audioCallback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo *timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData);
    
    QWidget *centralWidget;
    QWidget *pianoKeysContainer;  // Container for overlapping white and black keys
    QVBoxLayout *mainLayout;
    QHBoxLayout *whiteKeysLayout;
    
    QMap<QString, QPushButton*> pianoKeys;
    QMap<QString, QByteArray> audioBuffers;  // Pre-loaded PCM audio data
    QMap<QString, int> audioSampleRates;  // Sample rate for each note
    QMap<QString, int> audioChannels;  // Channel count for each note
    
    // PortAudio
    PaStream *audioStream;
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