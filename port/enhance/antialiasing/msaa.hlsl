int2 hm_aa_offset(uint sample_index,uint samples){
    if(samples==2)return sample_index==0?int2(64,64):int2(192,192);
    const int2 offsets[4]={int2(96,32),int2(224,96),int2(32,160),int2(160,224)};
    return samples==4?offsets[sample_index]:int2(128,128);
}
uint hm_aa_triangle(int2 a,int2 b,int2 c,uint2 pixel,uint samples){
    uint coverage=0;
    [unroll]for(uint i=0;i<4;++i){if(i>=samples)break;
        int2 p=int2(pixel)*256+hm_aa_offset(i,samples);
        float3 approximate=float3(edge_f(float2(b),float2(c),float2(p)),
            edge_f(float2(c),float2(a),float2(p)),edge_f(float2(a),float2(b),float2(p)))/65536.0;
        int3 signs=int3(hm_edge_sign(b,c,p,approximate.x),hm_edge_sign(c,a,p,approximate.y),hm_edge_sign(a,b,p,approximate.z));
        if(all(signs>=0)&&(signs.x!=0||top_left(b,c))&&(signs.y!=0||top_left(c,a))&&(signs.z!=0||top_left(a,b)))coverage|=1u<<i;
    }
    return coverage;
}
uint hm_aa_rect(int2 low,int2 high,uint2 pixel,uint samples){
    uint coverage=0;
    [unroll]for(uint i=0;i<4;++i){if(i>=samples)break;
        int2 p=int2(pixel)*256+hm_aa_offset(i,samples);
        if(all(p>=low)&&all(p<high))coverage|=1u<<i;
    }
    return coverage;
}
float2 hm_aa_center(uint2 pixel,uint coverage,uint samples){
    int2 sum=0;uint count=0;
    [unroll]for(uint i=0;i<4;++i){if(i>=samples)break;
        if(coverage&(1u<<i)){sum+=hm_aa_offset(i,samples);++count;}}
    return float2(pixel)+float2(sum)/(float(count)*256.0);
}
