#include "Midi/MidiQueue.h"
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
namespace {thread_local bool probe=false;std::atomic<int> allocations{0};int checks=0;}
void* operator new(size_t size){if(probe)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,size_t)noexcept{std::free(p);}
namespace {void expect(bool ok,const char* text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS: "<<text<<'\n';}
struct Packet{uint64_t id=0,checksum=0;};}
int main(){try{
    nova::MidiQueue<Packet,8> small;for(uint64_t i=0;i<8;++i)expect(small.tryPush({i,i^0x5a5a}),"queue accepts packet within prepared capacity");
    expect(!small.tryPush({9,9}),"full queue refuses packet without growth or waiting");Packet packet;
    bool ordered=true;for(uint64_t i=0;i<8;++i)ordered&=small.tryPop(packet)&&packet.id==i&&packet.checksum==(i^0x5a5a);
    expect(ordered&&!small.tryPop(packet),"consumer observes ordered complete payloads and empty queue");
    probe=true;for(uint64_t i=0;i<1000;++i){small.tryPush({i,i});small.tryPop(packet);}probe=false;expect(allocations==0,"prepared queue operations do not allocate C++ storage");
    auto queue=std::make_unique<nova::MidiQueue<Packet,1024>>();constexpr int producers=4,perProducer=5000,total=producers*perProducer;
    std::atomic<bool> go{false};std::vector<std::jthread> writers;
    for(int producer=0;producer<producers;++producer)writers.emplace_back([&,producer](std::stop_token token){
        while(!go&&!token.stop_requested())std::this_thread::yield();for(int i=0;i<perProducer&&!token.stop_requested();++i){const uint64_t id=producer*perProducer+i;
            bool pushed=false;while(!pushed&&!token.stop_requested()){probe=true;pushed=queue->tryPush({id,id^0x5a5a});probe=false;if(!pushed)std::this_thread::yield();}
        }
    });
    std::vector<bool> received(total,false);int count=0;bool intact=true;go=true;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(count<total&&std::chrono::steady_clock::now()<deadline){probe=true;const bool popped=queue->tryPop(packet);probe=false;
        if(!popped){std::this_thread::yield();continue;}if(packet.id>=total||packet.checksum!=(packet.id^0x5a5a)){intact=false;break;}
        if(received[packet.id]){intact=false;break;}received[packet.id]=true;++count;
    }
    for(auto& writer:writers)writer.request_stop();for(auto& writer:writers)writer.join();
    expect(intact&&count==total,"four concurrent MIDI producers deliver 20000 unique intact packets");expect(allocations==0,"concurrent bounded queue calls do not allocate C++ storage");
    std::cout<<checks<<" MIDI queue checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
