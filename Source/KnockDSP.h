#pragma once
#include <algorithm>
#include <cmath>
// Stereo-linked envelope preserves image. No allocations in the audio callback.
class KnockDSP {
 double rate=48000; float fast=0,slow=0,low[2]={}, fa=0,fr=0,sa=0,sr=0,lc=0;
 static float coeff(double ms,double r){return float(std::exp(-1.0/(ms*.001*r)));}
public:
 void prepare(double r){rate=r;fa=coeff(.3,r);fr=coeff(18,r);sa=coeff(25,r);sr=coeff(100,r);lc=float(1-std::exp(-2*3.141592653589793*150/r));reset();}
 void reset(){fast=slow=0;low[0]=low[1]=0;}
 void tick(float* x,int channels,float amount){
  float peak=0;for(int c=0;c<channels;++c)peak=std::max(peak,std::abs(x[c]));
  float f=peak>fast?fa:fr,s=peak>slow?sa:sr;
  fast=f*fast+(1-f)*peak;slow=s*slow+(1-s)*peak;
  const float a=std::clamp(amount,0.f,1.f);
  const float transient=std::clamp((fast-slow)/(slow+.025f),0.f,1.f);
  const float gain=1+1.4f*a*transient;
  const float drive=1+2.6f*a*a;
  const float compensation=1/std::sqrt(drive);
  for(int c=0;c<channels;++c){float dry=x[c];low[c]+=lc*(dry-low[c]);float shaped=(dry*gain+low[c]*a*.28f)*drive;
   // Smooth saturation followed by an increasingly firm ceiling.
   float wet=std::tanh(shaped)*compensation;
   x[c]=dry+(wet-dry)*a;
  }
 }
};
