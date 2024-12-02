#include <iostream>
#include <vector>
#include <string>
#include <future>
#include <algorithm>

#include <TFile.h>
#include <TTree.h>

#include "include/YamlUtils.h"
#include "include/TreeManager.h"
#include "include/EventMixer.h" 

void MixedEventInterface(const char * configFileName) {
    
    std::cout << "MixedEventInterface" << std::endl;

    YAML::Node config = YAML::LoadFile(configFileName);
    std::string inputTreeFile = config["InputTreeFile"].as<std::string>();
    std::vector<std::string> treeNames;
    YamlUtils::ReadYamlVector(config["TreeNames"], treeNames);
    std::string inputTreeMergeFile = config["InputTreeMergeFile"].as<std::string>();
    std::string inputTreeHMergeFile = config["InputTreeHMergeFile"].as<std::string>();

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

    //std::vector<TTree *> inputTrees;
    //for (const auto & treeName : treeNames) {
    //    inputTrees.push_back((TTree *) inputFile->Get(treeName.c_str()));
    //}
    //for (size_t itree = 1; itree < inputTrees.size(); ++itree) {
    //    inputTrees[0]->AddFriend(inputTrees[itree]);
    //}

    HorizontalMerge(inputTreeMergeFile.c_str(), treeNames, inputTreeHMergeFile.c_str(), 
                    columnDicts, columnDictFull);
    TFile * inputHMergeFile = TFile::Open(inputTreeHMergeFile.c_str());
    TTree * inputHMergeTree = (TTree *) inputHMergeFile->Get("outputTree");

    //EventMixer mixer(inputTrees[0], configFileName);
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
        std::cout << "Thread: " << ithread << "/" << nThreads << std::endl;
        int endBin = startBin + binPerThread + (remainingBins > 0 ? 1 : 0);
        if (remainingBins > 0) {
            --remainingBins;
        }
        futures.push_back(std::async(std::launch::async, worker, startBin, endBin));
        startBin = endBin;
    }
    
    for (auto & future : futures) {
        future.get();
    }

    // Save the mixed tree
    std::cout << "Saving mixed tree to sample_data/mixed_trees.root"  << std::endl;
    TFile * outputFile = TFile::Open("sample_data/mixed_trees.root", "RECREATE");
    mixer.SaveMixedTree(outputFile);
    outputFile->Close();
}
