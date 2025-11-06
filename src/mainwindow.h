#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAudioSink>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QIODevice>
#include <QByteArray>
#include <QFile>
#include <QTimer>

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
    QByteArray loadWavPcmData(const QString &filePath, QAudioFormat &format);
    int getAvailableAudioSink();
    
    QWidget *centralWidget;
    QWidget *pianoKeysContainer;  // Container for overlapping white and black keys
    QVBoxLayout *mainLayout;
    QHBoxLayout *whiteKeysLayout;
    
    QMap<QString, QPushButton*> pianoKeys;
    QMap<QString, QByteArray> audioBuffers;  // Pre-loaded PCM audio data
    QList<QAudioSink*> audioSinkPool;  // Pool of audio sinks for overlapping playback
    QList<QIODevice*> audioDevices;  // Corresponding IO devices for each sink
    QList<QByteArray> audioStreamBuffers;  // Remaining data to stream for each sink
    QList<QTimer*> streamTimers;  // Timers to continue streaming data
    QAudioFormat audioFormat;
    QAudioDevice audioDevice;
    
    void streamAudioData(int sinkIndex);
};

#endif // MAINWINDOW_H