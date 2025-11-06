#include "mainwindow.h"
#include <QLabel>
#include <QWidget>
#include <Qt>
#include <QStringList>
#include <QList>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QFile>
#include <QDataStream>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSet>
#include <QMutex>
#include <QVector>
#include <QKeyEvent>
#include <QTimer>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <cmath>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), audioUnit(nullptr), outputSampleRate(44100), outputChannels(2),
      unaCordaActive(false), damperPedalActive(false)
{
    setupUI();  // Must be called first to create pianoKeys
    setupAudio();  // Preload audio files after keys are created
    connectKeySignals();
    setWindowTitle("Virtual Piano");
    
    // Enable keyboard focus so keyPressEvent works
    setFocusPolicy(Qt::StrongFocus);
    setFocus();  // Ensure window has focus to receive keyboard events
    
    // Initialize low-pass filter state (one per channel)
    lowPassFilterState.resize(outputChannels);
    for (int i = 0; i < outputChannels; ++i) {
        lowPassFilterState[i] = 0.0;
    }
}

MainWindow::~MainWindow()
{
    // Stop and cleanup Core Audio
    if (audioUnit) {
        AudioOutputUnitStop(audioUnit);
        AudioUnitUninitialize(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
    }
}

void MainWindow::setupUI()
{
    // Create central widget and main layout
    centralWidget = new QWidget(this);
    centralWidget->setContentsMargins(0, 0, 0, 0);  // Remove widget margins
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    // Margins will be set after we know the container width for symmetry
    
    // Add title label
    QLabel *title = new QLabel("Virtual Piano - Press Keys or Click Buttons", centralWidget);
    title->setAlignment(Qt::AlignCenter);
    title->setMargin(10);  // Add padding inside the label
    title->setStyleSheet(
        "QLabel {"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "}"
    );
    mainLayout->addWidget(title);
    
    // Define key dimensions
    const int whiteKeyWidth = 60;  // Narrower keys
    const int whiteKeyHeight = 250;  // Taller keys
    const int totalWhiteKeys = 22;  // Three octaves plus final C (3 × 7 + 1 = 22)
    
    // Create a container widget for piano keys (allows absolute positioning of black keys)
    pianoKeysContainer = new QWidget(centralWidget);
    const int containerWidth = totalWhiteKeys * whiteKeyWidth;
    pianoKeysContainer->setFixedSize(containerWidth, whiteKeyHeight);
    whiteKeysLayout = new QHBoxLayout(pianoKeysContainer);
    whiteKeysLayout->setContentsMargins(0, 0, 0, 0);
    whiteKeysLayout->setSpacing(0);
    
    // Create white keys for three octaves plus final C with keybind labels
    // Octaves: C3-C4 (first), C4-C5 (second), C5-C6 (third), final C6
    QStringList whiteNotes = {
        "C3", "D3", "E3", "F3", "G3", "A3", "B3",  // Octave 3
        "C4", "D4", "E4", "F4", "G4", "A4", "B4",  // Octave 4 (middle C is C4)
        "C5", "D5", "E5", "F5", "G5", "A5", "B5",  // Octave 5
        "C6"                                        // Final C6
    };
    
    // Map note to keybind for display
    QMap<QString, QString> noteToKeybind;
    // First octave (C3-B3): number row
    noteToKeybind["C3"] = "1"; noteToKeybind["C#3"] = "2"; noteToKeybind["D3"] = "3";
    noteToKeybind["D#3"] = "4"; noteToKeybind["E3"] = "5"; noteToKeybind["F3"] = "6";
    noteToKeybind["F#3"] = "7"; noteToKeybind["G3"] = "8"; noteToKeybind["G#3"] = "9";
    noteToKeybind["A3"] = "0"; noteToKeybind["A#3"] = "-"; noteToKeybind["B3"] = "=";
    // Second octave (C4-B4): qwert row
    noteToKeybind["C4"] = "q"; noteToKeybind["C#4"] = "w"; noteToKeybind["D4"] = "e";
    noteToKeybind["D#4"] = "r"; noteToKeybind["E4"] = "t"; noteToKeybind["F4"] = "y";
    noteToKeybind["F#4"] = "u"; noteToKeybind["G4"] = "i"; noteToKeybind["G#4"] = "o";
    noteToKeybind["A4"] = "p"; noteToKeybind["A#4"] = "["; noteToKeybind["B4"] = "]";
    // Third octave (C5-B5): asdfg row
    noteToKeybind["C5"] = "a"; noteToKeybind["C#5"] = "s"; noteToKeybind["D5"] = "d";
    noteToKeybind["D#5"] = "f"; noteToKeybind["E5"] = "g"; noteToKeybind["F5"] = "h";
    noteToKeybind["F#5"] = "j"; noteToKeybind["G5"] = "k"; noteToKeybind["G#5"] = "l";
    noteToKeybind["A5"] = ";"; noteToKeybind["A#5"] = "'"; noteToKeybind["B5"] = "\\";
    // C6 uses z key
    noteToKeybind["C6"] = "z";
    
    for (int i = 0; i < whiteNotes.size(); i++) {
        const QString &note = whiteNotes[i];
        // Create unique key identifier (note + index to handle duplicates)
        QString keyId = QString("%1_%2").arg(note).arg(i);
        // Check if this is middle C (C4) - index 7
        bool isMiddleC = (i == 7);
        
        // Get keybind for this note (if no keybind, show empty string)
        QString keybind = noteToKeybind.value(note, "");
        
        // Create a container widget for the key with label at bottom
        QWidget *keyContainer = new QWidget(pianoKeysContainer);
        keyContainer->setFixedSize(whiteKeyWidth, whiteKeyHeight);
        
        // Create the button first (background layer)
        QPushButton *key = new QPushButton(keyContainer);
        key->setFixedSize(whiteKeyWidth, whiteKeyHeight);
        QString whiteKeyStyle = QString(
            "QPushButton {"
            "  background-color: white;"
            "  border: 2px solid black;"
            "  border-radius: 5px;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #e0e0e0;"
            "}"
        );
        key->setStyleSheet(whiteKeyStyle);
        keyOriginalStyles[keyId] = whiteKeyStyle;  // Store original style
        
        // Create label at bottom with keybind, positioned on top of button
        QLabel *noteLabel = new QLabel(keybind, keyContainer);
        noteLabel->setAlignment(Qt::AlignCenter);
        // Make C4 (middle C, keybind "q") bold and red, others normal weight and black
        QString labelStyle;
        if (isMiddleC) {
            labelStyle = QString(
                "QLabel {"
                "  background-color: transparent;"
                "  color: red;"
                "  font-size: 16px;"
                "  font-weight: bold;"
                "}"
            );
        } else {
            labelStyle = QString(
                "QLabel {"
                "  background-color: transparent;"
                "  color: black;"
                "  font-size: 16px;"
                "  font-weight: normal;"
                "}"
            );
        }
        noteLabel->setStyleSheet(labelStyle);
        // Position label at bottom of container
        noteLabel->setGeometry(0, whiteKeyHeight - 30, whiteKeyWidth, 30);
        noteLabel->raise();  // Put label on top so it's visible
        
        whiteKeysLayout->addWidget(keyContainer);
        pianoKeys[keyId] = key;
    }
    
    mainLayout->addWidget(pianoKeysContainer);
    
    // Add pedal indicators
    QHBoxLayout *pedalLayout = new QHBoxLayout();
    pedalLayout->setAlignment(Qt::AlignCenter);
    pedalLayout->setSpacing(20);
    
    unaCordaIndicator = new QCheckBox("Una Corda (Soft Pedal) - N", centralWidget);
    unaCordaIndicator->setEnabled(false);  // Disable interaction, just show state
    unaCordaIndicator->setChecked(false);
    unaCordaIndicator->setStyleSheet(
        "QCheckBox {"
        "  font-size: 14px;"
        "  padding: 5px;"
        "}"
    );
    pedalLayout->addWidget(unaCordaIndicator);
    
    damperPedalIndicator = new QCheckBox("Damper Pedal (Sustain) - M", centralWidget);
    damperPedalIndicator->setEnabled(false);  // Disable interaction, just show state
    damperPedalIndicator->setChecked(false);
    damperPedalIndicator->setStyleSheet(
        "QCheckBox {"
        "  font-size: 14px;"
        "  padding: 5px;"
        "}"
    );
    pedalLayout->addWidget(damperPedalIndicator);
    
    // Add "Press ESC to quit" label
    QLabel *quitLabel = new QLabel("Press ESC to quit", centralWidget);
    quitLabel->setStyleSheet(
        "QLabel {"
        "  font-size: 14px;"
        "  padding: 5px;"
        "  color: #666666;"
        "}"
    );
    pedalLayout->addWidget(quitLabel);
    
    mainLayout->addLayout(pedalLayout);
    
    // Resize window to fit the piano keys with symmetric margins
    // Add small symmetric padding to account for window frame
    int padding = 10;  // Symmetric padding on both sides
    int windowWidth = containerWidth + (padding * 2);
    int windowHeight = whiteKeyHeight + 120;  // Taller window, add space for title and pedal indicators
    resize(windowWidth, windowHeight);
    
    // Center the piano container horizontally with symmetric margins
    mainLayout->setContentsMargins(padding, 0, padding, 0);
    
    // Create black keys with absolute positioning to overlap white keys (three octaves)
    // Black keys need octave numbers for audio playback
    QStringList blackNotes = {
        "C#3", "D#3", "F#3", "G#3", "A#3",  // Octave 3
        "C#4", "D#4", "F#4", "G#4", "A#4",  // Octave 4
        "C#5", "D#5", "F#5", "G#5", "A#5"   // Octave 5
    };
    const int blackKeyWidth = 38;  // Proportionally narrower
    const int blackKeyHeight = 170;  // Taller black keys
    const int blackKeyY = 0;  // Position at top of container
    
    // Black keys are positioned between specific white key pairs for three octaves:
    // Each octave has 5 black keys: C#, D#, F#, G#, A#
    // Pattern repeats every 7 white keys
    // Each entry is the pair of white key indices: {leftWhiteKeyIndex, rightWhiteKeyIndex}
    QList<QPair<int, int>> blackKeyPositions = {
        // Octave 1
        {0, 1},   // C# between C and D
        {1, 2},   // D# between D and E
        {3, 4},   // F# between F and G
        {4, 5},   // G# between G and A
        {5, 6},   // A# between A and B
        // Octave 2
        {7, 8},   // C# between C and D
        {8, 9},   // D# between D and E
        {10, 11}, // F# between F and G
        {11, 12}, // G# between G and A
        {12, 13}, // A# between A and B
        // Octave 3
        {14, 15}, // C# between C and D
        {15, 16}, // D# between D and E
        {17, 18}, // F# between F and G
        {18, 19}, // G# between G and A
        {19, 20}  // A# between A and B
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
        
        // Get keybind for this black key note
        QString keybind = noteToKeybind.value(blackNotes[i], blackNotes[i]);
        
        QPushButton *key = new QPushButton(keybind, pianoKeysContainer);
        key->setGeometry(blackKeyX, blackKeyY, blackKeyWidth, blackKeyHeight);
        key->raise();  // Ensure black keys are on top
        QString blackKeyStyle = QString(
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
        key->setStyleSheet(blackKeyStyle);
        keyOriginalStyles[keyId] = blackKeyStyle;  // Store original style
        pianoKeys[keyId] = key;
    }
    
    // Set the central widget
    setCentralWidget(centralWidget);
}

QByteArray MainWindow::loadWavPcmData(const QString &filePath, int &sampleRate, int &channels)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open WAV file:" << filePath;
        return QByteArray();
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Read RIFF header
    char riff[4], wave[4];
    quint32 fileSize;
    stream.readRawData(riff, 4);
    stream >> fileSize;
    stream.readRawData(wave, 4);
    
    if (QByteArray(riff, 4) != "RIFF" || QByteArray(wave, 4) != "WAVE") {
        qWarning() << "Invalid WAV file format:" << filePath;
        file.close();
        return QByteArray();
    }
    
    // Read fmt chunk to get audio format
    char chunkId[4];
    quint32 chunkSize;
    quint16 audioFormat, numChannels;
    quint32 fileSampleRate, byteRate;
    quint16 blockAlign, bitsPerSample;
    
    bool foundFmt = false;
    while (!stream.atEnd()) {
        stream.readRawData(chunkId, 4);
        stream >> chunkSize;
        
        if (QByteArray(chunkId, 4) == "fmt ") {
            stream >> audioFormat;  // Should be 1 for PCM
            stream >> numChannels;
            stream >> fileSampleRate;
            stream >> byteRate;
            stream >> blockAlign;
            stream >> bitsPerSample;
            
            sampleRate = fileSampleRate;
            channels = numChannels;
            
            if (bitsPerSample != 16) {
                qWarning() << "Only 16-bit PCM is supported:" << filePath;
                file.close();
                return QByteArray();
            }
            
            foundFmt = true;
            break;
        } else {
            // Skip this chunk
            file.seek(file.pos() + chunkSize);
        }
    }
    
    if (!foundFmt) {
        qWarning() << "Could not find fmt chunk in WAV file:" << filePath;
        file.close();
        return QByteArray();
    }
    
    // Find "data" chunk
    file.seek(12);  // Reset to after WAVE header
    quint32 dataSize = 0;
    qint64 dataOffset = -1;
    
    while (!stream.atEnd()) {
        stream.readRawData(chunkId, 4);
        stream >> chunkSize;
        
        if (QByteArray(chunkId, 4) == "data") {
            dataOffset = file.pos();
            dataSize = chunkSize;
            break;
        } else {
            // Skip this chunk
            file.seek(file.pos() + chunkSize);
        }
    }
    
    file.close();
    
    if (dataOffset == -1) {
        qWarning() << "Could not find data chunk in WAV file:" << filePath;
        return QByteArray();
    }
    
    // Read the actual PCM data
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    file.seek(dataOffset);
    QByteArray pcmData = file.read(dataSize);
    file.close();
    
    return pcmData;
}

void MainWindow::setupAudio()
{
    // Load all audio files and determine common format
    QSet<QString> uniqueNotes;
    for (auto it = pianoKeys.begin(); it != pianoKeys.end(); ++it) {
        QString keyId = it.key();
        QString note = keyId.split('_').first();
        uniqueNotes.insert(note);
    }
    
    // Preload all audio files into memory as PCM data
    int commonChannels = 2;
    bool firstFile = true;
    
    for (const QString &note : uniqueNotes) {
        QString wavPath = getAudioFilePath(note);
        QFileInfo fileInfo(wavPath);
        
        if (!fileInfo.exists()) {
            qWarning() << "Audio file not found:" << wavPath;
            continue;
        }
        
        // Load WAV file and extract PCM data
        int sampleRate, channels;
        QByteArray pcmData = loadWavPcmData(wavPath, sampleRate, channels);
        if (!pcmData.isEmpty()) {
            audioBuffers[note] = pcmData;
            audioSampleRates[note] = sampleRate;
            audioChannels[note] = channels;
            
            // Use the first file's channel count
            if (firstFile) {
                commonChannels = channels;
                firstFile = false;
            }
        }
    }
    
    // We'll try to use a higher sample rate for lower latency
    // The actual sample rate will be determined when setting up the audio unit
    outputChannels = commonChannels;
    
    // Pre-allocate mix buffer for maximum callback size (typically 512 frames max)
    // This avoids allocations in the audio callback which can cause lag
    mixBuffer.resize(512 * outputChannels);
    
    qDebug() << "Preloaded" << audioBuffers.size() << "audio files into memory";
    qDebug() << "Audio format: SampleRate:" << outputSampleRate << "Channels:" << outputChannels;
    qDebug() << "All samples are pre-loaded and ready for direct playback";
    
    // Setup Core Audio with minimum latency
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (!component) {
        qWarning() << "Failed to find audio component";
        return;
    }
    
    OSStatus err = AudioComponentInstanceNew(component, &audioUnit);
    if (err != noErr) {
        qWarning() << "Failed to create audio unit:" << err;
        return;
    }
    
    // Set up audio format - use 44.1kHz
    outputSampleRate = 44100;
    AudioStreamBasicDescription audioFormat;
    audioFormat.mSampleRate = outputSampleRate;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    audioFormat.mBitsPerChannel = 16;
    audioFormat.mChannelsPerFrame = outputChannels;
    audioFormat.mBytesPerFrame = audioFormat.mChannelsPerFrame * sizeof(SInt16);
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mBytesPerPacket = audioFormat.mBytesPerFrame * audioFormat.mFramesPerPacket;
    audioFormat.mReserved = 0;
    
    // Set format on output scope
    err = AudioUnitSetProperty(audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input,
                               0,
                               &audioFormat,
                               sizeof(audioFormat));
    if (err != noErr) {
        qWarning() << "Failed to set audio format:" << err;
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
        return;
    }
    
    // Set render callback
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = audioRenderCallback;
    callbackStruct.inputProcRefCon = this;
    
    err = AudioUnitSetProperty(audioUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0,
                               &callbackStruct,
                               sizeof(callbackStruct));
    if (err != noErr) {
        qWarning() << "Failed to set render callback:" << err;
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
        return;
    }
    
    // Initialize audio unit
    err = AudioUnitInitialize(audioUnit);
    if (err != noErr) {
        qWarning() << "Failed to initialize audio unit:" << err;
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
        return;
    }
    
    // Start audio unit
    err = AudioOutputUnitStart(audioUnit);
    if (err != noErr) {
        qWarning() << "Failed to start audio unit:" << err;
        AudioUnitUninitialize(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
        return;
    }
    
    // Get actual latency information
    Float64 latencySeconds = 0;
    UInt32 latencySize = sizeof(latencySeconds);
    AudioUnitGetProperty(audioUnit,
                        kAudioUnitProperty_Latency,
                        kAudioUnitScope_Global,
                        0,
                        &latencySeconds,
                        &latencySize);
    
    qDebug() << "Core Audio started with minimum latency";
    qDebug() << "  Sample rate:" << outputSampleRate << "Hz";
    qDebug() << "  Channels:" << outputChannels;
    if (latencySeconds > 0) {
        qDebug() << "  AudioUnit latency:" << (latencySeconds * 1000.0) << "ms";
    }
    qDebug() << "  Direct sample playback - no effects or processing";
    qDebug() << "  All samples pre-loaded in memory for instant playback";
}

QString MainWindow::getAudioFilePath(const QString &note)
{
    // Convert note name to match audio file naming convention
    // Files use: Piano.ff.C4.wav, Piano.ff.Db4.wav, etc.
    // Our notes use: C4, C#4, D4, D#4, etc.
    
    QString noteName = note.left(note.length() - 1);  // Get note without octave
    QString octave = note.right(1);  // Get octave number
    
    // Map sharp notes to flat equivalents (file naming uses flats)
    QMap<QString, QString> noteMap;
    noteMap["C"] = "C";
    noteMap["C#"] = "Db";  // C# = Db
    noteMap["D"] = "D";
    noteMap["D#"] = "Eb";  // D# = Eb
    noteMap["E"] = "E";
    noteMap["F"] = "F";
    noteMap["F#"] = "Gb";  // F# = Gb
    noteMap["G"] = "G";
    noteMap["G#"] = "Ab";  // G# = Ab
    noteMap["A"] = "A";
    noteMap["A#"] = "Bb";  // A# = Bb
    noteMap["B"] = "B";
    
    QString mappedNote = noteMap.value(noteName, noteName);
    QString fileName = QString("Piano.ff.%1%2.wav").arg(mappedNote).arg(octave);
    
    // Try multiple paths to find the audio file
    QStringList possiblePaths;
    
    // 1. Relative to executable (for deployed app)
    QString appDir = QCoreApplication::applicationDirPath();
    possiblePaths << appDir + "/../src/NotesFF/" + fileName;
    possiblePaths << appDir + "/NotesFF/" + fileName;
    
    // 2. Relative to current working directory (for development)
    possiblePaths << "src/NotesFF/" + fileName;
    possiblePaths << QDir::currentPath() + "/src/NotesFF/" + fileName;
    
    // 3. Absolute path from project root
    possiblePaths << QDir::currentPath() + "/../src/NotesFF/" + fileName;
    
    // Find the first existing file
    for (const QString &path : possiblePaths) {
        QFileInfo fileInfo(path);
        if (fileInfo.exists()) {
            return QDir::cleanPath(fileInfo.absoluteFilePath());
        }
    }
    
    // Return the most likely path (for error reporting)
    return possiblePaths.first();
}

OSStatus MainWindow::audioRenderCallback(void *inRefCon,
                                         AudioUnitRenderActionFlags *ioActionFlags,
                                         const AudioTimeStamp *inTimeStamp,
                                         UInt32 inBusNumber,
                                         UInt32 inNumberFrames,
                                         AudioBufferList *ioData)
{
    Q_UNUSED(ioActionFlags);
    Q_UNUSED(inTimeStamp);
    Q_UNUSED(inBusNumber);
    
    MainWindow *mainWindow = static_cast<MainWindow*>(inRefCon);
    
    // Get output buffer
    AudioBuffer *buffer = &ioData->mBuffers[0];
    SInt16 *out = static_cast<SInt16*>(buffer->mData);
    UInt32 framesPerBuffer = inNumberFrames;
    int samplesPerFrame = mainWindow->outputChannels;
    UInt32 totalSamples = framesPerBuffer * samplesPerFrame;
    
    // Ensure mix buffer is large enough (pre-allocated to avoid allocations)
    if (mainWindow->mixBuffer.size() < static_cast<int>(totalSamples)) {
        mainWindow->mixBuffer.resize(totalSamples);
    }
    
    // Clear mix buffer (use 32-bit for accumulation to avoid clipping)
    qint32 *mix = mainWindow->mixBuffer.data();
    for (UInt32 i = 0; i < totalSamples; ++i) {
        mix[i] = 0;
    }
    
    // First, quickly add any pending notes to active notes (very fast operation)
    {
        QMutexLocker pendingLock(&mainWindow->pendingNotesMutex);
        if (!mainWindow->pendingNotes.isEmpty()) {
            QMutexLocker activeLock(&mainWindow->activeNotesMutex);
            mainWindow->activeNotes.append(mainWindow->pendingNotes);
            mainWindow->pendingNotes.clear();
        }
    }
    
    // Lock and mix all active notes (minimize lock time by working directly)
    // We need to lock because we're modifying positions
    QMutexLocker locker(&mainWindow->activeNotesMutex);
    
    // Check damper pedal state
    bool damperActive = false;
    {
        QMutexLocker damperLock(&mainWindow->damperPedalMutex);
        damperActive = mainWindow->damperPedalActive;
    }
    
    // Mix all active notes
    for (int i = mainWindow->activeNotes.size() - 1; i >= 0; --i) {
        ActiveNote &activeNote = mainWindow->activeNotes[i];
        
        // Check if note has reached the end of its sample data
        if (activeNote.position >= activeNote.length) {
            // If damper pedal is active, sustain it immediately
            if (damperActive && !activeNote.isSustained) {
                activeNote.isSustained = true;
                activeNote.sustainVolume = 1.0;  // Start at full volume
            }
            
            // If note is sustained, apply fade-out
            if (activeNote.isSustained) {
                // Calculate fade rate based on damper state
                double fadeRate;
                if (damperActive) {
                    // While damper is held, very slow fade (over ~5 seconds)
                    fadeRate = 1.0 / (mainWindow->outputSampleRate * 5.0);
                } else {
                    // Damper released - faster fade (over ~0.5 seconds for quick release)
                    fadeRate = 1.0 / (mainWindow->outputSampleRate * 0.5);
                }
                
                // For sustained notes, use the last sample of the audio
                // Apply 1.1x volume multiplier to make sustain more noticeable
                double sustainVolumeMultiplier = 1.1;
                // Use the last sample of the audio for sustain
                int lastSampleIndex = activeNote.length - activeNote.channels;
                if (lastSampleIndex < 0) lastSampleIndex = 0;
                
                // Render sustained note with per-frame volume decay
                for (UInt32 frame = 0; frame < framesPerBuffer; ++frame) {
                    // Calculate current volume for this frame (linear decay)
                    double currentVolume = activeNote.sustainVolume;
                    double frameVolume = currentVolume * sustainVolumeMultiplier;
                    
                    for (int ch = 0; ch < activeNote.channels && ch < samplesPerFrame; ++ch) {
                        if (lastSampleIndex + ch < activeNote.length) {
                            qint16 lastSample = activeNote.data[lastSampleIndex + ch];
                            UInt32 outIndex = frame * samplesPerFrame + ch;
                            if (outIndex < totalSamples) {
                                mix[outIndex] += static_cast<qint32>(lastSample * frameVolume);
                            }
                        }
                    }
                    
                    // Update volume for next frame (decay per frame)
                    activeNote.sustainVolume = qMax(0.0, activeNote.sustainVolume - fadeRate);
                }
                
                // Remove note when volume reaches zero
                if (activeNote.sustainVolume <= 0.0) {
                    mainWindow->activeNotes.removeAt(i);
                    continue;
                }
                continue;
            } else {
                // Note finished and not sustained - remove it
                mainWindow->activeNotes.removeAt(i);
                continue;
            }
        }
        
        // If damper pedal is active, mark note to be sustained (but continue playing normally)
        // The note will sustain when it reaches the end of its sample data
        if (damperActive && !activeNote.isSustained) {
            // Mark as "will be sustained" - note continues playing normally
            // When it reaches the end, it will start sustaining
            activeNote.isSustained = true;
            activeNote.sustainVolume = 1.0;
        }
        
        // Note: If isSustained is true but position < length, the note is still playing
        // and will continue to play normally. It will only start sustaining when
        // position >= length (handled in the check above)
        
        // Check if note has reached 1 second cutoff (only if damper is NOT active and not sustained)
        if (activeNote.framesPlayed >= mainWindow->outputSampleRate && !damperActive && !activeNote.isSustained) {
            // Note has played for 1 second and damper is not active - cut it off
            mainWindow->activeNotes.removeAt(i);
            continue;
        }
        
        // Calculate how many frames we can still play before hitting 1 second limit
        int framesRemaining = mainWindow->outputSampleRate - activeNote.framesPlayed;
        UInt32 framesToPlay = framesPerBuffer;
        if (!damperActive && framesRemaining > 0 && framesRemaining < static_cast<int>(framesPerBuffer)) {
            // Limit to remaining frames before 1 second cutoff
            framesToPlay = static_cast<UInt32>(framesRemaining);
        }
        
        // Calculate how many samples we need from this note
        int noteSamplesPerFrame = activeNote.channels;
        UInt32 noteSamplesNeeded = framesToPlay * noteSamplesPerFrame;
        int noteSamplesAvailable = activeNote.length - activeNote.position;
        UInt32 samplesToMix = qMin(static_cast<UInt32>(noteSamplesAvailable), noteSamplesNeeded);
        
        // Fast path: same sample rate and channels - direct mixing (no processing, no effects)
        if (activeNote.sampleRate == mainWindow->outputSampleRate && 
            activeNote.channels == mainWindow->outputChannels) {
            // Direct sample playback - just mix samples directly, no processing
            const qint16 *noteData = activeNote.data + activeNote.position;
            // Unroll loop for better performance (process 4 samples at a time when possible)
            UInt32 j = 0;
            for (; j + 3 < samplesToMix && j + 3 < totalSamples; j += 4) {
                mix[j] += static_cast<qint32>(noteData[j]);
                mix[j+1] += static_cast<qint32>(noteData[j+1]);
                mix[j+2] += static_cast<qint32>(noteData[j+2]);
                mix[j+3] += static_cast<qint32>(noteData[j+3]);
            }
            // Handle remaining samples
            for (; j < samplesToMix && j < totalSamples; ++j) {
                mix[j] += static_cast<qint32>(noteData[j]);
            }
            activeNote.position += static_cast<int>(samplesToMix);
        } else {
            // Sample rate conversion needed (simplified)
            double ratio = static_cast<double>(activeNote.sampleRate) / mainWindow->outputSampleRate;
            for (UInt32 frame = 0; frame < framesToPlay; ++frame) {
                double noteFrame = activeNote.position / static_cast<double>(noteSamplesPerFrame) + frame * ratio;
                int noteSampleIndex = static_cast<int>(noteFrame * noteSamplesPerFrame);
                
                if (noteSampleIndex < activeNote.length) {
                    for (int ch = 0; ch < samplesPerFrame && ch < noteSamplesPerFrame; ++ch) {
                        UInt32 outIndex = frame * samplesPerFrame + ch;
                        int noteIndex = noteSampleIndex + ch;
                        if (outIndex < totalSamples && noteIndex < activeNote.length) {
                            mix[outIndex] += static_cast<qint32>(activeNote.data[noteIndex]);
                        }
                    }
                }
            }
            activeNote.position += static_cast<int>(framesToPlay * ratio * noteSamplesPerFrame);
        }
        
        // Update frames played counter
        activeNote.framesPlayed += framesToPlay;
        
        // Check again if we've hit 1 second after updating (only if damper is NOT active)
        if (activeNote.framesPlayed >= mainWindow->outputSampleRate && !damperActive && !activeNote.isSustained) {
            // Cut it off (damper is not active, so no sustain)
            mainWindow->activeNotes.removeAt(i);
            continue;
        }
    }
    
    // Check if una corda (soft pedal) is active
    bool unaCorda = false;
    {
        QMutexLocker lock(&mainWindow->unaCordaMutex);
        unaCorda = mainWindow->unaCordaActive;
    }
    
    // Apply una corda effect: volume reduction (15%) and low-pass filter for muffled tone
    if (unaCorda) {
        // Low-pass filter parameters (cutoff ~2500 Hz for muffled sound)
        // alpha = dt / (dt + RC), where RC = 1 / (2 * pi * cutoff)
        // For 44.1kHz sample rate and 2500 Hz cutoff:
        double cutoffFreq = 2500.0;
        double dt = 1.0 / mainWindow->outputSampleRate;
        double rc = 1.0 / (2.0 * M_PI * cutoffFreq);
        double alpha = dt / (dt + rc);
        
        // Apply low-pass filter and volume reduction (0.79 = 21% reduction)
        double volumeMultiplier = 0.79;
        
        for (UInt32 i = 0; i < totalSamples; ++i) {
            int channel = i % samplesPerFrame;
            double filtered = alpha * mix[i] + (1.0 - alpha) * mainWindow->lowPassFilterState[channel];
            mainWindow->lowPassFilterState[channel] = filtered;
            qint32 finalValue = static_cast<qint32>(filtered * volumeMultiplier);
            out[i] = static_cast<SInt16>(qBound(-32768, finalValue, 32767));
        }
    } else {
        // Reset filter state when una corda is not active
        for (int ch = 0; ch < samplesPerFrame; ++ch) {
            mainWindow->lowPassFilterState[ch] = 0.0;
        }
        
        // Convert mix buffer to output with clipping protection (normal volume)
        for (UInt32 i = 0; i < totalSamples; ++i) {
            out[i] = static_cast<SInt16>(qBound(-32768, mix[i], 32767));
        }
    }
    
    return noErr;
}

void MainWindow::playNote(const QString &note)
{
    // Fast path - check if note exists (no lock needed for read)
    auto it = audioBuffers.find(note);
    if (it == audioBuffers.end() || it->isEmpty()) {
        return;  // Silently fail for speed
    }
    
    // Use const reference to avoid copy
    const QByteArray &audioData = *it;
    
    // Get audio format for this note (cached lookups, fast)
    int sampleRate = audioSampleRates.value(note, outputSampleRate);
    int channels = audioChannels.value(note, outputChannels);
    
    // Convert QByteArray to const qint16* pointer (no copy, just pointer)
    const qint16 *pcmData = reinterpret_cast<const qint16*>(audioData.constData());
    int sampleCount = audioData.size() / sizeof(qint16);
    
    // Create active note on stack (fast, no allocation)
    ActiveNote activeNote;
    activeNote.data = pcmData;
    activeNote.position = 0;
    activeNote.length = sampleCount;
    activeNote.sampleRate = sampleRate;
    activeNote.channels = channels;
    activeNote.isSustained = false;
    activeNote.sustainVolume = 1.0;
    activeNote.framesPlayed = 0;  // Initialize frames played counter
    
    // Add to pending notes queue (very fast, rarely blocks)
    // The audio callback will move these to active notes
    // This prevents blocking when many keys are pressed quickly
    QMutexLocker locker(&pendingNotesMutex);
    pendingNotes.append(activeNote);
    
    // Highlight the key visually
    highlightKey(note);
}

void MainWindow::connectKeySignals()
{
    // Connect all piano keys to playNote slot
    for (auto it = pianoKeys.begin(); it != pianoKeys.end(); ++it) {
        QString keyId = it.key();
        QPushButton *button = it.value();
        
        // Extract note name from keyId (format: "C4_7" -> "C4")
        QString note = keyId.split('_').first();
        
        // Connect button click to playNote
        connect(button, &QPushButton::clicked, this, [this, note]() {
            playNote(note);
        });
    }
}

void MainWindow::highlightKey(const QString &note)
{
    // Find all keys that match this note (could be multiple if same note appears in different octaves)
    for (auto it = pianoKeys.begin(); it != pianoKeys.end(); ++it) {
        QString keyId = it.key();
        QPushButton *button = it.value();
        
        // Extract note name from keyId (format: "C4_7" -> "C4")
        QString keyNote = keyId.split('_').first();
        
        if (keyNote == note) {
            // Determine if this is a white or black key based on note name
            bool isBlackKey = keyNote.contains('#');
            
            // Stop any existing fade timer for this key
            if (keyFadeTimers.contains(keyId)) {
                QTimer *existingTimer = keyFadeTimers[keyId];
                if (existingTimer) {
                    existingTimer->stop();
                    existingTimer->deleteLater();
                }
                keyFadeTimers.remove(keyId);
            }
            
            // Set highlighted color immediately
            QString highlightStyle;
            if (isBlackKey) {
                // Black key: highlight with dark blue
                highlightStyle = QString(
                    "QPushButton {"
                    "  background-color: #1E3A8A;"
                    "  color: white;"
                    "  border: 1px solid gray;"
                    "  border-radius: 3px;"
                    "  font-size: 12px;"
                    "  font-weight: bold;"
                    "}"
                );
            } else {
                // White key: highlight with light yellow
                highlightStyle = QString(
                    "QPushButton {"
                    "  background-color:rgb(149, 199, 255);"
                    "  border: 2px solid black;"
                    "  border-radius: 5px;"
                    "}"
                );
            }
            button->setStyleSheet(highlightStyle);
            
            // Create fade timer to restore original style with smooth fade
            QTimer *fadeTimer = new QTimer(this);
            fadeTimer->setInterval(20);  // Update every 20ms for smooth fade
            keyFadeSteps[keyId] = 0;  // Start fade step counter
            
            connect(fadeTimer, &QTimer::timeout, this, [this, keyId, button, fadeTimer, isBlackKey]() {
                int step = keyFadeSteps[keyId];
                step++;
                keyFadeSteps[keyId] = step;
                
                // Fade over 10 steps (200ms total)
                const int totalSteps = 10;
                if (step >= totalSteps) {
                    // Fade complete - restore original style
                    if (keyOriginalStyles.contains(keyId)) {
                        button->setStyleSheet(keyOriginalStyles[keyId]);
                    }
                    // Clean up
                    fadeTimer->stop();
                    fadeTimer->deleteLater();
                    keyFadeTimers.remove(keyId);
                    keyFadeSteps.remove(keyId);
                } else {
                    // Interpolate between highlight and original color
                    double progress = 1.0 - (static_cast<double>(step) / totalSteps);
                    
                    if (isBlackKey) {
                        // Fade from dark blue (#1E3A8A) back to black
                        int r1 = 0x1E, g1 = 0x3A, b1 = 0x8A;  // Dark blue
                        int r2 = 0x00, g2 = 0x00, b2 = 0x00;  // Black
                        int r = static_cast<int>(r1 * progress + r2 * (1.0 - progress));
                        int g = static_cast<int>(g1 * progress + g2 * (1.0 - progress));
                        int b = static_cast<int>(b1 * progress + b2 * (1.0 - progress));
                        QString fadeStyle = QString(
                            "QPushButton {"
                            "  background-color: rgb(%1, %2, %3);"
                            "  color: white;"
                            "  border: 1px solid gray;"
                            "  border-radius: 3px;"
                            "  font-size: 12px;"
                            "  font-weight: bold;"
                            "}"
                        ).arg(r).arg(g).arg(b);
                        button->setStyleSheet(fadeStyle);
                    } else {
                        // Fade from light blue (149, 199, 255) back to white
                        int r1 = 149, g1 = 199, b1 = 255;  // Light blue
                        int r2 = 0xFF, g2 = 0xFF, b2 = 0xFF;  // White
                        int r = static_cast<int>(r1 * progress + r2 * (1.0 - progress));
                        int g = static_cast<int>(g1 * progress + g2 * (1.0 - progress));
                        int b = static_cast<int>(b1 * progress + b2 * (1.0 - progress));
                        QString fadeStyle = QString(
                            "QPushButton {"
                            "  background-color: rgb(%1, %2, %3);"
                            "  border: 2px solid black;"
                            "  border-radius: 5px;"
                            "}"
                        ).arg(r).arg(g).arg(b);
                        button->setStyleSheet(fadeStyle);
                    }
                }
            });
            
            keyFadeTimers[keyId] = fadeTimer;
            fadeTimer->start();
            
            // Only highlight the first matching key (in case of duplicates)
            break;
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Check for Escape key to quit
    if (event->key() == Qt::Key_Escape) {
        QCoreApplication::quit();
        event->accept();
        return;
    }
    
    // Check for pedal keys: N for una corda, M for damper pedal
    int key = event->key();
    if (key == Qt::Key_N) {
        // Una corda (soft pedal)
        QMutexLocker lock(&unaCordaMutex);
        unaCordaActive = true;
        unaCordaIndicator->setChecked(true);
        event->accept();
        return;
    } else if (key == Qt::Key_M) {
        // Damper pedal (sustain)
        QMutexLocker lock(&damperPedalMutex);
        damperPedalActive = true;
        damperPedalIndicator->setChecked(true);
        event->accept();
        return;
    }
    
    // Map keyboard keys to piano notes
    // Each keyboard row = one octave, keys in order: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
    // First octave (C3-B3): number row - 1=C, 2=C#, 3=D, 4=D#, 5=E, 6=F, 7=F#, 8=G, 9=G#, 0=A, -=A#, ==B
    // Second octave (C4-B4): qwert row - q=C, w=C#, e=D, r=D#, t=E, y=F, u=F#, i=G, o=G#, p=A, [=A#, ]=B
    // Third octave (C5-B5): asdfg row - a=C, s=C#, d=D, f=D#, g=E, h=F, j=F#, k=G, l=G#, ;=A, '=A#, \=B
    
    QString note;
    // key variable already defined above for pedal checking
    
    // First octave (C3-B3) - number row in order
    if (key == Qt::Key_1) note = "C3";
    else if (key == Qt::Key_2) note = "C#3";
    else if (key == Qt::Key_3) note = "D3";
    else if (key == Qt::Key_4) note = "D#3";
    else if (key == Qt::Key_5) note = "E3";
    else if (key == Qt::Key_6) note = "F3";
    else if (key == Qt::Key_7) note = "F#3";
    else if (key == Qt::Key_8) note = "G3";
    else if (key == Qt::Key_9) note = "G#3";
    else if (key == Qt::Key_0) note = "A3";
    else if (key == Qt::Key_Minus) note = "A#3";
    else if (key == Qt::Key_Equal) note = "B3";
    
    // Second octave (C4-B4) - qwert row in order
    else if (key == Qt::Key_Q) note = "C4";
    else if (key == Qt::Key_W) note = "C#4";
    else if (key == Qt::Key_E) note = "D4";
    else if (key == Qt::Key_R) note = "D#4";
    else if (key == Qt::Key_T) note = "E4";
    else if (key == Qt::Key_Y) note = "F4";
    else if (key == Qt::Key_U) note = "F#4";
    else if (key == Qt::Key_I) note = "G4";
    else if (key == Qt::Key_O) note = "G#4";
    else if (key == Qt::Key_P) note = "A4";
    else if (key == Qt::Key_BracketLeft) note = "A#4";
    else if (key == Qt::Key_BracketRight) note = "B4";
    
    // Third octave (C5-B5) - asdfg row in order
    else if (key == Qt::Key_A) note = "C5";
    else if (key == Qt::Key_S) note = "C#5";
    else if (key == Qt::Key_D) note = "D5";
    else if (key == Qt::Key_F) note = "D#5";
    else if (key == Qt::Key_G) note = "E5";
    else if (key == Qt::Key_H) note = "F5";
    else if (key == Qt::Key_J) note = "F#5";
    else if (key == Qt::Key_K) note = "G5";
    else if (key == Qt::Key_L) note = "G#5";
    else if (key == Qt::Key_Semicolon) note = "A5";
    else if (key == Qt::Key_Apostrophe || key == Qt::Key_QuoteDbl) note = "A#5";
    else if (key == Qt::Key_Backslash) note = "B5";
    
    // C6 uses z key
    else if (key == Qt::Key_Z) note = "C6";
    
    if (!note.isEmpty()) {
        playNote(note);
        event->accept();
        return;
    }
    
    // Let parent handle other keys
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // Check for pedal key release: N for una corda, M for damper pedal
    int key = event->key();
    if (key == Qt::Key_N) {
        // Una corda (soft pedal) released
        QMutexLocker lock(&unaCordaMutex);
        unaCordaActive = false;
        unaCordaIndicator->setChecked(false);
        event->accept();
        return;
    } else if (key == Qt::Key_M) {
        // Damper pedal (sustain) released
        QMutexLocker lock(&damperPedalMutex);
        damperPedalActive = false;
        damperPedalIndicator->setChecked(false);
        event->accept();
        return;
    }
    
    QMainWindow::keyReleaseEvent(event);
}