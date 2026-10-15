#pragma once
#include <array>
#include <cmath>
namespace nova {
enum class Speaker {left,right,centre,lfe,leftSurround,rightSurround,leftRear,rightRear};
struct ChannelLayout {
    int channels=2;
    std::array<Speaker,8> order{Speaker::left,Speaker::right,Speaker::centre,Speaker::lfe,Speaker::leftSurround,Speaker::rightSurround,Speaker::leftRear,Speaker::rightRear};
    bool valid()const{return channels==1||channels==2||channels==6||channels==8;}
};
// Prepared multichannel routing primitive. Current playback/device graph remains
// stereo; no claim of 5.1/7.1 plugin or hardware playback is made by this matrix.
struct ChannelMatrix {
    ChannelLayout input,output;
    std::array<std::array<float,8>,8> gains{};
    bool configure(ChannelLayout in,ChannelLayout out){if(!in.valid()||!out.valid())return false;input=in;output=out;gains={};
        if(in.channels==out.channels){for(int c=0;c<in.channels;++c)gains[c][c]=1;}
        else if(out.channels==2){gains[0][0]=1;gains[1][in.channels==1?0:1]=1;if(in.channels>=6){gains[0][2]=gains[1][2]=float(std::sqrt(0.5));gains[0][4]=gains[1][5]=float(std::sqrt(0.5));}if(in.channels==8){gains[0][6]=gains[1][7]=float(std::sqrt(0.5));}}
        else if(in.channels==2&&out.channels>=6){gains[0][0]=gains[1][1]=1;}else return false;return true;}
    void process(const float* const* source,float* const* destination,int frames)const noexcept{for(int out=0;out<output.channels;++out)for(int n=0;n<frames;++n){float value=0;for(int in=0;in<input.channels;++in)value+=source[in][n]*gains[out][in];destination[out][n]=value;}}
};
}
