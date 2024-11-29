#include <iostream>
#include <vector>
#include <string>
#include <future>
#include <algorithm>

#include <TFile.h>
#include <TTree.h>

#include "TreeManager.h"
#include "EventMixer.h" 

void MixedEventInterfaceLi4(const char * configFileName) {
    
    std::cout << "MixedEventInterface" << std::endl;
    const char * inputTreeFile = "/data/galucia/lithium_local/same/LHC23_PbPb_pass4_long_same.root";
    const char * inputTreeMergeFile = "/data/galucia/lithium_local/same/LHC23_PbPb_pass4_long_same_merged.root";
    std::vector<std::string> treeNames = {"O2he3hadtable", "O2he3hadmult"};
    
    const bool skipMerging = true;
    if (!skipMerging) {
        MergeAllTrees(inputTreeFile, treeNames, inputTreeMergeFile);
    }

    std::cout << std::endl;
    TFile * inputFile = TFile::Open(inputTreeMergeFile);
    std::vector<TTree *> inputTrees;
    for (const auto & treeName : treeNames) {
        inputTrees.push_back((TTree *) inputFile->Get(treeName.c_str()));
    }

    for (size_t itree = 1; itree < inputTrees.size(); ++itree) {
        inputTrees[0]->AddFriend(inputTrees[itree]);
    }
    for (size_t itree = 0; itree < inputTrees.size(); ++itree) {
        inputTrees[itree]->Print();
    }
    //inputTrees[0]->Print();

    EventMixer mixer(inputTrees[0], configFileName);
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
    TFile * outputFile = TFile::Open("/data/galucia/lithium_local/mixing/LHC23_PbPb_pass4_long_mixing_new.root");
    mixer.SaveMixedTree(outputFile);
    outputFile->Close();
}
