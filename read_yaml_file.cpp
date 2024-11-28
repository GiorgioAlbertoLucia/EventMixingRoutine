#include <iostream>
#include <yaml-cpp/yaml.h>
#include <TROOT.h>
#include <TFile.h>
#include <TH1F.h>
#include <TRandom.h>

void readYaml() {
    // Load the YAML file
    YAML::Node config = YAML::LoadFile("sample_data/config.yml");

    // Access the variable
    int parameter = config["parameter"].as<int>();

    // Print the parameter
    std::cout << "Parameter from YAML: " << parameter << std::endl;
    
    // Use the variable in your ROOT analysis
    // Example: Creating a histogram
    TFile *outputFile = new TFile("sample_data/yaml_output.root", "RECREATE");
    TH1F *hist = new TH1F("hist", "Histogram from YAML Parameter", 100, 0, 100);
    for (int i = 0; i < 10000; i++) {
        hist->Fill(gRandom->Gaus(parameter, 10));
    }
    hist->Write();
    outputFile->Close();

    std::cout << "Histogram saved to sample_data/yaml_output.root" << std::endl;
}
