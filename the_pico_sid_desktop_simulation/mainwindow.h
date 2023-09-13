#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "./audiogenerator.h"

#include <QMainWindow>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioOutput>

#include "siddump.h"

extern "C" {
#include "../firmware/sid.h"
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define SOUND_BUFFER_SIZE 8192

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
	void OnFillAudioData(char *data, qint64 len);
    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

private:
    Ui::MainWindow *ui;

    unsigned int bufferSize;
    QAudioDeviceInfo m_device;
    QAudioFormat     m_format;
    QByteArray       m_buffer;
    QAudioOutput*    m_audioOutput;
    AudioGenerator* m_audiogen;

    SIDDumpClass*   sid_dump;
    uint8_t         sid_io;
};
#endif // MAINWINDOW_H
