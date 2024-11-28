#pragma once

#include <vector>

/**
 * @brief 2D histogram class
*/
class Hist2D 
{
    public:
        Hist2D(int nBinsX, float xMin, float xMax, int nBinsY, float yMin, float yMax): m_nBinsX(nBinsX), m_xMin(xMin), m_xMax(xMax), m_nBinsY(nBinsY), m_yMin(yMin), m_yMax(yMax)
        {
            m_xBinWidth = (m_xMax - m_xMin) / m_nBinsX;
            m_yBinWidth = (m_yMax - m_yMin) / m_nBinsY;
            m_data.resize(m_nBinsX * m_nBinsY + 1, 0.); // +1 for overflow and underflow bin
        }
        Hist2D() = default;
        Hist2D(const Hist2D& other) = default;
        Hist2D& operator=(const Hist2D& other) = default;
        ~Hist2D() = default;

        int GetNBinsX() const { return m_nBinsX; }
        int GetNBinsY() const { return m_nBinsY; }
        /**
         * @brief Get the total number of bins. The last bin is the overflow bin.
        */
        int GetNBins() const { return m_nBinsX * m_nBinsY + 1; }
        int GetBinX(float x) const { return (x - m_xMin) / m_xBinWidth; }
        int GetBinY(float y) const { return (y - m_yMin) / m_yBinWidth; }
        int GetBin(float x, float y) const
        {
            int binX = GetBinX(x);
            int binY = GetBinY(y);
            return (binX > m_nBinsX || binY > m_nBinsY || binX < 0 || binY < 0) ? m_nBinsX * m_nBinsY : binX * m_nBinsY + binY;

        }
        float GetBinContent(int bin) const { return m_data[bin]; }
        float GetXmin() const { return m_xMin; }
        float GetXmax() const { return m_xMax; }
        float GetYmin() const { return m_yMin; }
        float GetYmax() const { return m_yMax; }
        std::vector<float> GetData() const { return m_data; }
        
        void Fill(float x, float y) { m_data[GetBin(x, y)]++; }
        bool IsUnderflow(float x, float y) const;

    private: 
        int m_nBinsX, m_nBinsY;
        float m_xMin, m_xMax, m_yMin, m_yMax;
        float m_xBinWidth, m_yBinWidth;
        std::vector<float> m_data;

};

bool Hist2D::IsUnderflow(float x, float y) const
{
    return (x < m_xMin || x > m_xMax || y < m_yMin || y > m_yMax);
}