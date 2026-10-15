#include "DelayCompensation.h"
#include <algorithm>
#include <stdexcept>

namespace nova {
void StereoDelay::prepare(int samples) {
    if(samples<0 || samples>maximumDelay)throw std::invalid_argument("PDC delay outside supported range");
    cells.assign(static_cast<size_t>(samples),Cell{});write=0;generation=1;
}
void StereoDelay::reset() noexcept {
    write=0;
    // At 10^6 resets/second a uint64 rollover takes over 500,000 years.
    ++generation;
}
std::array<float,2> StereoDelay::process(float left,float right) noexcept {
    if(cells.empty())return {left,right};
    auto& cell=cells[write];const auto result=cell.generation==generation?cell.value:std::array<float,2>{};
    cell.value={left,right};cell.generation=generation;if(++write==cells.size())write=0;return result;
}
bool DelayCompensation::build(const std::vector<PdcNode>& nodes,int masterLatency,juce::String& error) {
    const int count=static_cast<int>(nodes.size());
    if(count>maximumNodes || masterLatency<0 || masterLatency>StereoDelay::maximumDelay){error="Limite de canais/latencia PDC excedido.";return false;}
    std::array<int,maximumNodes> degree{};std::array<bool,maximumNodes> visited{};
    auto valid=[&](int source,int dest){return dest==-1 || (dest>=0 && dest<count && dest!=source && nodes[dest].auxiliary);};
    for(int i=0;i<count;++i){const auto& n=nodes[i];
        if(n.sidechainSource<-1||n.sidechainSource>=count||n.sidechainSource==i){error="Fonte sidechain invalida.";return false;}if(n.sidechainSource>=0)++degree[i];
        if(n.latency<0 || n.latency>StereoDelay::maximumDelay || !valid(i,n.output)){error="Destino ou latencia de canal invalida.";return false;}
        if(n.output>=0)++degree[n.output];
        for(int target:n.sends){if(!valid(i,target)){error="Send invalido.";return false;}if(target>=0)++degree[target];}
    }
    for(int used=0;used<count;++used){int node=-1;for(int i=0;i<count;++i)if(!visited[i] && degree[i]==0){node=i;break;}
        if(node<0){error="Ciclo de roteamento nao suportado.";return false;}
        order[used]=node;visited[node]=true;
        const auto& n=nodes[node];if(n.output>=0)--degree[n.output];for(int dest:n.sends)if(dest>=0)--degree[dest];
        for(int dest=0;dest<count;++dest)if(nodes[dest].sidechainSource==node)--degree[dest];
    }
    inputLatency={};outputLatency={};masterInputLatency=0;allocatedBytes=0;
    for(int pos=0;pos<count;++pos){const int i=order[pos];const auto& n=nodes[i];
        const int64_t accumulated=static_cast<int64_t>(inputLatency[i])+n.latency;
        if(accumulated>StereoDelay::maximumDelay){error="Caminho PDC excede 262144 amostras.";return false;}
        outputLatency[i]=static_cast<int>(accumulated);
        for(int dest=0;dest<count;++dest)if(nodes[dest].sidechainSource==i)inputLatency[dest]=std::max(inputLatency[dest],outputLatency[i]);
        if(n.output>=0)inputLatency[n.output]=std::max(inputLatency[n.output],outputLatency[i]);
        else masterInputLatency=std::max(masterInputLatency,outputLatency[i]);
        for(int dest:n.sends)if(dest>=0)inputLatency[dest]=std::max(inputLatency[dest],outputLatency[i]);
    }
    const int64_t total=static_cast<int64_t>(masterInputLatency)+masterLatency;
    if(total>StereoDelay::maximumDelay){error="Master PDC excede 262144 amostras.";return false;}
    totalLatency=static_cast<int>(total);
    std::array<std::array<int,3>,maximumNodes> lengths{};
    for(int i=0;i<count;++i){const auto& n=nodes[i];lengths[i][0]=(n.output<0?masterInputLatency:inputLatency[n.output])-outputLatency[i];
        targets[i]={n.output,n.sends[0],n.sends[1]};
        for(int slot=0;slot<2;++slot)if(n.sends[slot]>=0)lengths[i][slot+1]=inputLatency[n.sends[slot]]-outputLatency[i];
        for(int delay:lengths[i])allocatedBytes+=static_cast<size_t>(delay)*16; // StereoDelay::Cell
        if(!n.auxiliary)allocatedBytes+=size_t(inputLatency[i])*16;
        if(n.sidechainSource>=0)allocatedBytes+=size_t(inputLatency[i]-outputLatency[n.sidechainSource])*16;
        int preceding=0;for(int latency:n.insertLatencies){if(latency<0||latency>StereoDelay::maximumDelay||preceding>StereoDelay::maximumDelay){error="Latencia sidechain invalida.";return false;}if(n.sidechainSource>=0)allocatedBytes+=size_t(preceding)*16;preceding+=latency;}
    }
    if(allocatedBytes>maximumMemory){error="Buffers de PDC excedem 128 MiB.";return false;}
    try{bool hasSidechain=false;for(const auto& node:nodes)hasSidechain|=node.sidechainSource>=0;if(hasSidechain)sidechain=std::make_unique<SidechainDelays>();for(int i=0;i<count;++i){for(int edge=0;edge<3;++edge)edges[i][edge].prepare(lengths[i][edge]);if(sidechain){sidechain->sourceAlignment[i].prepare(nodes[i].auxiliary?0:inputLatency[i]);sidechain->detectorAlignment[i].prepare(nodes[i].sidechainSource<0?0:inputLatency[i]-outputLatency[nodes[i].sidechainSource]);int preceding=0;for(int slot=0;slot<4;++slot){sidechain->detectorInserts[i][slot].prepare(nodes[i].sidechainSource<0?0:preceding);preceding+=nodes[i].insertLatencies[slot];}}}}
    catch(const std::bad_alloc&){error="Memoria insuficiente para PDC.";return false;}
    return true;
}
void DelayCompensation::reset() noexcept {for(auto& node:edges)for(auto& edge:node)edge.reset();if(sidechain){for(auto& delay:sidechain->sourceAlignment)delay.reset();for(auto& delay:sidechain->detectorAlignment)delay.reset();for(auto& node:sidechain->detectorInserts)for(auto& delay:node)delay.reset();}}
}
