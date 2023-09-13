#include "audiogenerator.h"
#include <QDebug>

AudioGenerator::AudioGenerator(const QAudioFormat &_format, QObject *parent = 0) : QIODevice(parent)
{

}

AudioGenerator::~AudioGenerator()
{

}

void AudioGenerator::start()
{
    open(QIODevice::ReadOnly);
}

void AudioGenerator::stop()
{
    close();
}

qint64 AudioGenerator::readData(char *data, qint64 len)
{
	emit FillAudioData(data, len);
	return len;
}

// Not used.
qint64 AudioGenerator::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return 0;
}

// Doesn't seem to be called by QAudioOutput.
qint64 AudioGenerator::bytesAvailable() const
{
    qDebug() << "bytesAvailable()";
    return m_buffer.size() + QIODevice::bytesAvailable();
}
