#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>

class SphereController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal level READ level WRITE setLevel NOTIFY levelChanged)
    Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(QVariantList spectrumLevels READ spectrumLevels NOTIFY spectrumLevelsChanged)
public:
    enum State { Loading, Listening, Paused, Transcribe };
    Q_ENUM(State)

    explicit SphereController(QObject *parent = nullptr) : QObject(parent) {}

    qreal level() const { return m_level; }
    void setLevel(qreal v) {
        v = qBound(0.0, v, 1.0);
        if (qFuzzyCompare(m_level, v)) return;
        m_level = v;
        emit levelChanged();
    }

    State state() const { return m_state; }
    void setState(State s) {
        if (m_state == s) return;
        m_state = s;
        emit stateChanged();
    }

    QVariantList spectrumLevels() const { return m_spectrumLevels; }
    void setSpectrumLevels(const QVector<float>& bands) {
        QVariantList list;
        list.reserve(bands.size());
        for (float v : bands) list.append(v);
        m_spectrumLevels = list;
        emit spectrumLevelsChanged();
    }

signals:
    void levelChanged();
    void stateChanged();
    void sphereClicked(); 
    void spectrumLevelsChanged();

private:
    QVariantList m_spectrumLevels;
    qreal m_level = 0.0;
    State m_state = Loading;
};