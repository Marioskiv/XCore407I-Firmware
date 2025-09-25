#include "auth.h"
#include <string.h>

/* Minimal SHA256 implementation (public domain style) */
typedef struct { uint32_t s[8]; uint64_t bits; uint8_t buf[64]; } sha256_ctx;

static uint32_t ROR(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }
static void sha256_init(sha256_ctx *c){
    c->s[0]=0x6a09e667u; c->s[1]=0xbb67ae85u; c->s[2]=0x3c6ef372u; c->s[3]=0xa54ff53au;
    c->s[4]=0x510e527fu; c->s[5]=0x9b05688cu; c->s[6]=0x1f83d9abu; c->s[7]=0x5be0cd19u; c->bits=0; }
static void sha256_transform(sha256_ctx *c,const uint8_t b[64]){
    static const uint32_t K[64]={
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
    uint32_t w[64];
    for(int i=0;i<16;i++){ w[i]= (uint32_t)b[i*4]<<24 | (uint32_t)b[i*4+1]<<16 | (uint32_t)b[i*4+2]<<8 | b[i*4+3]; }
    for(int i=16;i<64;i++){ uint32_t s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3); uint32_t s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
    uint32_t a=c->s[0],b0=c->s[1],c0=c->s[2],d=c->s[3],e=c->s[4],f=c->s[5],g=c->s[6],h=c->s[7];
    for(int i=0;i<64;i++){ uint32_t S1=ROR(e,6)^ROR(e,11)^ROR(e,25); uint32_t ch=(e&f)^((~e)&g); uint32_t t1=h+S1+ch+K[i]+w[i]; uint32_t S0=ROR(a,2)^ROR(a,13)^ROR(a,22); uint32_t maj=(a&b0)^(a&c0)^(b0&c0); uint32_t t2=S0+maj; h=g; g=f; f=e; e=d+t1; d=c0; c0=b0; b0=a; a=t1+t2; }
    c->s[0]+=a; c->s[1]+=b0; c->s[2]+=c0; c->s[3]+=d; c->s[4]+=e; c->s[5]+=f; c->s[6]+=g; c->s[7]+=h; }
static void sha256_update(sha256_ctx *c,const uint8_t *data,size_t len){
    while(len){ size_t r = 64 - (size_t)(c->bits/8 % 64); if(r>len) r=len; memcpy(c->buf + (c->bits/8)%64,data,r); c->bits += (uint64_t)r*8; data+=r; len-=r; if((c->bits/8)%64==0){ sha256_transform(c,c->buf);} }
}
static void sha256_final(sha256_ctx *c,uint8_t out[32])
{
    size_t idx = (size_t)(c->bits/8 % 64);
    c->buf[idx++] = 0x80;
    if (idx > 56) {
        while (idx < 64) {
            c->buf[idx++] = 0;
        }
        sha256_transform(c, c->buf);
        idx = 0;
    }
    while (idx < 56) {
        c->buf[idx++] = 0;
    }
    uint64_t bits_be = c->bits;
    for (int i = 0; i < 8; i++) {
        c->buf[63 - i] = (uint8_t)(bits_be & 0xFFu);
        bits_be >>= 8;
    }
    sha256_transform(c, c->buf);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(c->s[i] >> 24);
        out[i*4+1] = (uint8_t)(c->s[i] >> 16);
        out[i*4+2] = (uint8_t)(c->s[i] >> 8);
        out[i*4+3] = (uint8_t)(c->s[i]);
    }
}

static uint8_t g_key[AUTH_KEY_MAX_LEN];
static size_t  g_key_len = 0;

void auth_set_key(const uint8_t *key, size_t len){ if(len>AUTH_KEY_MAX_LEN) len=AUTH_KEY_MAX_LEN; memcpy(g_key,key,len); g_key_len=len; }

void auth_hmac_tag(const uint8_t *msg, size_t msgLen, uint8_t out_tag[AUTH_TAG_LEN]){
    uint8_t k_ipad[64]; uint8_t k_opad[64]; uint8_t keyblock[64]; memset(keyblock,0,64); memcpy(keyblock,g_key,g_key_len);
    if(g_key_len>64){ sha256_ctx hk; sha256_init(&hk); sha256_update(&hk,g_key,g_key_len); sha256_final(&hk,keyblock); memset(keyblock+32,0,32); }
    for(int i=0;i<64;i++){ k_ipad[i]=keyblock[i]^0x36; k_opad[i]=keyblock[i]^0x5c; }
    uint8_t inner[32]; sha256_ctx ic; sha256_init(&ic); sha256_update(&ic,k_ipad,64); sha256_update(&ic,msg,msgLen); sha256_final(&ic,inner);
    sha256_ctx oc; sha256_init(&oc); sha256_update(&oc,k_opad,64); sha256_update(&oc,inner,32); uint8_t full[32]; sha256_final(&oc,full);
    memcpy(out_tag, full, AUTH_TAG_LEN);
}

int auth_tag_verify(const uint8_t *msg, size_t msgLen, const uint8_t *tag){
    uint8_t calc[AUTH_TAG_LEN]; auth_hmac_tag(msg,msgLen,calc);
    uint8_t diff = 0; for(size_t i=0;i<AUTH_TAG_LEN;i++) diff |= (uint8_t)(calc[i]^tag[i]);
    return diff==0; }
