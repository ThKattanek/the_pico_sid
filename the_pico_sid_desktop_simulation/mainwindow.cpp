#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <math.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    bufferSize = SOUND_BUFFER_SIZE * 2;

    cycle_excact_sid = false;

    this->setWindowTitle("ThePicoSID Desktop Simulation");

    // Default Audioformat (44100 / Stereo / Float)
    m_format.setSampleRate(41223);
    m_format.setChannelCount(2);
    m_format.setSampleSize(32);
    m_format.setCodec("audio/pcm");
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setSampleType(QAudioFormat::SampleType::Float);

    bool is_supported_format = false;

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(m_format))
    {
        qWarning() << "Default format not supported (44100/Stereo/Float)";

        // Second Audioformat (44100/Stereo/Signed Int 16Bit)
        m_format.setSampleSize(16);
        m_format.setSampleType(QAudioFormat::SampleType::SignedInt);
        if (!info.isFormatSupported(m_format))
        {
            qWarning() << "Second format not supported (44100/Stereo/Signed Int 16Bit)";
        }
        else
        {
             qInfo() << "Current Audioformat: 44100/Stereo/Signed Int 16Bit";
             is_supported_format = true;
        }
    }
    else{
        qInfo() << "Current Audioformat: 41223/Stereo/Float";
        is_supported_format = true;
    }

    if(is_supported_format)
    {
        m_device = QAudioDeviceInfo::defaultOutputDevice();
        m_buffer = QByteArray(bufferSize*2, 0);

        m_audioOutput = new QAudioOutput(m_device, m_format, this);
        m_audioOutput->setBufferSize(bufferSize);
        m_audiogen = new AudioGenerator(m_format, this);

		connect(m_audiogen, SIGNAL(FillAudioData(char*, qint64)), this, SLOT(OnFillAudioData(char*, qint64)));

        SidInit(sid_io);
        SidSetChipTyp(MOS_8580);
        sid_dump = new SIDDumpClass(&sid_dump_io);

        m_audiogen->start();
		m_audioOutput->start(m_audiogen);
        m_audioOutput->setVolume(1);
    }
}

MainWindow::~MainWindow()
{
	disconnect(m_audiogen, SIGNAL(FillAudioData(char*, qint64)), this, SLOT(OnFillAudioData(char*, qint64)));
    m_audioOutput->stop();
    delete m_audioOutput;

    delete ui;
}

void MainWindow::OnFillAudioData(char *data, qint64 len)
{
    float* buffer = reinterpret_cast<float*>(data);
    int buffer_pos = 0;

    while(buffer_pos < (len / (m_format.sampleSize()/8)))
    {
        if(!cycle_excact_sid)
        {
            for(int i=0; i<6; i++)
            {
                if(sid_dump->CycleTickPlay()) SidWriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                if(sid_dump->CycleTickPlay()) SidWriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                if(sid_dump->CycleTickPlay()) SidWriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                if(sid_dump->CycleTickPlay()) SidWriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                SidCycle(4);
            }
        }
        else
        {
            for(int i=0; i<24; i++)
            {
                if(sid_dump->CycleTickPlay()) SidWriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                SidCycle(1);
            }
        }
        buffer[buffer_pos] = buffer[buffer_pos+1] = ((~(SidFilterOut() >> 4)+1) / (float)0xffff * 0x7ff) / float(0x7ff);

        buffer_pos += 2;
    }
}

void MainWindow::on_Quit_clicked()
{
    this->close();
}


void MainWindow::on_LoadSidDump_clicked()
{
    sid_dump->StopDump();

    QString filename = QFileDialog::getOpenFileName(this,tr("Emu64 SID Dump öffnen "),"",tr("Emu64 SID Dump Datei") + "(*.sdp);;" + tr("Alle Dateien") + "(*.*)");
    if(filename != "")
    {
        if(!sid_dump->LoadDump(filename.toLocal8Bit().data()))
            QMessageBox::warning(this,"realSID Error !","Fehler beim öffnen des SID Dump Files.");
        else
            sid_dump->PlayDump();
    }
}


void MainWindow::on_CycleExcact_toggled(bool checked)
{
    cycle_excact_sid = checked;
}


void MainWindow::on_mos6581_clicked()
{
    SidSetChipTyp(MOS_6581);
}


void MainWindow::on_mos8580_clicked()
{
    SidSetChipTyp(MOS_8580);
}

