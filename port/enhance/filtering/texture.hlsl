// Q8 four-tap filtering after CLUT lookup. Compose each tap using its own
// opaque/semi-transparent flag against the same destination, then combine.
uint hm_filter_tap(uint p,uint sample_color,uint3 light,int2 logical,uint background){
    uint op=word(p,1),flags=word(p,4),source=sample_color;
    if(!(op&1)){
        int3 value=int3(light),tex=int3(sample_color&31,(sample_color>>5)&31,(sample_color>>10)&31);
        if(word(p,0)==0){value=value*tex/16;if(flags&1)value+=dither(logical);value=clamp(value,0,255)>>3;}
        else value=min(tex*value/128,31);
        source=(sample_color&32768)|uint(value.x|(value.y<<5)|(value.z<<10));
    }
    if((op&2)&&(sample_color&32768)){
        int3 back=int3(background&31,(background>>5)&31,(background>>10)&31);
        int3 front=int3(source&31,(source>>5)&31,(source>>10)&31);
        uint mode=(word(p,2)>>5)&3;
        int3 mixed=mode==0?back/2+front/2:mode==1?back+front:mode==2?back-front:back+front/4;
        mixed=clamp(mixed,0,31);source=(source&32768)|uint(mixed.x|(mixed.y<<5)|(mixed.z<<10));
    }
    return source;
}
uint hm_filter_compose(uint p,int2 coord,uint4 bounds,uint3 light,int2 logical,uint background,uint filter_mode){
    coord=clamp(coord,int2(bounds.xy)*256,int2(bounds.zw)*256);
    uint2 low=uint2(coord)>>8,high=min(low+1,bounds.zw),fraction=uint2(coord)&255;
    uint taps[4]={texel(p,low),texel(p,uint2(high.x,low.y)),texel(p,uint2(low.x,high.y)),texel(p,high)};
    uint weights[4];
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
    uint3 sum=0;uint strongest=0,mask=0;bool covered=false;
    [unroll]for(uint tap=0;tap<4;++tap){uint color=background;
        if(taps[tap]!=0&&weights[tap]!=0){covered=true;color=hm_filter_tap(p,taps[tap],light,logical,background);
            if(weights[tap]>strongest){strongest=weights[tap];mask=taps[tap]&32768;}}
        sum+=uint3(color&31,(color>>5)&31,(color>>10)&31)*weights[tap];
    }
    if(!covered)return background;
    sum=(sum+32768)>>16;
    return mask|((word(p,4)&4)?32768:0)|sum.x|(sum.y<<5)|(sum.z<<10);
}
uint hm_filter_uv_i(uint3 values,int3 w,int area){
    uint numerator=uint(w.x)*values.x+uint(w.y)*values.y+uint(w.z)*values.z;
    uint whole=numerator/uint(area),rest=numerator-whole*uint(area);
    return whole*256+(rest*256+uint(area/2))/uint(area);
}
uint hm_filter_uv_f(uint3 values,float3 w,float area){return uint(floor(dot(float3(values),w)/area*256+0.5));}
