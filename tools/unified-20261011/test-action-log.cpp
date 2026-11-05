#include "../../src/unified-20261011/bridge/src/action-sink.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <set>
#include <cstdlib>
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int wmain(int argc,wchar_t** argv) {
    CHECK(argc==2);
    const std::filesystem::path dir=argv[1];
    CHECK(std::filesystem::create_directory(dir));
    auto sink=std::make_unique<ActionSink>();
    sink->Configure(dir.c_str());
    sink->Flush();
    for(unsigned i=0;i<20;++i) {
        char s[128]; std::snprintf(s,sizeof(s),"[action] key=ForceMultiplier before=%u requested=%u",i%6,(i+1)%6);
        SetLastError(1234);sink->Add(3,s);CHECK(GetLastError()==1234);
    }
    sink->Add(2,"[action] newline\ninjection\rtest");
    sink->Add(2,std::string(1800,'x').c_str());
    sink->Flush();
    std::vector<std::thread> producers;
    for(int t=0;t<4;++t) producers.emplace_back([&]{for(int n=0;n<1000;++n)sink->Add(1,"[action] concurrent");});
    for(auto& t:producers)t.join();
    sink->Flush(); // The writer remains open while the reader inspects it.
    std::string all;
    for(const auto& file:std::filesystem::directory_iterator(dir)) {
        // std::ifstream on Windows allows reading the active shared file.
        std::ifstream in(file.path());CHECK(in.good());
        all.append(std::istreambuf_iterator<char>(in),{});
    }
    size_t actions=0,at=0;while((at=all.find("key=ForceMultiplier",at))!=std::string::npos){++actions;++at;}
    CHECK(actions==20);CHECK(all.find("newline injection test")!=std::string::npos);
    CHECK(all.find("truncated=1")!=std::string::npos);
    CHECK(all.find("qpc_frequency=")!=std::string::npos);
    const auto gap=all.find("[action-gap] lost=");CHECK(gap!=std::string::npos);
    const auto lost=std::strtoull(all.c_str()+gap+18,nullptr,10);
    size_t received=0;at=0;while((at=all.find("[action] concurrent",at))!=std::string::npos){++received;++at;}
    CHECK(received+lost==4000);
    std::set<unsigned long long> sequence;
    at=0;while((at=all.find(" seq=",at))!=std::string::npos){CHECK(sequence.insert(std::strtoull(all.c_str()+at+5,nullptr,10)).second);++at;}
    CHECK(dlss5::eventlog::IsOperationEvidence("prefix [action] phase=config_request"));
    CHECK(!dlss5::eventlog::IsOperationEvidence("[fg-relay] rate presents=60"));
    std::printf("PASS: %d assertions, 20 transitions retained, concurrent accepted=%zu lost=%llu accounted; active file readable.\n",checks,received,lost);
}
