#include "mainwindow.h"
#include <QLabel>
#include <QWidget>
#include <Qt>
#include <QStringList>
#include <QList>
#include <QVBoxLayout>
#include <QSpacerItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    setWindowTitle("Virtual Piano");
    resize(1200, 400);
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
    
    // Define key dimensions
    const int whiteKeyWidth = 80;
    const int whiteKeyHeight = 200;
    const int totalWhiteKeys = 15;  // Two octaves plus final C
    
    // Create a container widget for piano keys (allows absolute positioning of black keys)
    pianoKeysContainer = new QWidget(centralWidget);
    const int containerWidth = totalWhiteKeys * whiteKeyWidth;
    pianoKeysContainer->setFixedSize(containerWidth, whiteKeyHeight);
    whiteKeysLayout = new QHBoxLayout(pianoKeysContainer);
    whiteKeysLayout->setContentsMargins(0, 0, 0, 0);
    whiteKeysLayout->setSpacing(0);
    
    // Create white keys for two octaves plus final C
    QStringList whiteNotes = {"C", "D", "E", "F", "G", "A", "B", "C", "D", "E", "F", "G", "A", "B", "C"};
    
    for (int i = 0; i < whiteNotes.size(); i++) {
        const QString &note = whiteNotes[i];
        // Create unique key identifier (note + index to handle duplicates)
        QString keyId = QString("%1_%2").arg(note).arg(i);
        // Create a container widget for the key with label at bottom
        QWidget *keyContainer = new QWidget(pianoKeysContainer);
        keyContainer->setFixedSize(whiteKeyWidth, whiteKeyHeight);
        
        // Create the button first (background layer)
        QPushButton *key = new QPushButton(keyContainer);
        key->setFixedSize(whiteKeyWidth, whiteKeyHeight);
        key->setStyleSheet(
            "QPushButton {"
            "  background-color: white;"
            "  border: 2px solid black;"
            "  border-radius: 5px;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #e0e0e0;"
            "}"
        );
        
        // Create label at bottom, positioned on top of button
        QLabel *noteLabel = new QLabel(note, keyContainer);
        noteLabel->setAlignment(Qt::AlignCenter);
        noteLabel->setStyleSheet(
            "QLabel {"
            "  background-color: transparent;"
            "  color: black;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "}"
        );
        // Position label at bottom of container
        noteLabel->setGeometry(0, whiteKeyHeight - 30, whiteKeyWidth, 30);
        noteLabel->raise();  // Put label on top so it's visible
        
        whiteKeysLayout->addWidget(keyContainer);
        pianoKeys[keyId] = key;
    }
    
    mainLayout->addWidget(pianoKeysContainer);
    
    // Create black keys with absolute positioning to overlap white keys (two octaves)
    QStringList blackNotes = {"C#", "D#", "F#", "G#", "A#", "C#", "D#", "F#", "G#", "A#"};
    const int blackKeyWidth = 50;
    const int blackKeyHeight = 130;
    const int blackKeyY = 0;  // Position at top of container
    
    // Black keys are positioned between specific white key pairs for two octaves:
    // First octave: C# between C(0) and D(1), D# between D(1) and E(2), 
    //               F# between F(3) and G(4), G# between G(4) and A(5), A# between A(5) and B(6)
    // Second octave: C# between C(7) and D(8), D# between D(8) and E(9),
    //                F# between F(10) and G(11), G# between G(11) and A(12), A# between A(12) and B(13)
    // Each entry is the pair of white key indices: {leftWhiteKeyIndex, rightWhiteKeyIndex}
    QList<QPair<int, int>> blackKeyPositions = {
        {0, 1},   // C# between C and D (octave 1)
        {1, 2},   // D# between D and E (octave 1)
        {3, 4},   // F# between F and G (octave 1)
        {4, 5},   // G# between G and A (octave 1)
        {5, 6},   // A# between A and B (octave 1)
        {7, 8},   // C# between C and D (octave 2)
        {8, 9},   // D# between D and E (octave 2)
        {10, 11}, // F# between F and G (octave 2)
        {11, 12}, // G# between G and A (octave 2)
        {12, 13}  // A# between A and B (octave 2)
    };
    
    for (int i = 0; i < blackNotes.size(); i++) {
        int leftWhiteIndex = blackKeyPositions[i].first;
        int rightWhiteIndex = blackKeyPositions[i].second;
        
        // Calculate the center point between the CENTERS of the two white keys
        // White key center = left edge + half width
        int leftKeyCenter = leftWhiteIndex * whiteKeyWidth + whiteKeyWidth / 2;
        int rightKeyCenter = rightWhiteIndex * whiteKeyWidth + whiteKeyWidth / 2;
        int centerBetweenKeys = (leftKeyCenter + rightKeyCenter) / 2;
        
        // Position black key centered at that point
        int blackKeyX = centerBetweenKeys - blackKeyWidth / 2;
        
        // Create unique key identifier for black keys
        QString keyId = QString("%1_%2").arg(blackNotes[i]).arg(i);
        
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
        pianoKeys[keyId] = key;
    }
    
    // Set the central widget
    setCentralWidget(centralWidget);
}

MainWindow::~MainWindow(){}