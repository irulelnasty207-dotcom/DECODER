#include "yurlaps_decoder/yurlaps.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace yurlaps::decoder {
namespace {
constexpr int STATES = 256;
constexpr int PAYLOAD_BITS = 40;
constexpr std::uint16_t POLY_A = 0x1af;
constexpr std::uint16_t POLY_B = 0x11d;
int parity(std::uint16_t value) { int result=0; while(value){result^=value&1u;value>>=1u;} return result; }
std::pair<int,int> encoded_pair(int state,int input){const auto reg=static_cast<std::uint16_t>(((state<<1)|input)&0x1ff);return {parity(reg&POLY_A),parity(reg&POLY_B)};}
std::uint16_t crc16(std::uint32_t id){const std::array<std::uint8_t,3>b{static_cast<std::uint8_t>(id>>16u),static_cast<std::uint8_t>(id>>8u),static_cast<std::uint8_t>(id)};std::uint16_t c=0xffffu;for(auto x:b){c^=static_cast<std::uint16_t>(x)<<8u;for(int i=0;i<8;i++)c=(c&0x8000u)?static_cast<std::uint16_t>((c<<1u)^0x1021u):static_cast<std::uint16_t>(c<<1u);}return c;}
std::array<bool,PAYLOAD_BITS> payload_bits(std::uint32_t id){const auto c=crc16(id);const std::array<std::uint8_t,5>b{static_cast<std::uint8_t>(id>>16u),static_cast<std::uint8_t>(id>>8u),static_cast<std::uint8_t>(id),static_cast<std::uint8_t>(c>>8u),static_cast<std::uint8_t>(c)};std::array<bool,PAYLOAD_BITS>r{};for(std::size_t i=0;i<r.size();i++)r[i]=(b[i/8]&(0x80u>>(i%8)))!=0;return r;}
}
std::array<std::uint8_t,YURLAPS_PAYLOAD_SYMBOLS> encode_yurlaps(std::uint32_t id){std::array<std::uint8_t,YURLAPS_PAYLOAD_SYMBOLS>r{};const auto bits=payload_bits(id);int initial=0;for(int s=0;s<STATES;s++){int t=s;for(bool b:bits)t=((t<<1)|(b?1:0))&0xff;if(t==s){initial=s;break;}}int state=initial;std::size_t p=0;for(bool b:bits){auto q=encoded_pair(state,b?1:0);r[p++]=q.first?255u:0u;r[p++]=q.second?255u:0u;state=((state<<1)|(b?1:0))&0xff;}return r;}
std::optional<YurlapsFrame> decode_yurlaps(const std::array<std::uint8_t,YURLAPS_PAYLOAD_SYMBOLS>& softbits){int best=std::numeric_limits<int>::min(),best_state=0;std::array<bool,PAYLOAD_BITS> best_bits{};for(int initial=0;initial<STATES;initial++){std::array<std::array<int,STATES>,PAYLOAD_BITS+1>score{};std::array<std::array<std::uint8_t,STATES>,PAYLOAD_BITS+1>prev{};std::array<std::array<bool,STATES>,PAYLOAD_BITS+1>dec{};for(auto&r:score)r.fill(std::numeric_limits<int>::min()/4);score[0][initial]=0;for(int step=0;step<PAYLOAD_BITS;step++)for(int state=0;state<STATES;state++)if(score[step][state]>std::numeric_limits<int>::min()/8)for(int input=0;input<=1;input++){auto q=encoded_pair(state,input);int a=static_cast<int>(softbits[step*2])-127,b=static_cast<int>(softbits[step*2+1])-127,next=((state<<1)|input)&0xff,c=score[step][state]+a*(q.first?1:-1)+b*(q.second?1:-1);if(c>score[step+1][next]){score[step+1][next]=c;prev[step+1][next]=static_cast<std::uint8_t>(state);dec[step+1][next]=input!=0;}}if(score[PAYLOAD_BITS][initial]<=best)continue;best=score[PAYLOAD_BITS][initial];best_state=initial;int s=initial;for(int step=PAYLOAD_BITS;step>0;step--){best_bits[step-1]=dec[step][s];s=prev[step][s];}}
if(best==std::numeric_limits<int>::min())return std::nullopt;std::array<std::uint8_t,5>b{};for(int i=0;i<PAYLOAD_BITS;i++)b[i/8]=static_cast<std::uint8_t>((b[i/8]<<1u)|(best_bits[i]?1u:0u));std::uint32_t id=(static_cast<std::uint32_t>(b[0])<<16u)|(static_cast<std::uint32_t>(b[1])<<8u)|b[2];std::uint16_t received=static_cast<std::uint16_t>((b[3]<<8u)|b[4]);if(id==0||id>0x00ffffffu||received!=crc16(id))return std::nullopt;return YurlapsFrame{id};}
} // namespace yurlaps::decoder
