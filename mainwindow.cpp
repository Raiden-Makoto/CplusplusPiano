#include "mainwindow.h"
#include <QLabel>
#include <QWidget>
#include <Qt>
#include <QStringList>
#include <QList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    setWindowTitle("Virtual Piano");
    resize(900, 400);
}

void MainWindow::setupUI()
{
    // Create central widget and main layout
    centralWidget = new QWidget(this);
    mainLayout = new QVBoxLayout(centralWidget);
    
    // Add title label
    QLabel *title = new QLabel("Virtual Piano - Press Keys or Click Buttons", centralWidget);
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);
    
    // Create a container widget for piano keys (allows absolute positioning of black keys)
    pianoKeysContainer = new QWidget(centralWidget);
    pianoKeysContainer->setFixedHeight(200);
    whiteKeysLayout = new QHBoxLayout(pianoKeysContainer);
    whiteKeysLayout->setContentsMargins(0, 0, 0, 0);
    whiteKeysLayout->setSpacing(0);
    
    // Create white keys
    QStringList whiteNotes = {"C", "D", "E", "F", "G", "A", "B"};
    const int whiteKeyWidth = 80;
    const int whiteKeyHeight = 200;
    
    for (const QString &note : whiteNotes) {
        QPushButton *key = new QPushButton(note, pianoKeysContainer);
        key->setFixedSize(whiteKeyWidth, whiteKeyHeight);
        key->setStyleSheet(
            "QPushButton {"
            "  background-color: white;"
            "  border: 2px solid black;"
            "  border-radius: 5px;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #e0e0e0;"
            "}"
        );
        whiteKeysLayout->addWidget(key);
        pianoKeys[note] = key;
    }
    
    mainLayout->addWidget(pianoKeysContainer);
    
    // Create black keys with absolute positioning to overlap white keys
    QStringList blackNotes = {"C#", "D#", "F#", "G#", "A#"};
    const int blackKeyWidth = 50;
    const int blackKeyHeight = 130;
    const int blackKeyY = 0;  // Position at top of container
    
    // Black keys are positioned between white keys:
    // C# after C (pos 0), D# after D (pos 1), F# after F (pos 3), G# after G (pos 4), A# after A (pos 5)
    QList<int> blackKeyPositions = {0, 1, 3, 4, 5};  // White key indices before each black key
    
    for (int i = 0; i < blackNotes.size(); i++) {
        int whiteKeyIndex = blackKeyPositions[i];
        // Calculate x position: center the black key between two white keys
        // Midpoint between whiteKeyIndex and whiteKeyIndex+1, then offset by half black key width
        int blackKeyX = whiteKeyIndex * whiteKeyWidth + (whiteKeyWidth - blackKeyWidth) / 2;
        
        QPushButton *key = new QPushButton(blackNotes[i], pianoKeysContainer);
        key->setGeometry(blackKeyX, blackKeyY, blackKeyWidth, blackKeyHeight);
        key->raise();  // Ensure black keys are on top
        key->setStyleSheet(
            "QPushButton {"
            "  background-color: black;"
            "  color: white;"
            "  border: 1px solid gray;"
            "  border-radius: 3px;"
            "  font-size: 12px;"
            "  font-weight: bold;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #333333;"
            "}"
        );
        pianoKeys[blackNotes[i]] = key;
    }
    
    // Set the central widget
    setCentralWidget(centralWidget);
}

MainWindow::~MainWindow(){}