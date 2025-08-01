#include "FileMonitorWidget.h"

FileMonitorWidget::FileMonitorWidget(QWidget* parent)
    : QWidget(parent)
{
    // File/pattern selection buttons
    fileButton = new QPushButton("Select Files...", this);
    fileListButton = new QPushButton("Select File List...", this);
    patternsButton = new QPushButton("Select Patterns INI...", this);

    // Action buttons
    startButton = new QPushButton("Start Monitoring", this);
    reloadButton = new QPushButton("Reload", this);
    aboutButton = new QPushButton("About", this);

    logEdit = new QTextEdit(this);
    logEdit->setReadOnly(true);

    // Layouts
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* selectLayout = new QHBoxLayout();
    selectLayout->addWidget(fileButton);
    selectLayout->addWidget(fileListButton);
    selectLayout->addWidget(patternsButton);

    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->addWidget(startButton);
    actionLayout->addWidget(reloadButton);
    actionLayout->addWidget(aboutButton);

    mainLayout->addLayout(selectLayout);
    mainLayout->addLayout(actionLayout);
    mainLayout->addWidget(logEdit);
    setLayout(mainLayout);

    timer = new QTimer(this);
    timer->setInterval(1000);

    connect(fileButton, &QPushButton::clicked, this, &FileMonitorWidget::selectFiles);
    connect(fileListButton, &QPushButton::clicked, this, &FileMonitorWidget::selectFileList);
    connect(patternsButton, &QPushButton::clicked, this, &FileMonitorWidget::selectPatternsFile);
    connect(startButton, &QPushButton::clicked, this, &FileMonitorWidget::toggleMonitoring);
    connect(reloadButton, &QPushButton::clicked, this, &FileMonitorWidget::reloadConfig);
    connect(timer, &QTimer::timeout, this, &FileMonitorWidget::checkFiles);
    connect(aboutButton, &QPushButton::clicked, this, &FileMonitorWidget::showAboutBox);

    QSettings appSettings;
    monitoredFilePaths = appSettings.value("lastMonitoredFiles").toStringList();
    patternsFilePath = appSettings.value("lastPatternsFile").toString();
    if (!monitoredFilePaths.isEmpty())
        logEdit->append("Last monitored files: " + monitoredFilePaths.join(", "));
    if (!patternsFilePath.isEmpty())
        logEdit->append("Last patterns file: " + patternsFilePath);

    // Initial config report
    reportConfig();
}

void FileMonitorWidget::selectFiles()
{
    QSettings appSettings;
    QString initialPath = monitoredFilePaths.isEmpty() ? QString() : monitoredFilePaths.first();
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Monitor", initialPath);
    if (!files.isEmpty()) {
        monitoredFilePaths = files;
        appSettings.setValue("lastMonitoredFiles", monitoredFilePaths);
        logEdit->append("Selected files: " + monitoredFilePaths.join(", "));
        reportConfig();
    }
}

void FileMonitorWidget::selectFileList()
{
    QString fileListPath = QFileDialog::getOpenFileName(this, "Select File List (Text File)");
    if (!fileListPath.isEmpty()) {
        loadFileList(fileListPath);
        QSettings appSettings;
        appSettings.setValue("lastMonitoredFiles", monitoredFilePaths);
        logEdit->append("Loaded file list: " + fileListPath);
        logEdit->append("Files to monitor: " + monitoredFilePaths.join(", "));
        reportConfig();
    }
}

void FileMonitorWidget::loadFileList(const QString& listPath)
{
    monitoredFilePaths.clear();
    QFile listFile(listPath);
    if (listFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&listFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty())
                monitoredFilePaths.append(line);
        }
        listFile.close();
    }
    else {
        logEdit->append("Failed to open file list: " + listPath);
    }
}

void FileMonitorWidget::selectPatternsFile()
{
    QSettings appSettings;
    QString initialPath = patternsFilePath.isEmpty() ? QString() : patternsFilePath;
    QString file = QFileDialog::getOpenFileName(this, "Select Patterns INI File", initialPath);
    if (!file.isEmpty()) {
        patternsFilePath = file;
        appSettings.setValue("lastPatternsFile", patternsFilePath);
        logEdit->append("Selected patterns file: " + file);
        loadPatterns(patternsFilePath);
        reportConfig();
    }
}

void FileMonitorWidget::toggleMonitoring()
{
    if (!monitoring) {
        // Start monitoring
        if (monitoredFilePaths.isEmpty() || patternsFilePath.isEmpty()) {
            logEdit->append("Please select both files.");
            return;
        }
        loadPatterns(patternsFilePath);

        reportConfig();

        // Open all files and set initial positions
        for (const QString& filePath : monitoredFilePaths) {
            QFile* file = new QFile(filePath);
            if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
                logEdit->append("Failed to open monitored file: " + filePath);
                delete file;
                continue;
            }
            monitoredFiles[filePath] = file;
            lastPositions[filePath] = file->size();
        }
        timer->start();
        monitoring = true;
        startButton->setText("Stop Monitoring");
        logEdit->append("Monitoring started.");
    }
    else {
        // Stop monitoring
        timer->stop();
        for (auto file : monitoredFiles) {
            if (file->isOpen())
                file->close();
            delete file;
        }
        monitoredFiles.clear();
        lastPositions.clear();
        monitoring = false;
        startButton->setText("Start Monitoring");
        logEdit->append("Monitoring stopped.");
    }
}

void FileMonitorWidget::reloadConfig()
{
    loadPatterns(patternsFilePath);
    reportConfig();
    logEdit->append("Reloaded patterns and reported current configuration.");
}

void FileMonitorWidget::reportConfig()
{
    logEdit->append("---- Current Configuration ----");
    logEdit->append("Files to monitor:");
    for (const QString& file : monitoredFilePaths)
        logEdit->append("  " + file);

    logEdit->append("Active patterns:");
    for (const PatternInfo& info : patternInfos) {
        logEdit->append(QString("  Pattern: \"%1\" | Highlight: %2 | Before: %3 | After: %4")
            .arg(info.regex.pattern())
            .arg(info.highlight ? "Yes" : "No")
            .arg(info.before)
            .arg(info.after));
    }
    logEdit->append("------------------------------");
}

void FileMonitorWidget::showAboutBox()
{
    QMessageBox::about(this, "About FileMon",
        QString("FileMon\n\nQt version: %1").arg(QT_VERSION_STR));
}

void FileMonitorWidget::loadPatterns(const QString& iniPath)
{
    patternInfos.clear();
    QSettings settings(iniPath, QSettings::IniFormat);
    QStringList keys = settings.allKeys();

    // Group by pattern prefix (e.g., pattern1, pattern2)
    QMap<QString, QMap<QString, QString>> patternGroups;
    for (const QString& key : keys) {
        int dotIdx = key.indexOf('.');
        if (dotIdx > 0) {
            QString group = key.left(dotIdx);
            QString attr = key.mid(dotIdx + 1);
            patternGroups[group][attr] = settings.value(key).toString().trimmed();
        }
    }

    for (auto it = patternGroups.begin(); it != patternGroups.end(); ++it) {
        const QMap<QString, QString>& attrs = it.value();
        QString patternStr = attrs.value("pattern");
        if (patternStr.isEmpty())
            continue;
        bool highlight = attrs.value("highlight").toLower() == "yes";
        int before = attrs.value("before").toInt();
        int after = attrs.value("after").toInt();
        QRegularExpression re(patternStr);
        if (re.isValid())
            patternInfos.append({ re, highlight, before, after });
    }
    logEdit->append(QString("Loaded %1 patterns.").arg(patternInfos.size()));
}

void FileMonitorWidget::checkFiles()
{
    for (auto it = monitoredFiles.begin(); it != monitoredFiles.end(); ++it) {
        QFile* file = it.value();
        QString filePath = it.key();
        if (!file->isOpen())
            continue;

        file->seek(lastPositions[filePath]);
        QTextStream in(file);

        // Read all new lines into a buffer
        QStringList lines;
        while (!in.atEnd()) {
            lines.append(in.readLine());
        }

        int lineCount = lines.size();
        for (int i = 0; i < lineCount; ++i) {
            const QString& line = lines[i];
            for (const PatternInfo& info : patternInfos) {
                QRegularExpressionMatch match = info.regex.match(line);
                if (match.hasMatch()) {
                    QString time = QDateTime::currentDateTime().toString(Qt::ISODate);
                    QStringList contextLines;

                    // Collect before lines
                    int beforeStart = qMax(0, i - info.before);
                    for (int b = beforeStart; b < i; ++b)
                        contextLines.append(QString("  [before] %1").arg(lines[b]));

                    // The matching line
                    contextLines.append(QString("[%1] Pattern found: \"%2\" in file: %3, line: %4")
                        .arg(time)
                        .arg(info.regex.pattern())
                        .arg(filePath)
                        .arg(line));

                    // Collect after lines
                    int afterEnd = qMin(lineCount, i + 1 + info.after);
                    for (int a = i + 1; a < afterEnd; ++a)
                        contextLines.append(QString("  [after] %1").arg(lines[a]));

                    // Output with or without highlight
                    if (info.highlight) {
                        QTextCursor cursor = logEdit->textCursor();
                        cursor.movePosition(QTextCursor::End);
                        QTextCharFormat fmt;
                        fmt.setBackground(QColor(144, 238, 144)); // Light green
                        fmt.setFontWeight(QFont::Bold);           // Boldface
                        cursor.setCharFormat(fmt);
                        for (const QString& ctx : contextLines)
                            cursor.insertText(ctx + "\n");
                        logEdit->setTextCursor(cursor);
                        logEdit->setCurrentCharFormat(QTextCharFormat());
                    }
                    else {
                        for (const QString& ctx : contextLines)
                            logEdit->append(ctx);
                    }
                }
            }
        }
        lastPositions[filePath] = file->pos();
    }
}