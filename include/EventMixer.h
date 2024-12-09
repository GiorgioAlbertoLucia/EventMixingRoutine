#pragma once

#include <iostream>
#include <vector>
#include <map> 
#include <variant>
#include <string>
#include <algorithm>
#include <numeric>
#include <mutex>
#include <cmath>

#include <yaml-cpp/yaml.h>
#include <TTree.h>
#include <TFile.h>
#include <TROOT.h>

#include "Hist2D.h"
#include "YamlUtils.h"
#include "TreeReader.h"
#include "Queue.h"
#include "Row.h"

namespace physics
{
    const float massProton = 0.938272;
    const float massHe3 = 2.80923;
}

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
        void BinMixingParallel(const int ibin);
        void SaveMixedBinTree(TFile * outputFile, const int ibin);
        void SaveMixedTree(TFile * outputFile, const char * treeName);
        void SaveMixedTree(const char * outputFileName) {};
        void Print();

    private:

        int m_nThreads;                                 // number of threads for parallel processing
        std::mutex m_mutex;                             // mutex for thread safety

        int m_bufferSize;                               // size of the buffer for the event mixing
        int m_nEvents;
        int m_maxMixSize;
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
    m_maxMixSize = config["MaxMixSize"].as<int>();

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
        if (ientry % 100000 == 0) std::cout << "Processing event: " << ientry << "/" << m_nEvents << "\r" << std::flush;
        inputTree->GetEntry(ientry);
        if (std::abs(inputRow.GetFloat("fNSigmaTPCHad")) > 2 || std::abs(inputRow.GetFloat("fNSigmaTPCHe3")) > 2) {
            continue;
        }
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
    std::vector<int> binPositionArray(m_inputArray.size()); // bin index of each event
    std::transform(m_inputArray.begin(), m_inputArray.end(), binPositionArray.begin(), [&](Row& row) {
        return m_binningHist.GetBin(row.GetFloat(m_binVariableX), row.GetFloat(m_binVariableY));
    });

    std::vector<std::pair<int, int>> binPositionIndexArray(m_inputArray.size()); // index and bin index of each event
    for (size_t i = 0; i < m_inputArray.size(); i++)
    {
        binPositionIndexArray[i] = std::make_pair(i, binPositionArray[i]);
    }

    std::sort(binPositionIndexArray.begin(), binPositionIndexArray.end(), [](std::pair<int, int>& a, std::pair<int, int>& b) {
        return a.second < b.second;
    });

    std::cout << "Filling sorted arrays" << std::endl;
    m_sortedArray.clear();
    m_sortedArray.reserve(binPositionIndexArray.size());
    for (auto& [index, bin]: binPositionIndexArray)
    {
        m_sortedArray.push_back(m_inputArray[index]);
        m_binningHist.Fill(m_inputArray[index].GetFloat(m_binVariableX), m_inputArray[index].GetFloat(m_binVariableY));
    }

    m_inputArray.clear();

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
 * @brief Mix the events in a given bin (not thread safe)
 * @param ibin Index of the bin
 */
void EventMixer::BinMixing(const int ibin)
{
    const int binStart = m_binIndex[ibin];
    //const int binEnd = m_binIndex[ibin] + 10; // checking purpose
    const int binEnd = m_binIndex[ibin + 1];

    const float massHe3 = physics::massHe3;
    const float massProton = physics::massProton;
    
    Queue<Row> queue(m_bufferSize);
    int currentlyMixed = 0;

    for (int ievent = binStart; ievent < binEnd; ievent++)
    {
        Row currentRow = m_sortedArray[ievent];
        Row mixedRow = currentRow;

        float energyHe3 = std::sqrt(massHe3 * massHe3 + 
                                            currentRow.GetFloat("fPtHe3") * std::cosh(currentRow.GetFloat("fEtaHe3")) * 
                                            currentRow.GetFloat("fPtHe3") * std::cosh(currentRow.GetFloat("fEtaHe3")));
       
        for (int i = 0; i < queue.GetSize(); i++)
        {
            auto rowToMix = queue.GetElement(i);
            if (currentRow[m_mixingExclusionVariable] == rowToMix[m_mixingExclusionVariable]) {
                continue;
            }

            float energyProton = std::sqrt(massProton * massProton + 
                                           rowToMix.GetFloat("fPtHad") * std::cosh(rowToMix.GetFloat("fEtaHad")) * 
                                           rowToMix.GetFloat("fPtHad") * std::cosh(rowToMix.GetFloat("fEtaHad")));              

            float px = currentRow.GetFloat("fPtHe3") * std::cos(currentRow.GetFloat("fPhiHe3")) + 
                       rowToMix.GetFloat("fPtHad") * std::cos(rowToMix.GetFloat("fPhiHad"));
            float py = currentRow.GetFloat("fPtHe3") * std::sin(currentRow.GetFloat("fPhiHe3")) + 
                       rowToMix.GetFloat("fPtHad") * std::sin(rowToMix.GetFloat("fPhiHad"));
            float pz = currentRow.GetFloat("fPtHe3") * std::sinh(currentRow.GetFloat("fEtaHe3")) + 
                       rowToMix.GetFloat("fPtHad") * std::sinh(rowToMix.GetFloat("fEtaHad"));               

            float invariantMass = std::sqrt((energyHe3 + energyProton) * (energyHe3 + energyProton) - 
                                            px * px - py * py - pz * pz);
            
            if (invariantMass > 4.15314) {
                continue;
            }

            for (auto & column: m_secondElementColumns) {
                mixedRow[column] = rowToMix[column];
            }

            //std::cout << std::endl << "mixedRow after mixing: " << std::endl;
            //mixedRow.Print();
            //std::cout << "invariant mass: " << invariantMass << std::endl;
            m_mixedArray.push_back(mixedRow);
            //std::cout << "mixedRow pushed" << std::endl;
            //m_mixedArray[m_mixedArray.size()-1].Print();
            currentlyMixed++;
            if (m_mixedArray.size() % 100000 == 0) {
                std::cout << "Mixed size: " << m_mixedArray.size() << "/" << m_maxMixSize << "\r" << std::flush;
            }
            if (m_mixedArray.size() >= m_maxMixSize) {
                return;
            }
        }

        queue.Fill(currentRow);
        
    }
    
    m_mixedBinIndex[ibin+1] = currentlyMixed + m_mixedBinIndex[ibin];
}

/**
 * @brief Mix the events in a given bin (thread safe)
 * @param ibin Index of the bin
 */
void EventMixer::BinMixingParallel(const int ibin)
{
    const int binStart = m_binIndex[ibin];
    const int binEnd = m_binIndex[ibin + 1];

    const float massHe3 = physics::massHe3;
    const float massProton = physics::massProton;
    
    Queue<Row> queue(m_bufferSize);
    int currentlyMixed = 0;

    for (int ievent = binStart; ievent < binEnd; ievent++)
    {
        Row currentRow = m_sortedArray[ievent];
        Row mixedRow = currentRow;
        queue.Fill(currentRow);

        /*
        float energyHe3 = std::sqrt(massHe3 * massHe3 + 
                                            currentRow.GetFloat("fPtHe3") * std::cosh(currentRow.GetFloat("fEtaHe3")) * 
                                            currentRow.GetFloat("fPtHe3") * std::cosh(currentRow.GetFloat("fEtaHe3")));
        */
       
        if (ievent == binStart) {
            continue;
        } else {
            for (int i = 0; i < queue.GetSize()-1; i++)
            {
                auto rowToMix = queue.GetElement(i);
                if (currentRow[m_mixingExclusionVariable] == rowToMix[m_mixingExclusionVariable]) {
                    continue;
                }

                /*
                float energyProton = std::sqrt(massProton * massProton + 
                                               rowToMix.GetFloat("fPtHad") * std::cosh(rowToMix.GetFloat("fEtaHad")) * 
                                               rowToMix.GetFloat("fPtHad") * std::cosh(rowToMix.GetFloat("fEtaHad")));              

                float px = currentRow.GetFloat("fPtHe3") * std::cos(currentRow.GetFloat("fPhiHe3")) + 
                           rowToMix.GetFloat("fPtHad") * std::cos(rowToMix.GetFloat("fPhiHad"));
                float py = currentRow.GetFloat("fPtHe3") * std::sin(currentRow.GetFloat("fPhiHe3")) + 
                           rowToMix.GetFloat("fPtHad") * std::sin(rowToMix.GetFloat("fPhiHad"));
                float pz = currentRow.GetFloat("fPtHe3") * std::sinh(currentRow.GetFloat("fEtaHe3")) + 
                           rowToMix.GetFloat("fPtHad") * std::sinh(rowToMix.GetFloat("fEtaHad"));               

                float invariantMass = std::sqrt((energyHe3 + energyProton) * (energyHe3 + energyProton) - 
                                                px * px - py * py - pz * pz);
                
                if (invariantMass > 4.15314) {
                    continue;
                }
                */
                for (auto & column: m_secondElementColumns) {
                    mixedRow[column] = rowToMix[column];
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex); // Lock the mutex
                    m_mixedArray.push_back(mixedRow);
                    currentlyMixed++;
                    if (m_mixedArray.size() % 100000 == 0) {
                        std::cout << "Mixed size: " << m_mixedArray.size() << "/" << m_maxMixSize << "\r" << std::flush;
                    }
                    if (m_mixedArray.size() >= m_maxMixSize) {
                        return;
                    }
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex); // Lock the mutex
        m_mixedBinIndex[ibin+1] = currentlyMixed + m_mixedBinIndex[ibin];
    }
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
    //ROOT::EnableImplicitMT(m_nThreads);

    std::cout << "Freeing sorted array" << std::endl;
    m_sortedArray.clear();

    outputFile->cd();
    TTree * outputTree = new TTree(treeName, treeName);

    Row mixedRow;
    mixedRow.InitRowFromDict(m_columnDict);
    mixedRow.CreateBranchesFromDict(outputTree, m_columnDict);

    std::cout << "Saving mixed tree" << std::endl;
    // checking purpose
    const float massHe3 = physics::massHe3;
    const float massProton = physics::massProton;
    //

    for (auto& row: m_mixedArray)
    {
        mixedRow = row;
        
        float energyHe3 = std::sqrt(massHe3 * massHe3 + 
                                            mixedRow.GetFloat("fPtHe3") * std::cosh(mixedRow.GetFloat("fEtaHe3")) * 
                                            mixedRow.GetFloat("fPtHe3") * std::cosh(mixedRow.GetFloat("fEtaHe3")));

        float energyProton = std::sqrt(massProton * massProton + 
                                       mixedRow.GetFloat("fPtHad") * std::cosh(mixedRow.GetFloat("fEtaHad")) * 
                                       mixedRow.GetFloat("fPtHad") * std::cosh(mixedRow.GetFloat("fEtaHad")));              

        float px = mixedRow.GetFloat("fPtHe3") * std::cos(mixedRow.GetFloat("fPhiHe3")) + 
                   mixedRow.GetFloat("fPtHad") * std::cos(mixedRow.GetFloat("fPhiHad"));
        float py = mixedRow.GetFloat("fPtHe3") * std::sin(mixedRow.GetFloat("fPhiHe3")) + 
                   mixedRow.GetFloat("fPtHad") * std::sin(mixedRow.GetFloat("fPhiHad"));
        float pz = mixedRow.GetFloat("fPtHe3") * std::sinh(mixedRow.GetFloat("fEtaHe3")) + 
                   mixedRow.GetFloat("fPtHad") * std::sinh(mixedRow.GetFloat("fEtaHad"));               

        float invariantMass = std::sqrt((energyHe3 + energyProton) * (energyHe3 + energyProton) - 
                                        px * px - py * py - pz * pz);
        
        if (invariantMass > 4.15314) {
            std::cout << "input row: " << std::endl;
            mixedRow.Print();
            std::cout << "mixed row: " << std::endl;
            mixedRow.Print();
        }
        outputTree->Fill();
    }
    outputFile->cd();
    outputTree->Write();

    //ROOT::DisableImplicitMT();
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