// Q8 four-tap filtering after CLUT lookup. Compose each tap using its own
// opaque/semi-transparent flag against the same destination, then combine.
bool hm_filter_prepare(uint p,int2 coord,uint4 bounds,uint3 light,int2 logical,uint filter_mode,
                       out uint taps[4],out uint weights[4],out uint sources[4],out uint out_mask){
    coord=clamp(coord,int2(bounds.xy)*256,int2(bounds.zw)*256);
    uint2 low=uint2(coord)>>8,high=min(low+1,bounds.zw),fraction=uint2(coord)&255;
    taps[0]=texel(p,low);taps[1]=texel(p,uint2(high.x,low.y));
    taps[2]=texel(p,uint2(low.x,high.y));taps[3]=texel(p,high);
    if(filter_mode==2){
        float2 f=float2(fraction)/255.0;
        float w=0.18;
        f=smoothstep(0.5-w,0.5+w,f);
        uint cA=taps[0],cB=taps[1],cC=taps[2],cD=taps[3];
        if(cA==cD&&cB!=cC){
            float d=f.x+f.y-1.0;
            f=lerp(f,float2(d>0.0?1.0:0.0,d>0.0?1.0:0.0),0.35);
        }else if(cB==cC&&cA!=cD){
            float d=f.x-f.y;
            f=lerp(f,float2(d>0.0?1.0:0.0,d>0.0?0.0:1.0),0.35);
        }
        uint2 sf=uint2(clamp(f*255.0,0.0,255.0));
        weights[0]=(256-sf.x)*(256-sf.y);weights[1]=sf.x*(256-sf.y);
        weights[2]=(256-sf.x)*sf.y;weights[3]=sf.x*sf.y;
    }else{
        weights[0]=(256-fraction.x)*(256-fraction.y);weights[1]=fraction.x*(256-fraction.y);
        weights[2]=(256-fraction.x)*fraction.y;weights[3]=fraction.x*fraction.y;
    }
    uint op=word(p,1),flags=word(p,4),strongest=0,mask=0;
    bool covered=false,is_tri=(word(p,0)==0);
    int dither_val=((flags&1)&&is_tri&&!(op&1))?dither(logical):0;
    [unroll]for(uint tap=0;tap<4;++tap){
        uint sc=taps[tap],src=sc;
        if(sc!=0&&weights[tap]!=0){
            covered=true;
            if(!(op&1)){
                int3 value=int3(light),tex=int3(sc&31,(sc>>5)&31,(sc>>10)&31);
                if(is_tri){value=value*tex/16+dither_val;value=clamp(value,0,255)>>3;}
                else value=min(tex*value/128,31);
                src=(sc&32768)|uint(value.x|(value.y<<5)|(value.z<<10));
            }
            if(weights[tap]>strongest){strongest=weights[tap];mask=sc&32768;}
        }
        sources[tap]=src;
    }
    out_mask=mask|((flags&4)?32768:0);
    return covered;
}
uint hm_filter_blend(uint p,uint taps[4],uint weights[4],uint sources[4],uint out_mask,uint background){
    uint op=word(p,1),mode=(word(p,2)>>5)&3;
    int3 back=int3(background&31,(background>>5)&31,(background>>10)&31);
    uint3 sum=0;
    [unroll]for(uint tap=0;tap<4;++tap){
        uint color=background;
        if(taps[tap]!=0&&weights[tap]!=0){
            color=sources[tap];
            if((op&2)&&(taps[tap]&32768)){
                int3 front=int3(color&31,(color>>5)&31,(color>>10)&31);
                int3 mixed=mode==0?back/2+front/2:mode==1?back+front:mode==2?back-front:back+front/4;
                mixed=clamp(mixed,0,31);color=(color&32768)|uint(mixed.x|(mixed.y<<5)|(mixed.z<<10));
            }
        }
        sum+=uint3(color&31,(color>>5)&31,(color>>10)&31)*weights[tap];
    }
    sum=(sum+32768)>>16;
    return out_mask|sum.x|(sum.y<<5)|(sum.z<<10);
}
uint hm_filter_uv_i(uint3 values,int3 w,int area){
    uint numerator=uint(w.x)*values.x+uint(w.y)*values.y+uint(w.z)*values.z;
    uint whole=numerator/uint(area),rest=numerator-whole*uint(area);
    return whole*256+(rest*256+uint(area/2))/uint(area);
}
uint hm_filter_uv_f(uint3 values,float3 w,float area){return uint(floor(dot(float3(values),w)/area*256+0.5));}
