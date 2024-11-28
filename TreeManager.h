/*
    Functions to handle TTrees
*/

#pragma once

#include <iostream>

#include <TTree.h>
#include <TFile.h>
#include <TKey.h>
#include <TDirectory.h>
#include <TList.h>
#include <TString.h>
#include <ROOT/RDataFrame.hxx>

/**
 * Loops over the directories in a TFile and merges all the TTrees with the given name.
 * 
 * @param fileName The path to the file to open.
 * @param treeName The name of the tree to merge.
 */
void TreeMerging(const char * inputFileName, const char * treeName, TFile * outputFile) {
    
    TFile * inputFile = TFile::Open(inputFileName, "READ");
    TList * treeList = new TList();
    TIter nextDir(inputFile->GetListOfKeys());
    TKey *key;
    while ((key = (TKey*)nextDir())) {
        std::cout << "Reading directory: " << key->GetName() << std::endl;
        TObject *obj = key->ReadObj();

        if (obj->InheritsFrom(TDirectory::Class())) {
            TDirectory *dir = (TDirectory*)obj;
            TTree * tmpTree = (TTree*)dir->Get(treeName);
            treeList->Add(tmpTree);
            
        } else {
            std::cerr << "Missing trees in directory: " << key->GetName() << std::endl;
        }
    }

    outputFile->cd();
    TTree * tree = TTree::MergeTrees(treeList);
    //tree->Write();
    inputFile->Close();
}

/**
 * Loops over the directories in a TFile and merges all the TTrees with the given name.
 * 
 * @param fileName The path to the file to open.
 * @param treeName The name of the tree to merge.
 * 
 * @return The RDataFrame with the stacked trees.
 */
void MergeAllTrees(const char* inputFileName, std::vector<std::string>& treeNames, const char* outputFileName) {
    
    //TFile * inputFile = TFile::Open(inputFileName, "READ");
    size_t nTrees = treeNames.size();

    TFile * outputFile = TFile::Open(outputFileName, "RECREATE");

    for (size_t itree = 0; itree < nTrees; ++itree) {
        TreeMerging(inputFileName, treeNames[itree].c_str(), outputFile);
    }
    outputFile->Close();
}