#include <iostream>
#include <vector>
#include <map>
#include <variant>
#include <string>
#include <sstream>

#include <yaml-cpp/yaml.h>
#include <TTree.h>
#include <TFile.h>
#include <TRandom.h>
#include "YamlUtils.h"
#include "TreeReader.h"

using ColumnValue = std::variant<Char_t, UChar_t, Short_t, UShort_t, Int_t, UInt_t, Long64_t, ULong64_t, Float_t, Double_t, bool, std::string>;
using Row = std::map<std::string, ColumnValue>;


void CopyTree(const char * configFilePath) {

    YAML::Node config = YAML::LoadFile(configFilePath);
    
    std::vector<std::string> branches;
    YamlUtils::ReadYamlVector(config["branches"], branches);

    Row row;
    TreeDict::InitRowFromDict(branches, row);

    std::string outputFileName = config["outputFileName"].as<std::string>();
    TFile * outputFile = TFile::Open(outputFileName.c_str(), "RECREATE");
    TTree * outputTree = new TTree("outputTree", "outputTree");
    TreeDict::CreateBranchesFromDict(outputTree, branches, row);

    const int NEntries = 1000;
    for (int i = 0; i < NEntries; i++) {
        for (const auto& [columnName, columnValue]: row) {
            //row[columnName] = gRandom->Rndm();
            row[columnName] = 1.;
        }
        outputTree->Fill();
    }
    outputTree->Write();
    outputFile->Close();
}