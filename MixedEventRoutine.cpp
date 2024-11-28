#include <iostream>
#include <vector>
#include <string>
#include <variant>
#include <thread>
#include <future>

#include <ROOT/RDataFrame.hxx>
#include <ROOT/RDF/RInterface.hxx>
#include <TFile.h>
#include <TKey.h>
#include <TDirectory.h>
#include <TString.h>
#include <TRandom3.h>

#include <yaml-cpp/yaml.h>

#include "TreeManager_old.h"
#include "YamlUtils.h"

typedef struct {
    const char * xvarName;
    float xlow, xmax;
    const char * yvarName;
    float ylow, ymax;
} bin2d;

namespace Personal {
    std::map<std::string, std::string> columnTypeDictTest = {
        {"x0", "Double_t"}, // dummy centrality
        {"x1", "Double_t"}, // dummy z vertex
        {"x2", "Double_t"}, // x2, x3, x4, x5, x6 are the dummy variables to mix
        {"x3", "Int_t"},
        {"x4", "Double_t"},
        {"x5", "Double_t"},
        {"x6", "Double_t"},
        {"x7", "Int_t"}     // dummy collision ID
    };

    std::vector<std::string> columnNamesTest = {
        "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
    };
    std::vector<std::string> trackAColumnNames = {
        "x2", "x3"
    };
    std::vector<std::string> trackBColumnNames = {
        "x4", "x5", "x6"
    };
    std::vector<std::string> sharedColumnNames = {
        "x0", "x1", "x7"
    };
}

/**
 * Creates binned trees from the given dataframe.
*/
void SaveBinnedTrees(ROOT::RDataFrame& df, YAML::Node& config) {

    std::string outputFileName = config["bin_tree_file"].as<std::string>();
    std::vector<std::string> columnNames;
    YamlUtils::ReadYamlVector(config["column_names"], columnNames);

    const std::string binningColumnA = config["binning_column_A"].as<std::string>();
    const std::string binningColumnB = config["binning_column_B"].as<std::string>();

    // bin definition
    std::vector<float> centrality_value, zvertex_value;
    YamlUtils::ReadYamlVector(config["centrality_bin_edges"], centrality_value);
    YamlUtils::ReadYamlVector(config["z_vertex_bin_edges"], zvertex_value);

    TFile * outputFileTree = TFile::Open(outputFileName.c_str(), "RECREATE");
    outputFileTree->Close();
    outputFileTree = TFile::Open(outputFileName.c_str(), "UPDATE");
    ROOT::RDF::RSnapshotOptions options;
    options.fMode = "UPDATE";

    for (size_t icent = 0; icent < centrality_value.size()-1; icent++) {
        std::cout << binningColumnA << ": " << centrality_value[icent] << " - " << centrality_value[icent+1] << std::endl;
        for (size_t izvtx = 0; izvtx < zvertex_value.size()-1; izvtx++) {
            std::cout << binningColumnB << ": " << zvertex_value[izvtx] << " - " << zvertex_value[izvtx+1] << std::endl;
            const char * filterName = Form("same_event_%ld_%ld", icent, izvtx);

            auto df_tmp = df.Filter(Form("%s > %f ", binningColumnA.c_str(), centrality_value[icent]), filterName)
                            .Filter(Form("%s < %f", binningColumnA.c_str(), centrality_value[icent+1]), filterName)
                            .Filter(Form("%s > %f", binningColumnB.c_str(), zvertex_value[izvtx]), filterName)
                            .Filter(Form("%s < %f", binningColumnB.c_str(), zvertex_value[izvtx+1]), filterName);
            std::cout << "Snapshotting..." << std::endl;
            
            //df_tmp.Snapshot(filterName, outputFileTree->GetName(), columnNames, options);
            df_tmp.Snapshot(filterName, outputFileTree->GetName());
            std::cout << "Snapshot complete." << std::endl;
        }
    }
}

TTree* SingleBinMixing(TTree* binTree, const YAML::Node& config) {
    
    std::string filterNameStr(binTree->GetName());
    filterNameStr = filterNameStr.substr(filterNameStr.find_first_of("_")+1);;
    auto mixedTree = new TTree(Form("mixed_%s", filterNameStr.c_str()), Form("mixed_%s", filterNameStr.c_str()));

    const int bufferSize = config["buffer_size"].as<int>(); // How many pairs for each event
    const int nEvents = binTree->GetEntries();

    std::vector<std::string> columnNames, trackAColumnNames, trackBColumnNames, sharedColumnNames;
    YamlUtils::ReadYamlVector(config["column_names"], columnNames);
    YamlUtils::ReadYamlVector(config["trackA_column_names"], trackAColumnNames);
    YamlUtils::ReadYamlVector(config["trackB_column_names"], trackBColumnNames);
    YamlUtils::ReadYamlVector(config["shared_column_names"], sharedColumnNames);
    std::map<std::string, std::string> columnTypeDict;
    YamlUtils::ReadYamlMap(config["column_type_dict"], columnTypeDict);
    const std::string collisionID = config["collision_id_column"].as<std::string>();


    // prepare branches
    using ColumnValue = std::variant<int, double, float, unsigned short, Long64_t, ULong64_t, unsigned int, unsigned char, bool, std::string>;
    std::map<std::string, ColumnValue> sameColumnData;
    std::map<std::string, ColumnValue> mixedColumnData;

    for (const auto& [columnName, columnType]: columnTypeDict) {
        if (columnType == "Int_t") {
            sameColumnData[columnName] = int(0);
            mixedColumnData[columnName] = int(0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<int>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<int>(mixedColumnData[columnName]), (columnName + "/I").c_str());
        } else if (columnType == "Double_t") {
            sameColumnData[columnName] = double(0.0);
            mixedColumnData[columnName] = double(0.0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<double>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<double>(mixedColumnData[columnName]), (columnName + "/D").c_str());
        } else if (columnType == "Float_t") {
            sameColumnData[columnName] = float(0.0f);
            mixedColumnData[columnName] = float(0.0f);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<float>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<float>(mixedColumnData[columnName]), (columnName + "/F").c_str());
        } else if (columnType == "Long_t") {
            sameColumnData[columnName] = static_cast<Long64_t>(0);
            mixedColumnData[columnName] = static_cast<Long64_t>(0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<Long64_t>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<Long64_t>(mixedColumnData[columnName]), (columnName + "/L").c_str());
        } else if (columnType == "ULong_t") {
            sameColumnData[columnName] = static_cast<ULong64_t>(0);
            mixedColumnData[columnName] = static_cast<ULong64_t>(0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<ULong64_t>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<ULong64_t>(mixedColumnData[columnName]), (columnName + "/l").c_str());
        } else if (columnType == "UShort_t") {
            sameColumnData[columnName] = static_cast<unsigned short>(0);
            mixedColumnData[columnName] = static_cast<unsigned short>(0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<unsigned short>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<unsigned short>(mixedColumnData[columnName]), (columnName + "/s").c_str());
        } else if (columnType == "UInt_t") {
            sameColumnData[columnName] = static_cast<unsigned int>(0);
            mixedColumnData[columnName] = static_cast<unsigned int>(0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<unsigned int>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<unsigned int>(mixedColumnData[columnName]), (columnName + "/i").c_str());
        } else if (columnType == "UChar_t") {
            sameColumnData[columnName] = static_cast<unsigned char>(0);
            mixedColumnData[columnName] = static_cast<unsigned char>(0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<unsigned char>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<unsigned char>(mixedColumnData[columnName]), (columnName + "/b").c_str());
        } else if (columnType == "bool") {
            sameColumnData[columnName] = static_cast<bool>(0);
            mixedColumnData[columnName] = static_cast<bool>(0);
            binTree->SetBranchAddress(columnName.c_str(), &std::get<bool>(sameColumnData[columnName]));
            mixedTree->Branch(columnName.c_str(), &std::get<bool>(mixedColumnData[columnName]), (columnName + "/O").c_str());
        } else {
            throw std::runtime_error("Unsupported column type: " + columnType);
        }
    }

    int bufferSizeCounter = 0;
    for (int ientry = 0; ientry < nEvents; ientry++) {
        binTree->GetEntry(ientry);
        
        for (const auto& trackAColumnName: Personal::trackAColumnNames) {
            mixedColumnData[trackAColumnName] = sameColumnData[trackAColumnName];
        }
        for (const auto& sharedColumnName: Personal::sharedColumnNames) {
            mixedColumnData[sharedColumnName] = sameColumnData[sharedColumnName];
        }
        
        while (bufferSizeCounter < bufferSize) {
            const int randomIndex = gRandom->Integer(nEvents);
            binTree->GetEntry(randomIndex);
            if (sameColumnData[collisionID] == mixedColumnData[collisionID]) {
                continue;
            }
            for (const auto& trackBColumnName: Personal::trackBColumnNames) {
                mixedColumnData[trackBColumnName] = sameColumnData[trackBColumnName];
            }
            mixedTree->Fill();
            bufferSizeCounter++;
        }

        bufferSizeCounter = 0;
    }
    return mixedTree;
}

void DisplayInitialMessage(const YAML::Node& config) {
    std::cout << "=== Mixed Event Routine ===" << std::endl;
    std::cout << "Configuration Details:" << std::endl;

    // Display input and output file information
    std::cout << "Input File: " << config["input_file"].as<std::string>() << std::endl;
    std::cout << "Bin Tree File: " << config["bin_tree_file"].as<std::string>() << std::endl;
    std::cout << "Output File: " << config["output_file"].as<std::string>() << std::endl;

    // Display binning details
    std::cout << "Binning Columns: " << std::endl;
    std::cout << "  A: " << config["binning_column_A"].as<std::string>() << std::endl;
    std::cout << "  B: " << config["binning_column_B"].as<std::string>() << std::endl;

    std::vector<float> centralityEdges, zvertexEdges;
    YamlUtils::ReadYamlVector(config["centrality_bin_edges"], centralityEdges);
    YamlUtils::ReadYamlVector(config["z_vertex_bin_edges"], zvertexEdges);
    std::cout << "Centrality Bin Edges: ";
    for (auto val : centralityEdges) std::cout << val << " ";
    std::cout << std::endl;
    std::cout << "Z Vertex Bin Edges: ";
    for (auto val : zvertexEdges) std::cout << val << " ";
    std::cout << std::endl;

    // Display tree names
    std::cout << "Tree Names: ";
    std::vector<std::string> treeNames;
    YamlUtils::ReadYamlVector(config["tree_names"], treeNames);
    for (const auto& name : treeNames) std::cout << name << " ";
    std::cout << std::endl;

    // Display buffer size for mixing
    std::cout << "Mixing Buffer Size: " << config["buffer_size"].as<int>() << std::endl;

    // Display column configuration details
    std::cout << "Columns for Mixing:" << std::endl;
    std::vector<std::string> trackAColumns, trackBColumns, sharedColumns;
    YamlUtils::ReadYamlVector(config["trackA_column_names"], trackAColumns);
    YamlUtils::ReadYamlVector(config["trackB_column_names"], trackBColumns);
    YamlUtils::ReadYamlVector(config["shared_column_names"], sharedColumns);
    std::cout << "  Track A Columns: ";
    for (const auto& col : trackAColumns) std::cout << col << " ";
    std::cout << std::endl;
    std::cout << "  Track B Columns: ";
    for (const auto& col : trackBColumns) std::cout << col << " ";
    std::cout << std::endl;
    std::cout << "  Shared Columns: ";
    for (const auto& col : sharedColumns) std::cout << col << " ";
    std::cout << std::endl;

    std::cout << "Collision ID Column: " << config["collision_id_column"].as<std::string>() << std::endl;
    std::cout << "=========================" << std::endl;
}

void MixedEventRoutine(const char* configFilePath) {
    
    YAML::Node config = YAML::LoadFile(configFilePath);
    DisplayInitialMessage(config);
    
    std::vector<std::string> treeNames;
    YamlUtils::ReadYamlVector(config["tree_names"], treeNames);

    std::string inputFileName = config["input_file"].as<std::string>();
    std::string mergedTreeFileName = config["merged_tree_file"].as<std::string>();
    ROOT::RDataFrame df = HStackTreeInDataFrame(inputFileName.c_str(), treeNames, mergedTreeFileName.c_str());

    std::string binFileName = config["bin_tree_file"].as<std::string>();
    SaveBinnedTrees(df, config);
    TFile * binFileTree = TFile::Open(binFileName.c_str(), "READ");

    std::vector<TTree*> mixedBinTrees;
    std::string outputFileName = config["output_file"].as<std::string>();
    TFile * outputFileTree = TFile::Open(outputFileName.c_str(), "RECREATE");
    
    TIter next(binFileTree->GetListOfKeys());
    TKey *key;
    std::vector<std::future<TTree*>> futures;

    while ((key = (TKey*)next())) {
        std::cout << "=";
        futures.push_back(std::async(std::launch::async, [binFileName, key, &config]() {
            TFile *localFile = TFile::Open(binFileName.c_str(), "READ");
            TTree *binTree = (TTree*)localFile->Get(key->GetName());
            TTree *result = SingleBinMixing(binTree, config);
            localFile->Close();
            delete localFile;
            return result;
        }));
    }
    std::vector<TTree*> mixedBinTreesTmp;
    for (auto& future: futures) {
        mixedBinTreesTmp.push_back(future.get());
    }

    //std::cout << "Mixing..." << std::endl;
    //while ((key = (TKey*)next())) {
    //    std::cout << "=";
    //    TTree * binTree = (TTree*)key->ReadObj();
    //    outputFileTree->cd();
    //    TTree * mixedBinTree = SingleBinMixing(binTree, config);
    //    mixedBinTrees.push_back(mixedBinTree);
    //    delete binTree;
    //}
    //std::cout << std::endl << "Completed!" << std::endl;

    //for (auto& mixedBinTree: mixedBinTrees) {
    //    outputFileTree->cd();
    //    mixedBinTree->Write();
    //}
    TList * mixedBinTreeList = new TList();
    for (TTree* mixedBinTree: mixedBinTrees) {
        mixedBinTreeList->Add(mixedBinTree);
    }
    TTree * mixedTree = TTree::MergeTrees(mixedBinTreeList);
    outputFileTree->cd();
    mixedTree->Write();
    outputFileTree->Close();
    
    return;
}
