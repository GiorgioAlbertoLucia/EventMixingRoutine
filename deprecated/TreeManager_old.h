/*
    Functions to handle TTrees
*/

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
TTree* TreeMerging(const char * inputFileName, const char * treeName) {
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

    TTree * tree = TTree::MergeTrees(treeList);
    tree->SetDirectory(0);
    inputFile->Close();

    return tree;
}

/**
 * Loops over the directories in a TFile and merges all the TTrees with the given name.
 * Then it horizontally stacks the trees into a single RDataFrame.
 * 
 * @param fileName The path to the file to open.
 * @param treeName The name of the tree to merge.
 * 
 * @return The RDataFrame with the stacked trees.
 */
ROOT::RDataFrame HStackTreeInDataFrame(const char* inputFileName, std::vector<std::string>& treeNames, const char* outputFileName = "") {
    
    TFile *inputFile = TFile::Open(inputFileName, "READ");
    const int nTrees = treeNames.size();

    TFile * outputFile = nullptr;
    if (outputFileName != std::string("")) {
        TFile *outputFile = TFile::Open(outputFileName, "RECREATE");
    }

    std::vector<TTree*> trees;
    for (int itree = 0; itree < nTrees; ++itree) {
        if (outputFile) {
            outputFile->cd();
        }
        TTree *tree = TreeMerging(inputFileName, treeNames[itree].c_str());
        tree->SetDirectory(0);
        if (tree) {
            trees.push_back(tree);
        } else {
            std::cerr << "Tree not found: " << treeNames[itree] << std::endl;
        }
    }
    inputFile->Close();
    std::cout << "Trees merged." << std::endl;

    std::vector<std::string> friendTreeNames;
    for (size_t itree = 1; itree < trees.size(); ++itree) {
        if (outputFile) {
            outputFile->cd();
        }
        trees[0]->AddFriend(trees[itree]);
        friendTreeNames.push_back(trees[itree]->GetName());
    }

    ROOT::RDataFrame dataframe(*trees[0]);
    std::cout << "DataFrame created." << std::endl;

    std::cout << "DataFrame columns: " << std::endl;
    std::cout << "[ ";
    for (auto& column: dataframe.GetColumnNames())
        std::cout << column << ", ";
    std::cout << "]" << std::endl;

    /*
    // change the column names to avoid having the tree name inside
    // DOES NOT WORK YET
    for (auto& column: dataframe.GetColumnNames()) {
        TString newColumn = column;
        // check if the tree names
        for (auto& friendTreeName: friendTreeNames) {
            if (TString(column).Contains(friendTreeName.c_str())) {
                // if the column name contains the tree name, remove it from the column name
                newColumn.ReplaceAll((friendTreeName+".").c_str(), "");
            }
        }
        dataframe.Redefine(newColumn.Data(), column);
    }

    std::cout << "DataFrame columns: " << std::endl;
    std::cout << "[ ";
    for (auto& column: dataframe.GetColumnNames())
        std::cout << column << ", ";
    std::cout << "]" << std::endl;
    */

    if (outputFile) {
        dataframe.Snapshot(std::string("MergedTree"), outputFile->GetName());
        outputFile->Close();
    }    
    
    return dataframe;
}
