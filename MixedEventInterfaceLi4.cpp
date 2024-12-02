#include <iostream>
#include <vector>
#include <string>
#include <future>
#include <algorithm>

#include <TFile.h>
#include <TTree.h>

#include "include/TreeManager.h"
#include "include/EventMixer.h" 

void MixedEventInterfaceLi4(const char * configFileName) {
    
    std::cout << "MixedEventInterface" << std::endl;

    YAML::Node config = YAML::LoadFile(configFileName);
    std::string inputTreeFile = config["InputTreeFile"].as<std::string>();
    std::vector<std::string> treeNames;
    YamlUtils::ReadYamlVector(config["TreeNames"], treeNames);
    std::string inputTreeMergeFile = config["InputTreeMergeFile"].as<std::string>();
    std::string inputTreeHMergeFile = config["InputTreeHMergeFile"].as<std::string>();

    bool doMerge = config["DoMerge"].as<bool>();
    if (doMerge) {
        std::cout << "MergeAllTrees" << std::endl;
        MergeAllTrees(inputTreeFile.c_str(), treeNames, inputTreeMergeFile.c_str());

        std::cout << std::endl;
        std::vector<std::vector<std::string>> columnDicts;
        for (const auto & treeName : treeNames) {
            std::vector<std::string> columnDict;
            YamlUtils::ReadYamlVector(config[treeName+"Dict"], columnDict);
            columnDicts.push_back(columnDict);
        }
        std::vector<std::string> columnDictFull;
        YamlUtils::ReadYamlVector(config["ColumnDict"], columnDictFull);

        HorizontalMerge(inputTreeMergeFile.c_str(), treeNames, inputTreeHMergeFile.c_str(), 
                        columnDicts, columnDictFull);
    }
    TFile * inputHMergeFile = TFile::Open(inputTreeHMergeFile.c_str());
    TTree * inputHMergeTree = (TTree *) inputHMergeFile->Get("outputTree");
    inputHMergeTree->Print();

    EventMixer mixer(inputHMergeTree, configFileName);
    inputHMergeFile->Close();
    mixer.Print();
    mixer.Sorting();

    const int nMixingBins = mixer.GetNBins() - 1; // Exclude the overflow bin
    /*  
    // No parallel
    for (int ibin = 0; ibin < nMixingBins; ibin++) {
        std::cout << "BinMixing: " << ibin << "/" << nMixingBins << std::endl;
        mixer.BinMixing(ibin);
    }
    */

    // Parallel
    int nThreads = mixer.GetNThreads();
    nThreads = std::min(nThreads, nMixingBins);

    std::vector<std::future<void>> futures;

    auto worker = [&] (int startBin, int endBin) {
        for (int ibin = startBin; ibin < endBin; ibin++) {
            mixer.BinMixing(ibin);
        }
    };

    const int binPerThread = nMixingBins / nThreads;
    int remainingBins = nMixingBins % nThreads;

    int startBin = 0;
    for (int ithread = 0; ithread < nThreads; ithread++) {
        int endBin = startBin + binPerThread + (remainingBins > 0 ? 1 : 0);
        if (remainingBins > 0) {
            --remainingBins;
        }
        futures.push_back(std::async(std::launch::async, worker, startBin, endBin));
        startBin = endBin;
        std::cout << "Thread " << ithread << " / " << nThreads << " completed" << std::endl;
    }
    
    for (auto & future : futures) {
        future.get();
    }

    // Save the mixed tree
    std::string outputFileName = config["OutputFile"].as<std::string>();
    std::cout << "Saving mixed tree to " << outputFileName << std::endl;
    TFile * outputFile = TFile::Open(outputFileName.c_str(), "RECREATE");
    mixer.SaveMixedTree(outputFile, "MixedTree");
    outputFile->Close();
}
