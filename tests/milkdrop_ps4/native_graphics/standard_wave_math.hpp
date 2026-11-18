#pragma once
#include <array>
#include <cmath>
#include <cassert>
// Vertex math adapted from pinned projectM 4.1.8 Waveforms sources (LGPL).
// Fixed storage, no frame allocations; original sources used as host references.
struct StandardWaveMath {
 struct Point{float x=0,y=0;};
 struct Points{std::array<Point,480> data{};unsigned count=0;void resize(unsigned n){assert(n<=480);count=n;}Point &operator[](unsigned n){return data[n];}};
 const float *m_pcmDataL=nullptr,*m_pcmDataR=nullptr;
 float m_aspectX=1,m_aspectY=.5625f,m_waveX=0,m_waveY=0,m_mysteryWaveParam=0;
 int m_samples=0,m_sampleOffset=0;float m_edgeX=0,m_edgeY=0,m_distanceX=0,m_distanceY=0,m_perpetualDX=0,m_perpetualDY=0;
 Points m_wave1Vertices,m_wave2Vertices;
 void clip(float angle){
    m_distanceX = cosf(angle);
    m_distanceY = sinf(angle);

    std::array<float, 2> edgeX{
        m_waveX * cosf(angle + 1.57f) - m_distanceX * 3.0f,
        m_waveX * cosf(angle + 1.57f) + m_distanceX * 3.0f};

    std::array<float, 2> edgeY{
        m_waveX * sinf(angle + 1.57f) - m_distanceY * 3.0f,
        m_waveX * sinf(angle + 1.57f) + m_distanceY * 3.0f};

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            float t{0.0};
            bool clip{false};

            switch (j)
            {
                case 0:
                    if (edgeX[i] > 1.1f)
                    {
                        t = (1.1f - edgeX[1 - i]) / (edgeX[i] - edgeX[1 - i]);
                        clip = true;
                    }
                    break;

                case 1:
                    if (edgeX[i] < -1.1f)
                    {
                        t = (-1.1f - edgeX[1 - i]) / (edgeX[i] - edgeX[1 - i]);
                        clip = true;
                    }
                    break;

                case 2:
                    if (edgeY[i] > 1.1f)
                    {
                        t = (1.1f - edgeY[1 - i]) / (edgeY[i] - edgeY[1 - i]);
                        clip = true;
                    }
                    break;

                case 3:
                    if (edgeY[i] < -1.1f)
                    {
                        t = (-1.1f - edgeY[1 - i]) / (edgeY[i] - edgeY[1 - i]);
                        clip = true;
                    }
                    break;
            }

            if (clip)
            {
                const float diffX = edgeX[i] - edgeX[1 - i];
                const float diffY = edgeY[i] - edgeY[1 - i];
                edgeX[i] = edgeX[1 - i] + diffX * t;
                edgeY[i] = edgeY[1 - i] + diffY * t;
            }
        }
    }

    m_sampleOffset = (480 - m_samples) / 2;

    m_distanceX = (edgeX[1] - edgeX[0]) / static_cast<float>(m_samples);
    m_distanceY = (edgeY[1] - edgeY[0]) / static_cast<float>(m_samples);

    m_edgeX = edgeX[0];
    m_edgeY = edgeY[0];

    const float angle2 = atan2f(m_distanceY, m_distanceX);
    m_perpetualDX = cosf(angle2 + 1.57f);
    m_perpetualDY = sinf(angle2 + 1.57f);
}
 void circle(double seconds){
    m_samples = 480 / 2;

    m_wave1Vertices.resize(m_samples);

    const int sampleOffset{(480 - m_samples) / 2};

    const float inverseSamplesMinusOne{1.0f / static_cast<float>(m_samples)};

    for (int i = 0; i < m_samples; i++)
    {
        float radius = 0.5f + 0.4f * m_pcmDataR[i + sampleOffset] + m_mysteryWaveParam;
        float const angle = static_cast<float>(i) * inverseSamplesMinusOne * 6.28f + seconds * 0.2f;
        if (i < m_samples / 10)
        {
            float mix = static_cast<float>(i) / (static_cast<float>(m_samples) * 0.1f);
            mix = 0.5f - 0.5f * cosf(mix * 3.1416f);
            float const radius2 = 0.5f + 0.4f * m_pcmDataR[i + m_samples + sampleOffset] + m_mysteryWaveParam;
            radius = radius2 * (1.0f - mix) + radius * (mix);
        }

        m_wave1Vertices[i].x = radius * cosf(angle) * m_aspectY + m_waveX;
        m_wave1Vertices[i].y = radius * sinf(angle) * m_aspectX + m_waveY;
    }
}
 void spiral(double seconds){
    m_samples = 480 / 2;

    m_wave1Vertices.resize(m_samples);

    for (int i = 0; i < m_samples; i++)
    {
        float const radius = (0.53f + 0.43f * m_pcmDataR[i] + m_mysteryWaveParam);
        float const angle = m_pcmDataL[i + 32] * 1.57f + seconds * 2.3f;

        m_wave1Vertices[i].x = radius * cosf(angle) * m_aspectY + m_waveX;
        m_wave1Vertices[i].y = radius * sinf(angle) * m_aspectX + m_waveY;
    }
}
 void centered(double seconds){
    // Alpha calculation is handled in MaximizeColors().
    m_samples = 480;

    m_wave1Vertices.resize(m_samples);

    for (int i = 0; i < m_samples; i++)
    {
        m_wave1Vertices[i].x = m_pcmDataR[i] * m_aspectY + m_waveX;
        m_wave1Vertices[i].y = m_pcmDataL[i + 32] * m_aspectX + m_waveY;
    }
}
 void derivative(double seconds){
    m_samples = 480;

    if (m_samples > 512 / 3)
    {
        m_samples /= 3;
    }

    m_wave1Vertices.resize(m_samples);

    int const sampleOffset = (480 - m_samples) / 2;

    const float w1 = 0.45f + 0.5f * (m_mysteryWaveParam * 0.5f + 0.5f);
    const float w2 = 1.0f - w1;

    const float inverseSamples = 1.0f / static_cast<float>(m_samples);

    for (int i = 0; i < m_samples; i++)
    {
        assert((i + 25 + sampleOffset) < 512);
        m_wave1Vertices[i].x = -1.0f + 2.0f * (static_cast<float>(i) * inverseSamples) + m_waveX;
        m_wave1Vertices[i].y = m_pcmDataL[i + sampleOffset] * 0.47f + m_waveY;
        m_wave1Vertices[i].x += m_pcmDataR[i + 25 + sampleOffset] * 0.44f;

        // Momentum
        if (i > 1)
        {
            m_wave1Vertices[i].x = m_wave1Vertices[i].x * w2 + w1 * (m_wave1Vertices[i - 1].x * 2.0f - m_wave1Vertices[i - 2].x);
            m_wave1Vertices[i].y = m_wave1Vertices[i].y * w2 + w1 * (m_wave1Vertices[i - 1].y * 2.0f - m_wave1Vertices[i - 2].y);
        }
    }
}
 void explosive(double seconds){
    m_samples = 480;

    m_wave1Vertices.resize(m_samples);

    const float cosineRotation = cosf(seconds * 0.3f);
    const float sineRotation = sinf(seconds * 0.3f);

    for (int i = 0; i < m_samples; i++)
    {
        const float x0 = (m_pcmDataR[i] * m_pcmDataL[i + 32] + m_pcmDataL[i] * m_pcmDataR[i + 32]);
        const float y0 = (m_pcmDataR[i] * m_pcmDataR[i] - m_pcmDataL[i + 32] * m_pcmDataL[i + 32]);
        m_wave1Vertices[i].x = ((x0 * cosineRotation - y0 * sineRotation) * m_aspectY) + m_waveX;
        m_wave1Vertices[i].y = ((x0 * sineRotation + y0 * cosineRotation) * m_aspectX) + m_waveY;
    }
}
 void line(double seconds){
    m_samples = 480 / 2;

    if (m_samples > 512 / 3)
    {
        m_samples /= 3;
    }

    m_wave1Vertices.resize(m_samples);

    clip(1.57f * m_mysteryWaveParam);

    for (int i = 0; i < m_samples; i++)
    {
        m_wave1Vertices[i].x = m_edgeX + m_distanceX * static_cast<float>(i) + m_perpetualDX * 0.25f * m_pcmDataL[i + m_sampleOffset];
        m_wave1Vertices[i].y = m_edgeY + m_distanceY * static_cast<float>(i) + m_perpetualDY * 0.25f * m_pcmDataL[i + m_sampleOffset];
    }
}
 void doubleLine(double seconds){
    m_samples = 480 / 2;

    if (m_samples > 512 / 3)
    {
        m_samples /= 3;
    }

    m_wave1Vertices.resize(m_samples);
    m_wave2Vertices.resize(m_samples);

    clip(1.57f * m_mysteryWaveParam);

    float const separation = powf(m_waveY * 0.5f + 0.5f, 2.0f);
    for (int i = 0; i < m_samples; i++)
    {
        m_wave1Vertices[i].x = m_edgeX + m_distanceX * static_cast<float>(i) +
                               m_perpetualDX * (0.25f * m_pcmDataL[i + m_sampleOffset] + separation);
        m_wave1Vertices[i].y = m_edgeY + m_distanceY * static_cast<float>(i) +
                               m_perpetualDY * (0.25f * m_pcmDataL[i + m_sampleOffset] + separation);

        m_wave2Vertices[i].x = m_edgeX + m_distanceX * static_cast<float>(i) +
                               m_perpetualDX * (0.25f * m_pcmDataR[i + m_sampleOffset] - separation);
        m_wave2Vertices[i].y = m_edgeY + m_distanceY * static_cast<float>(i) +
                               m_perpetualDY * (0.25f * m_pcmDataR[i + m_sampleOffset] - separation);
    }
}
 bool generate(int mode,const float *left,const float *right,float x,float y,float mystery,float seconds){
  m_pcmDataL=left;m_pcmDataR=right;m_waveX=x;m_waveY=y;m_mysteryWaveParam=mystery;m_wave1Vertices.count=m_wave2Vertices.count=0;
  if((mode==0 || mode==1 || mode==4) && (mystery<-1 || mystery>1)){m_mysteryWaveParam=mystery*.5f+.5f;m_mysteryWaveParam-=floorf(m_mysteryWaveParam);m_mysteryWaveParam=fabsf(m_mysteryWaveParam)*2-1;}
  switch(mode){case 0:circle(seconds);break;case 1:spiral(seconds);break;case 2:case 3:centered(seconds);break;case 4:derivative(seconds);break;case 5:explosive(seconds);break;case 6:line(seconds);break;case 7:doubleLine(seconds);break;default:return false;}
  return m_samples>1 && m_samples<=480;
 }
};
