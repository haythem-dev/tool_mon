#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QRegularExpression>
#include <QtCore/QDateTime>
#include <QtCore/QSettings>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtCore/QMap>

struct PatternInfo {
    QRegularExpression regex;
    bool highlight = false;
    int before = 0;
    int after = 0;
};

class FileMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileMonitorWidget(QWidget *parent = nullptr);

private slots:
    void selectFiles();
    void selectFileList();
    void selectPatternsFile();
    void toggleMonitoring();
    void reloadConfig();
    void checkFiles();
    void showAboutBox();

private:
    void loadPatterns(const QString &iniPath);
    void loadFileList(const QString &listPath);
    void reportConfig();

    QStringList monitoredFilePaths;
    QString patternsFilePath;
    QMap<QString, QFile*> monitoredFiles;
    QMap<QString, qint64> lastPositions;
    QVector<PatternInfo> patternInfos;

    QPushButton *fileButton;
    QPushButton *fileListButton;
    QPushButton *patternsButton;
    QPushButton *startButton;
    QPushButton *reloadButton;
    QPushButton *aboutButton;
    QTextEdit *logEdit;
    QTimer *timer;
    bool monitoring = false;
};