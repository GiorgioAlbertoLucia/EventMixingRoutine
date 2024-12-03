#pragma once

#include <iostream>
#include <vector>
#include <map> 
#include <variant>
#include <string>
#include <algorithm>
#include <numeric>
#include <mutex>

#include <yaml-cpp/yaml.h>
#include <TTree.h>
#include <TFile.h>
#include <TROOT.h>

#include "Hist2D.h"
#include "YamlUtils.h"
#include "TreeReader.h"
#include "Queue.h"
#include "Row.h"

using ColumnValue = std::variant<Char_t, UChar_t, Short_t, UShort_t, Int_t, UInt_t, Long64_t, ULong64_t, Float_t, Double_t, bool, std::string>;
using RowType = std::map<std::string, ColumnValue>;

class EventMixer
{
    public: 
        EventMixer(TTree* inputTree, const char* configFileName);
        
        int GetNEvents() const { return m_nEvents; }
        int GetNBins() const { return m_binningHist.GetNBins(); }
        int GetNThreads() const { return m_nThreads; }
        void CleanUnderflow();
        void Sorting();
        void BinMixing(const int ibin);
        void SaveMixedBinTree(TFile * outputFile, const int ibin);
        void SaveMixedTree(TFile * outputFile, const char * treeName);
        void SaveMixedTree(const char * outputFileName) {};
        void Print();

    private:

        int m_nThreads;                                 // number of threads for parallel processing
        std::mutex m_mutex;                             // mutex for thread safety

        int m_bufferSize;                               // size of the buffer for the event mixing
        int m_nEvents;
        std::map<std::string, size_t> m_columnTypeCache;// cache the types of the columns in a row
        std::vector<std::string> m_columnDict;          // dictionary of columns to be read from the input tree
        std::vector<std::string> m_columns;             // list of columns to be read from the input tree

        std::vector<Row> m_inputArray;
        std::vector<Row> m_sortedArray;
        std::vector<Row> m_mixedArray;

        std::vector<int> m_sortedArrayIndex;            // map to the original index of the sorted array
        Hist2D m_binningHist;                           // histogram with binning. Will be used to store the first position of the bin in the sorted array
        std::vector<int> m_binIndex;                    // index of the first event in the bin
        std::vector<int> m_mixedBinIndex;               // size of the bin

        std::string m_binVariableX, m_binVariableY;     // name of the variables used for the binning
        std::string m_mixingExclusionVariable;          // name of the variable used to exclude pairs from mixing
        std::vector<std::string> m_secondElementColumns;// columns of the second element to be mixed
        
};

EventMixer::EventMixer(TTree* inputTree, const char* configFileName)
{
    // Read the configuration file
    YAML::Node config = YAML::LoadFile(configFileName);

    m_nThreads = config["NThreads"].as<int>();
    m_bufferSize = config["BufferSize"].as<int>();

    const int nbinsx = config["NbinsX"].as<int>();
    const int nbinsy = config["NbinsY"].as<int>();
    const float xmin = config["Xmin"].as<float>();
    const float xmax = config["Xmax"].as<float>();
    const float ymin = config["Ymin"].as<float>();
    const float ymax = config["Ymax"].as<float>();
    m_binningHist = Hist2D(nbinsx, xmin, xmax, nbinsy, ymin, ymax);
    m_binVariableX = config["BinVariableX"].as<std::string>();
    m_binVariableY = config["BinVariableY"].as<std::string>();
    m_mixingExclusionVariable = config["MixingExclusionVariable"].as<std::string>();

    // Prepare to read from the input tree
    YamlUtils::ReadYamlVector(config["ColumnDict"], m_columnDict);
    YamlUtils::ReadYamlVector(config["Columns"], m_columns);
    YamlUtils::ReadYamlVector(config["SecondElementColumns"], m_secondElementColumns);  
    
    Row inputRow;
    inputRow.InitRowFromDict(m_columnDict);
    inputRow.SetBranchAddressesFromDict(inputTree, m_columnDict);

    ROOT::EnableImplicitMT(m_nThreads);

    m_nEvents = inputTree->GetEntries();
    const int underflowBin = m_binningHist.GetNBins();
    m_inputArray.reserve(m_nEvents);
    int filteredSize = 0;
    for (int ientry = 0; ientry < m_nEvents; ientry++)
    {
        if (ientry % 100000 == 0) std::cout << "Processing event: " << ientry << "/" << m_nEvents << "\r";
        inputTree->GetEntry(ientry);
        if (!m_binningHist.IsUnderflow(inputRow.GetFloat(m_binVariableX), inputRow.GetFloat(m_binVariableY)))
        {
            m_inputArray.push_back(inputRow);
            filteredSize++;
        }
    }
    std::cout << std::endl;
    m_nEvents = filteredSize;

    ROOT::DisableImplicitMT();
}

void EventMixer::CleanUnderflow()
{
    /*
    float xMin = m_binningHist.GetXmin();
    float xMax = m_binningHist.GetXmax();
    float yMin = m_binningHist.GetYmin();
    float yMax = m_binningHist.GetYmax();

    auto getFloatValue = [](const ColumnValue& value) -> float {
        return std::visit([](auto&& arg) -> float {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<float>(arg);
            } else {
                throw std::runtime_error("Non-arithmetic type in variant");
            }
        }, value);
    };

    auto isUnderflow = [&](const Row& row) {
        float x = getFloatValue(m_GetColumnValue(row, m_binVariableX));
        float y = getFloatValue(m_GetColumnValue(row, m_binVariableY));
        if (x < xMin || x > xMax || y < yMin || y > yMax)
        {
            std::cout << "Removing event with x = " << x << " and y = " << y << std::endl;
            return true;
        }
        return false;
    };

    m_inputArray.erase(std::remove_if(m_inputArray.begin(), m_inputArray.end(), isUnderflow), m_inputArray.end());
    */
}

/**
 * @brief Sort the input array according to the binning variables
 */
void EventMixer::Sorting()
{
    std::cout << "Sorting" << std::endl;
    m_sortedArray = m_inputArray;
    std::iota(m_sortedArrayIndex.begin(), m_sortedArrayIndex.end(), 0); // Initialize indices

    std::sort(m_sortedArray.begin(), m_sortedArray.end(), [&](Row& a, Row& b) {
        return m_binningHist.GetBin(a.GetFloat(m_binVariableX), a.GetFloat(m_binVariableY)) < 
                m_binningHist.GetBin(b.GetFloat(m_binVariableX), b.GetFloat(m_binVariableY));
    });

    std::sort(m_sortedArrayIndex.begin(), m_sortedArrayIndex.end(), [&](int a, int b) {
        return m_binningHist.GetBin(m_sortedArray[a].GetFloat(m_binVariableX), m_sortedArray[a].GetFloat(m_binVariableY)) < 
                m_binningHist.GetBin(m_sortedArray[b].GetFloat(m_binVariableX), m_sortedArray[b].GetFloat(m_binVariableY));
    });

    for (auto& row: m_sortedArray)
    {
        m_binningHist.Fill(row.GetFloat(m_binVariableX), row.GetFloat(m_binVariableY));
    }
    
    m_binIndex.resize(m_binningHist.GetNBins(), 0);
    std::vector<float> binData = m_binningHist.GetData();
    for (int bin = 0; bin < m_binningHist.GetNBins(); bin++)
    {
        m_binIndex[bin] = static_cast<int>(binData[bin]);
    }
    // make binIndex store the first index of the bin in the sorted array
    std::exclusive_scan(m_binIndex.begin(), m_binIndex.end(), m_binIndex.begin(), 0);
    m_mixedBinIndex.resize(m_binIndex.size(), 0);

}

/**
 * @brief Mix the events in a given bin
 * @param ibin Index of the bin
 */
void EventMixer::BinMixing(const int ibin)
{
    const int binStart = m_binIndex[ibin];
    const int binEnd = m_binIndex[ibin + 1];
    
    Queue<Row> queue(m_bufferSize);
    int currentlyMixed = 0;

    for (int ievent = binStart; ievent < binEnd; ievent++)
    {
        Row currentRow = m_sortedArray[ievent];
        Row mixedRow = currentRow;
        queue.Fill(currentRow);
        if (ievent == binStart) {
            continue;
        } else {
            for (int i = 0; i < queue.GetSize()-1; i++)
            {
                std::cout << "Mixing: " << i << std::endl;
                auto rowToMix = queue.GetElement(i);
                if (currentRow[m_mixingExclusionVariable] == rowToMix[m_mixingExclusionVariable]) {
                    continue;
                }
                for (auto & column: m_secondElementColumns) {
                    mixedRow[column] = rowToMix[column];
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex); // Lock the mutex
                    m_mixedArray.push_back(mixedRow);
                    currentlyMixed++;
                    std::cout << "Mixed: " << currentlyMixed << std::endl;
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex); // Lock the mutex
        m_mixedBinIndex[ibin+1] = currentlyMixed + m_mixedBinIndex[ibin];
    }
    std::cout << "size of mixed array: " << m_mixedArray.size() << std::endl;
}

/**
 * @brief Save the mixed events in a given bin to a TFile.
 * DEPRECATED!!! the parallel version for bin mixing does not ensure the order of the events!
 * @param outputFile Output file
 * @param ibin Index of the bin
 */
void EventMixer::SaveMixedBinTree(TFile * outputFile, const int ibin)
{
    //ROOT::EnableImplicitMT(m_nThreads);
//
    //const int binStart = m_mixedBinIndex[ibin];
    //const int binEnd = m_mixedBinIndex[ibin + 1];
//
    //std::string treeName = "MixedBin" + std::to_string(ibin);
    //outputFile->cd();
    //TTree * outputTree = new TTree(treeName.c_str(), treeName.c_str());
//
    //Row mixedRow;
    //TreeDict::InitRowFromDict(m_columnDict, mixedRow);
    //TreeDict::CreateBranchesFromDict(outputTree, m_columnDict, mixedRow);
//
    //for (int ievent = binStart; ievent < binEnd; ievent++)
    //{
    //    // This does not work: make Row a class with a copy constructor
    //    //mixedRow = m_mixedArray[ievent];
    //    for (auto column: m_columns)
    //    {
    //        mixedRow[column] = m_mixedArray[ievent][column];
    //    }
    //    outputTree->Fill();
    //}
    //outputFile->cd();
    //outputTree->Write();
//
    //ROOT::DisableImplicitMT();
}

void EventMixer::SaveMixedTree(TFile * outputFile, const char * treeName = "MixedTree")
{
    ROOT::EnableImplicitMT(m_nThreads);

    outputFile->cd();
    TTree * outputTree = new TTree(treeName, treeName);

    Row mixedRow;
    mixedRow.InitRowFromDict(m_columnDict);
    mixedRow.CreateBranchesFromDict(outputTree, m_columnDict);

    for (auto& row: m_mixedArray)
    {
        mixedRow = row;
        outputTree->Fill();
    }
    outputFile->cd();
    outputTree->Write();

    ROOT::DisableImplicitMT();
}

void EventMixer::Print()
{
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "\t\tEventMixer" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Number of events: " << m_nEvents << std::endl;
    std::cout << "Number of bins: " << GetNBins() << std::endl;
    std::cout << "Buffer size: " << m_bufferSize << std::endl;
    std::cout << "Number of threads: " << m_nThreads << std::endl;
    std::cout << "Binning variables: " << m_binVariableX << ", " << m_binVariableY << std::endl;
    std::cout << "X Binning: " << m_binningHist.GetNBinsX() << " bins, in [" << m_binningHist.GetXmin() << ", " << m_binningHist.GetXmax() << "]" << std::endl;
    std::cout << "Y Binning: " << m_binningHist.GetNBinsY() << " bins, in [" << m_binningHist.GetYmin() << ", " << m_binningHist.GetYmax() << "]" << std::endl;
    std::cout << "Mixing exclusion variable: " << m_mixingExclusionVariable << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << std::endl;
}