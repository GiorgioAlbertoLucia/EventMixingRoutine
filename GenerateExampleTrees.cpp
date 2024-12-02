#include <TFile.h>
#include <TTree.h>
#include <TDirectory.h>
#include <TRandom.h>

#include <vector>

/**
 * This function creates a file with two folders each containing two trees with a single branch.
*/
void GenerateExampleTrees(const char* outputFileName) {
    // Create a new ROOT file
    TFile *file = TFile::Open(outputFileName, "RECREATE");

    std::vector<TDirectory*> directories;
    std::vector<TTree*> trees;
    for (int idir = 0; idir < 2; ++idir) {
        // Create a new directory
        TDirectory *dir = file->mkdir(Form("dir%d", idir));
        dir->cd();
        directories.push_back(dir);

        // Create new trees
        for (int itree = 0; itree < 2; ++itree) {
            TTree *tree = new TTree(Form("Tree%d", itree), Form("Tree%d", itree));
            double x0, x1, x2;
            int x3;
            tree->Branch(Form("x%d", itree*4+0), &x0);
            tree->Branch(Form("x%d", itree*4+1), &x1);
            tree->Branch(Form("x%d", itree*4+2), &x2);
            tree->Branch(Form("x%d", itree*4+3), &x3);
            for (int nelements = 0; nelements < 1000000; ++nelements) {
                x0 = 200*gRandom->Rndm() - 100;
                x1 = 200*gRandom->Rndm() - 100;
                x2 = 200*gRandom->Rndm() - 100;
                x3 = gRandom->Integer(100);
                tree->Fill();
            }
            tree->Write();
            trees.push_back(tree);
        }
    }

    // Close the file
    file->Close();

}