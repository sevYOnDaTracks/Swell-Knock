#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <cmath>
#include <vector>
constexpr int rate=48000,bs=256;
bool save(const juce::File& f,const std::vector<float>& data){juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());if(!stream)return false;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),rate,1,24,{},0));if(!writer)return false;stream.release();const float* ptr=data.data();return writer->writeFromFloatArrays(&ptr,1,(int)data.size());}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI init;if(argc!=3)return 1;
 juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> types;format.findAllTypesForFile(types,argv[1]);if(types.isEmpty()){std::cerr<<"VST3 not found";return 2;}
 juce::AudioPluginFormatManager manager;manager.addFormat(new juce::VST3PluginFormat());juce::String error;
 auto plugin=manager.createPluginInstance(*types[0],rate,bs,error);if(!plugin){std::cerr<<error;return 3;}
 juce::AudioProcessorParameter* amount=nullptr;for(auto* p:plugin->getParameters())if(p->getName(64)=="Knock")amount=p;if(!amount)return 4;
 amount->setValueNotifyingHost(.7f);plugin->setNonRealtime(true);plugin->prepareToPlay(rate,bs);
 // Original synthetic kick. A damped sine pitch sweep and a seeded short attack.
 const int phraseSamples=rate*4;std::vector<float> dry(phraseSamples);juce::Random random(20261103);
 for(int hit=0;hit<8;++hit){double phase=0;for(int n=0;n<rate/2;++n){double t=(double)n/rate;double hz=48+105*std::exp(-t/0.018);phase+=2*juce::MathConstants<double>::pi*hz/rate;double env=(1-std::exp(-t/.0007))*std::exp(-t/.10);dry[hit*rate/2+n]=float(.63*std::sin(phase)*env+.045*(random.nextFloat()*2-1)*std::exp(-t/.004));}}
 // Warm up parameter smoothers before rendering, then compensate reported latency.
 juce::AudioBuffer<float> b(2,bs);juce::MidiBuffer midi;for(int i=0;i<100;++i){b.clear();plugin->processBlock(b,midi);}
 int latency=plugin->getLatencySamples();std::vector<float> raw(phraseSamples+latency+bs);
 for(int offset=0;offset<(int)raw.size();offset+=bs){b.clear();for(int i=0;i<bs;++i)for(int c=0;c<2;++c)if(offset+i<phraseSamples)b.setSample(c,i,dry[offset+i]);plugin->processBlock(b,midi);for(int i=0;i<bs&&offset+i<(int)raw.size();++i)raw[offset+i]=b.getSample(0,i);}
 std::vector<float> wet(phraseSamples);double dryEnergy=0,wetEnergy=0,difference=0;for(int i=0;i<phraseSamples;++i){wet[i]=raw[i+latency];if(!std::isfinite(wet[i]))return 5;dryEnergy+=dry[i]*dry[i];wetEnergy+=wet[i]*wet[i];difference+=std::abs(wet[i]-dry[i]);}if(wetEnergy<1e-10||difference<.1)return 6;
 double matching=std::sqrt(dryEnergy/wetEnergy);float peak=0;for(int i=0;i<phraseSamples;++i){wet[i]*=(float)matching;peak=std::max(peak,std::max(std::abs(wet[i]),std::abs(dry[i])));}float commonGain=.8f/std::max(peak,.8f);for(int i=0;i<phraseSamples;++i){dry[i]*=commonGain;wet[i]*=commonGain;}
 juce::File folder(argv[2]);folder.createDirectory();std::vector<float> comparison;for(int pass=0;pass<2;++pass){comparison.insert(comparison.end(),dry.begin(),dry.end());comparison.insert(comparison.end(),wet.begin(),wet.end());}
 if(!save(folder.getChildFile("01-kick-avant.wav"),dry)||!save(folder.getChildFile("02-kick-apres-knock-70.wav"),wet)||!save(folder.getChildFile("03-comparaison-avant-apres.wav"),comparison))return 7;
 juce::String info="Swell Knock 0.1.0 — demonstration\nKick synthetique original, 120 BPM. Vrai VST3 Windows charge et rendu hors ligne.\nKnock: 70 %. Mono WAV 48 kHz / 24 bits.\nComparaison: 0-4 s avant, 4-8 s apres, 8-12 s avant, 12-16 s apres.\nNiveaux RMS egalises (pas une mesure LUFS). Compensation apres: "+juce::String(20*std::log10(matching),2)+" dB.\nLatence compensee: "+juce::String(latency)+" echantillons.\n";
 folder.getChildFile("LISEZ-MOI.txt").replaceWithText(info);std::cout<<info;plugin->releaseResources();return 0;
}
