#include <string>
#include <TString.h>
#include <TSystem.h>

void load(TString myopt = "fast") {
    
    gSystem->AddIncludePath((std::string("-I ")+"build").c_str());
    
    //gSystem->AddIncludePath((std::string("-I ")+"/home/galucia/local/include").c_str());
    //gSystem->Load("/home/galucia/local/lib/libyaml-cpp.so");
    
    gSystem->AddIncludePath((std::string("-I ")+"/opt/homebrew/opt/yaml-cpp/include").c_str());
    gSystem->Load("/opt/homebrew/opt/yaml-cpp/lib/libyaml-cpp.0.8.dylib");
    
    TString opt;
    if(myopt.Contains("force"))   opt = "kfg";
    else                          opt = "kg";
  

    gSystem->CompileMacro("ReadYamlFile.cpp", opt.Data(), "", "build");
    //gSystem->CompileMacro(".L /home/galucia/antiLithium4/mixed_event/TreeManager.cpp+", opt.Data(), "", "build");
    gSystem->CompileMacro("GenerateExampleTrees.cpp", opt.Data(), "", "build");
    gSystem->CompileMacro("MixedEventRoutine.cpp", opt.Data(), "", "build");

    gSystem->CompileMacro("Test.cpp", opt.Data(), "", "build");
    //gSystem->CompileMacro("MixedEventInterface.cpp", opt.Data(), "", "build");
    gSystem->CompileMacro("MixedEventInterfaceLi4.cpp", opt.Data(), "", "build");
}