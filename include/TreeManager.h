/*
    Functions to handle TTrees
*/

#pragma once

#include <iostream>
#include <vector>
#include <string>

#include <yaml-cpp/yaml.h>

#include <TTree.h>
#include <TFile.h>
#include <TKey.h>
#include <TDirectory.h>
#include <TList.h>
#include <TString.h>

#include "TreeReader.h"
#include "Row.h"

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
    tree->Write();
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
        std::cout << "Merging tree: " << treeNames[itree] << std::endl;
        TreeMerging(inputFileName, treeNames[itree].c_str(), outputFile);
    }
    std::cout << "Merging done" << std::endl;
    outputFile->Close();
}

/**
 * Merge the trees horizontally by adding the columns of the trees.
*/
void HorizontalMerge(const char* inputFileName, std::vector<std::string>& treeNames, const char* outputFileName, std::vector<std::vector<std::string>>& columnDicts, std::vector<std::string>& columnDictFull) {
    
    TFile * inputFile = TFile::Open(inputFileName, "READ");
    size_t nTrees = treeNames.size();

    std::vector<TTree *> inputTrees;
    std::vector<Row> inputRows;
    std::vector<std::vector<std::string>> columnNames;

    inputTrees.reserve(nTrees);
    inputRows.reserve(nTrees);

    // read the trees to merge and initialize the rows
    std::cout << "Reading trees" << std::endl;
    for (size_t itree = 0; itree < nTrees; ++itree) {
        TTree * tree = (TTree*)inputFile->Get(treeNames[itree].c_str());
        tree->Print();
        inputTrees.push_back(tree);
        Row inputRow;
        //TreeDict::InitRowFromDict(columnDicts[itree], inputRow);
        inputRow.InitRowFromDict(columnDicts[itree]);
        inputRows.push_back(inputRow);
        //TreeDict::SetBranchAddressesFromDict(inputTrees[itree], columnDicts[itree], inputRows[itree]);
        inputRows[itree].SetBranchAddressesFromDict(inputTrees[itree], columnDicts[itree]);
        std::vector<std::string> columnNamesTmp = TreeDict::GetColumnNamesFromDict(columnDicts[itree]);
        columnNames.push_back(columnNamesTmp);
    }

    // create the output tree
    std::cout << "Creating output tree" << std::endl;
    TFile * outputFile = TFile::Open(outputFileName, "RECREATE");
    TTree * outputTree = new TTree("outputTree", "outputTree");
    Row outputRow;
    //TreeDict::InitRowFromDict(columnDictFull, outputRow);
    //TreeDict::CreateBranchesFromDict(outputTree, columnDictFull, outputRow);
    outputRow.InitRowFromDict(columnDictFull);
    outputRow.CreateBranchesFromDict(outputTree, columnDictFull);

    std::cout << "Merging trees" << std::endl;
    const int nEntries = inputTrees[0]->GetEntries();
    for (int ientry = 0; ientry < nEntries; ++ientry) {
        for (size_t itree = 0; itree < nTrees; ++itree) {
            inputTrees[itree]->GetEntry(ientry);
            for (const auto & column : columnNames[itree]) {
                outputRow[column] = inputRows[itree][column];
            }
        }
        outputTree->Fill();
    }
    outputFile->cd();
    outputTree->Write();

    std::cout << "Merging done" << std::endl;
    inputFile->Close();
    outputFile->Close();
}
