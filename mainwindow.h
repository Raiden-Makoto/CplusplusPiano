#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUI();
    
    QWidget *centralWidget;
    QWidget *pianoKeysContainer;  // Container for overlapping white and black keys
    QVBoxLayout *mainLayout;
    QHBoxLayout *whiteKeysLayout;
    
    QMap<QString, QPushButton*> pianoKeys;
};

#endif // MAINWINDOW_H