#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static const int ima_index_table[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};
static const int ima_step_table[89] = {
7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,
449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,
2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,
7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
24623,27086,29794,32767};

static int clamp_s16(int v){ if(v<-32768)return -32768; if(v>32767)return 32767; return v; }
static int16_t rd_s16le(const unsigned char*p){ return (int16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8)); }
static void wr16le(unsigned char*p,unsigned v){ p[0]=(unsigned char)(v&255u); p[1]=(unsigned char)((v>>8)&255u); }
static void wr32le(unsigned char*p,uint32_t v){ p[0]=(unsigned char)(v&255u); p[1]=(unsigned char)((v>>8)&255u); p[2]=(unsigned char)((v>>16)&255u); p[3]=(unsigned char)((v>>24)&255u); }

static unsigned encode_nibble(int sample,int*predictor,int*index){
    int step=ima_step_table[*index]; int diff=sample-*predictor; unsigned nibble=0;
    if(diff<0){ nibble=8; diff=-diff; }
    int delta=step>>3;
    if(diff>=step){ nibble|=4; diff-=step; delta+=step; }
    int step2=step>>1; if(diff>=step2){ nibble|=2; diff-=step2; delta+=step2; }
    int step4=step>>2; if(diff>=step4){ nibble|=1; delta+=step4; }
    if(nibble&8)*predictor-=delta; else *predictor+=delta;
    *predictor=clamp_s16(*predictor); *index+=ima_index_table[nibble&15];
    if(*index<0)*index=0; if(*index>88)*index=88; return nibble&15;
}

static size_t fread_full(unsigned char*buf,size_t want){
    size_t got=0; while(got<want){ size_t n=fread(buf+got,1,want-got,stdin); if(n==0)break; got+=n; } return got;
}

int main(int argc,char**argv){
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    if(argc!=3){ fprintf(stderr,"usage: %s SAMPLES_PER_FRAME START_FRAME\n",argv[0]); return 2; }
    long spf_l=strtol(argv[1],NULL,10); long frame_l=strtol(argv[2],NULL,10);
    if(spf_l<=0||spf_l>65535||frame_l<0){ fprintf(stderr,"invalid samples_per_frame/start_frame\n"); return 2; }
    uint32_t spf=(uint32_t)spf_l; uint32_t frame_no=(uint32_t)frame_l;
    size_t pcm_bytes=(size_t)spf*2u; size_t adpcm_bytes=(spf>0)?((spf-1u+1u)/2u):0u; size_t body_bytes=20u+adpcm_bytes;
    unsigned char*pcm=(unsigned char*)malloc(pcm_bytes); unsigned char*body=(unsigned char*)malloc(body_bytes);
    if(!pcm||!body){ fprintf(stderr,"out of memory\n"); free(pcm); free(body); return 3; }
    for(;;){
        size_t got=fread_full(pcm,pcm_bytes); if(got==0)break;
        if(got!=pcm_bytes){ fprintf(stderr,"partial PCM frame: got %lu of %lu bytes\n",(unsigned long)got,(unsigned long)pcm_bytes); free(pcm); free(body); return 4; }
        int predictor0=rd_s16le(pcm); int predictor=predictor0; int index0=0; int index=0;
        memcpy(body,"IA4M",4); wr32le(body+4,frame_no); wr16le(body+8,spf); body[10]=1; body[11]=0;
        wr16le(body+12,(uint16_t)(int16_t)predictor0); body[14]=(unsigned char)index0; body[15]=0; wr32le(body+16,(uint32_t)adpcm_bytes); memset(body+20,0,adpcm_bytes);
        size_t out_i=0; unsigned pending=0; int have_pending=0;
        for(uint32_t i=1;i<spf;i++){ unsigned nib=encode_nibble(rd_s16le(pcm+(size_t)i*2u),&predictor,&index); if(!have_pending){ pending=nib; have_pending=1; } else { body[20+out_i++]=(unsigned char)(pending|(nib<<4)); have_pending=0; pending=0; } }
        if(have_pending) body[20+out_i++]=(unsigned char)pending;
        if(out_i!=adpcm_bytes){ fprintf(stderr,"internal adpcm size mismatch\n"); free(pcm); free(body); return 5; }
        unsigned char lenbuf[4]; wr32le(lenbuf,(uint32_t)body_bytes);
        if(fwrite(lenbuf,1,4,stdout)!=4||fwrite(body,1,body_bytes,stdout)!=body_bytes){ fprintf(stderr,"stdout write failed\n"); free(pcm); free(body); return 6; }
        fflush(stdout); frame_no++;
    }
    free(pcm); free(body); return 0;
}
