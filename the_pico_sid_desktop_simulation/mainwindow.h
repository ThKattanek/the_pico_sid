#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "./audiogenerator.h"

#include <QMainWindow>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioOutput>

#include "siddump.h"

extern "C" {
#include "../firmware/pico_sid.h"
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define SOUND_BUFFER_SIZE 8192*2

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
	void OnFillAudioData(char *data, qint64 len);
    void on_Quit_clicked();
    void on_LoadSidDump_clicked();
    void on_CycleExcact_toggled(bool checked);
    void on_mos6581_clicked();
    void on_mos8580_clicked();

private:
    Ui::MainWindow *ui;

    unsigned int bufferSize;
    QAudioDeviceInfo m_device;
    QAudioFormat     m_format;
    QByteArray       m_buffer;
    QAudioOutput*    m_audioOutput;
    AudioGenerator*  m_audiogen;

    PICO_SID         sid;

    uint8_t         sid_io[32];
    SIDDumpClass*   sid_dump;
    uint8_t         sid_dump_io;

    bool            cycle_excact_sid;
};
#endif // MAINWINDOW_H
