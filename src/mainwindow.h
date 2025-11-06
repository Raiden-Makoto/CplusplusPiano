#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSoundEffect>
#include <QUrl>

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
    
    QWidget *centralWidget;
    QWidget *pianoKeysContainer;  // Container for overlapping white and black keys
    QVBoxLayout *mainLayout;
    QHBoxLayout *whiteKeysLayout;
    
    QMap<QString, QPushButton*> pianoKeys;
    QMap<QString, QList<QSoundEffect*>> soundEffectPools;  // Pool of effects per note for overlapping
};

#endif // MAINWINDOW_H