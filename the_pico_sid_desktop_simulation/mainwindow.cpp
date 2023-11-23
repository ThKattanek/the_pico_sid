#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <math.h>

#include "../firmware/version.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    bufferSize = SOUND_BUFFER_SIZE * 2;

    cycle_excact_sid = true;

    this->setWindowTitle("ThePicoSID Desktop Simulation");

    ui->osc1->setMinimumSize(200,200);
    ui->osc2->setMinimumSize(200,200);
    ui->osc3->setMinimumSize(200,200);

    ui->osc1->SetAmplifire(3);
    ui->osc1->SetSamplerate(44100);
    ui->osc1->SetTriggerLevel(1.5f);
    ui->osc1->SetTriggerTyp(RISING_EDGE);

    ui->osc2->SetAmplifire(3);
    ui->osc2->SetSamplerate(44100);
    ui->osc2->SetTriggerLevel(1.5f);
    ui->osc2->SetTriggerTyp(RISING_EDGE);

    ui->osc3->SetAmplifire(3);
    ui->osc3->SetSamplerate(44100);
    ui->osc3->SetTriggerLevel(1.5f);
    ui->osc3->SetTriggerTyp(RISING_EDGE);

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

        sid.SetSidType(MOS_6581);
        sid_dump = new SIDDumpClass(&sid_dump_io);

        m_audiogen->start();
		m_audioOutput->start(m_audiogen);
        m_audioOutput->setVolume(1);
    }

    ui->fw_version->setText("Firmware Version: " + QString::number(VERSION_MAJOR) + "." + QString::number(VERSION_MINOR) + "." + QString::number(VERSION_PATCH));
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

    static unsigned short wave0_buffer[SOUND_BUFFER_SIZE];
    static unsigned short wave1_buffer[SOUND_BUFFER_SIZE];
    static unsigned short wave2_buffer[SOUND_BUFFER_SIZE];

    int wave_pos = 0;

    while(buffer_pos < (len / (m_format.sampleSize()/8)))
    {
        if(!cycle_excact_sid)
        {
            for(int i=0; i<6; i++)
            {
                if(sid_dump->CycleTickPlay()) sid.WriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                if(sid_dump->CycleTickPlay()) sid.WriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                if(sid_dump->CycleTickPlay()) sid.WriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                if(sid_dump->CycleTickPlay()) sid.WriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                sid.Clock(4);
            }
        }
        else
        {
            for(int i=0; i<24; i++)
            {
                if(sid_dump->CycleTickPlay()) sid.WriteReg(sid_dump->RegOut, sid_dump->RegWertOut);
                sid.Clock();
            }
        }

        //buffer[buffer_pos] = buffer[buffer_pos+1] = ((~(sid.AudioOut() >> 4)+1) / (float)0xffff * 0x7ff) / float(0x7ff);
        buffer[buffer_pos] = buffer[buffer_pos+1] = ((signed short)(~sid.AudioOut()+1) / (float)0xffff) * 2;


        // Osc Waves Visualisieren
        wave0_buffer[wave_pos] = sid.voice[0].wave.Output();
        wave1_buffer[wave_pos] = sid.voice[1].wave.Output();
        wave2_buffer[wave_pos] = sid.voice[2].wave.Output();
        wave_pos++;

        buffer_pos += 2;
    }

    ui->osc1->NextAudioData((unsigned char*)wave0_buffer, wave_pos, 12, false);
    ui->osc2->NextAudioData((unsigned char*)wave1_buffer, wave_pos, 12, false);
    ui->osc3->NextAudioData((unsigned char*)wave2_buffer, wave_pos, 12, false);
}

void MainWindow::on_Quit_clicked()
{
    this->close();
}


void MainWindow::on_LoadSidDump_clicked()
{
    sid_dump->StopDump();

    QString filename = QFileDialog::getOpenFileName(this,tr("Emu64 SID Dump öffnen "),QDir::homePath()+"/Elektronik/Projekte/the_pico_sid/the_pico_sid_desktop_simulation/sid_dump_demos",tr("Emu64 SID Dump Datei") + "(*.sdp);;" + tr("Alle Dateien") + "(*.*)");
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
    sid.SetSidType(MOS_6581);
}


void MainWindow::on_mos8580_clicked()
{
    sid.SetSidType(MOS_8580);
}

