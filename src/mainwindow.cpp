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
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <cmath>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), audioUnit(nullptr), outputSampleRate(44100), outputChannels(2)
{
    setupUI();  // Must be called first to create pianoKeys
    setupAudio();  // Preload audio files after keys are created
    connectKeySignals();
    setWindowTitle("Virtual Piano");
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
    
    // Create white keys for three octaves plus final C with octave numbers
    // Octaves: C3-C4 (first), C4-C5 (second), C5-C6 (third), final C6
    QStringList whiteNotes = {
        "C3", "D3", "E3", "F3", "G3", "A3", "B3",  // Octave 3
        "C4", "D4", "E4", "F4", "G4", "A4", "B4",  // Octave 4 (middle C is C4)
        "C5", "D5", "E5", "F5", "G5", "A5", "B5",  // Octave 5
        "C6"                                        // Final C6
    };
    
    for (int i = 0; i < whiteNotes.size(); i++) {
        const QString &note = whiteNotes[i];
        // Create unique key identifier (note + index to handle duplicates)
        QString keyId = QString("%1_%2").arg(note).arg(i);
        // Check if this is middle C (C4) - index 7
        bool isMiddleC = (i == 7);
        
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
        // Make C4 (middle C) bold and red, others normal weight and black
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
    
    // Resize window to fit the piano keys with symmetric margins
    // Add small symmetric padding to account for window frame
    int padding = 10;  // Symmetric padding on both sides
    int windowWidth = containerWidth + (padding * 2);
    int windowHeight = whiteKeyHeight + 80;  // Taller window, add space for title
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
    
    qDebug() << "Preloaded" << audioBuffers.size() << "audio files into memory";
    qDebug() << "Audio format: SampleRate:" << outputSampleRate << "Channels:" << outputChannels;
    
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
    err = AudioUnitGetProperty(audioUnit,
                               kAudioUnitProperty_Latency,
                               kAudioUnitScope_Global,
                               0,
                               &latencySeconds,
                               &latencySize);
    
    // Calculate buffer time at current sample rate
    // Typical buffer sizes: 32-128 frames
    // At 96kHz: 64 frames = 0.67ms, 32 frames = 0.33ms
    // At 48kHz: 64 frames = 1.33ms, 32 frames = 0.67ms
    // At 44.1kHz: 64 frames = 1.45ms, 32 frames = 0.73ms
    
    qDebug() << "Core Audio started with minimum latency";
    qDebug() << "  Sample rate:" << outputSampleRate << "Hz";
    qDebug() << "  Channels:" << outputChannels;
    if (err == noErr) {
        qDebug() << "  AudioUnit latency:" << (latencySeconds * 1000.0) << "ms";
    }
    qDebug() << "  Estimated buffer time (64 frames):" << (64.0 * 1000.0 / outputSampleRate) << "ms";
    qDebug() << "  Estimated buffer time (32 frames):" << (32.0 * 1000.0 / outputSampleRate) << "ms";
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
    
    // Clear output buffer
    for (UInt32 i = 0; i < totalSamples; ++i) {
        out[i] = 0;
    }
    
    // Lock the active notes list - work directly on it to minimize overhead
    QMutexLocker locker(&mainWindow->activeNotesMutex);
    
    // Mix all active notes
    for (int i = mainWindow->activeNotes.size() - 1; i >= 0; --i) {
        ActiveNote &activeNote = mainWindow->activeNotes[i];
        
        // Calculate how many samples we need from this note
        int noteSamplesPerFrame = activeNote.channels;
        UInt32 noteSamplesNeeded = framesPerBuffer * noteSamplesPerFrame;
        int noteSamplesAvailable = activeNote.length - activeNote.position;
        UInt32 samplesToMix = qMin(static_cast<UInt32>(noteSamplesAvailable), noteSamplesNeeded);
        
        // Handle sample rate conversion if needed (simple linear interpolation)
        if (activeNote.sampleRate == mainWindow->outputSampleRate && 
            activeNote.channels == mainWindow->outputChannels) {
            // Same sample rate and channels - direct mixing (interleaved)
            for (UInt32 j = 0; j < samplesToMix; ++j) {
                int noteIndex = activeNote.position + static_cast<int>(j);
                if (j < totalSamples && noteIndex < activeNote.length) {
                    // Mix with clipping protection
                    qint32 mixed = static_cast<qint32>(out[j]) + static_cast<qint32>(activeNote.data[noteIndex]);
                    out[j] = static_cast<SInt16>(qBound(-32768, mixed, 32767));
                }
            }
            activeNote.position += static_cast<int>(samplesToMix);
        } else {
            // Sample rate conversion needed (simplified - for production use proper resampling)
            double ratio = static_cast<double>(activeNote.sampleRate) / mainWindow->outputSampleRate;
            for (UInt32 frame = 0; frame < framesPerBuffer; ++frame) {
                double noteFrame = activeNote.position / static_cast<double>(noteSamplesPerFrame) + frame * ratio;
                int noteSampleIndex = static_cast<int>(noteFrame * noteSamplesPerFrame);
                
                if (noteSampleIndex < activeNote.length) {
                    for (int ch = 0; ch < samplesPerFrame && ch < noteSamplesPerFrame; ++ch) {
                        UInt32 outIndex = frame * samplesPerFrame + ch;
                        int noteIndex = noteSampleIndex + ch;
                        if (outIndex < totalSamples && noteIndex < activeNote.length) {
                            qint32 mixed = static_cast<qint32>(out[outIndex]) + static_cast<qint32>(activeNote.data[noteIndex]);
                            out[outIndex] = static_cast<SInt16>(qBound(-32768, mixed, 32767));
                        }
                    }
                }
            }
            activeNote.position += static_cast<int>(framesPerBuffer * ratio * noteSamplesPerFrame);
        }
        
        // Remove note if it's finished
        if (activeNote.position >= activeNote.length) {
            mainWindow->activeNotes.removeAt(i);
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
    activeNote.note = note;
    activeNote.data = pcmData;
    activeNote.position = 0;
    activeNote.length = sampleCount;
    activeNote.sampleRate = sampleRate;
    activeNote.channels = channels;
    
    // Add to active notes list (minimal lock time)
    // Use tryLock first to avoid blocking if audio callback is running
    if (activeNotesMutex.tryLock()) {
        activeNotes.append(activeNote);
        activeNotesMutex.unlock();
    } else {
        // If lock is busy (rare), wait briefly
        QMutexLocker locker(&activeNotesMutex);
        activeNotes.append(activeNote);
    }
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