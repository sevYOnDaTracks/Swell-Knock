#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "BinaryData.h"
#include "KnockDSP.h"
class KnockProcessor;
class KnockLook : public juce::LookAndFeel_V4 {
public:
 void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&) override {
  auto r=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(9);auto centre=r.getCentre();float radius=r.getWidth()/2;
  juce::Path track,arc;track.addCentredArc(centre.x,centre.y,radius-5,radius-5,0,start,end,true);arc.addCentredArc(centre.x,centre.y,radius-5,radius-5,0,start,start+pos*(end-start),true);
  g.setColour(juce::Colour(0xff34353b));g.strokePath(track,juce::PathStrokeType(6));g.setColour(juce::Colour(0xffff8a3d));g.strokePath(arc,juce::PathStrokeType(6));
  auto face=r.reduced(25);g.setGradientFill(juce::ColourGradient(juce::Colour(0xff303138),face.getTopLeft(),juce::Colour(0xff1b1c20),face.getBottomRight(),false));g.fillEllipse(face);g.setColour(juce::Colour(0xff45464b));g.drawEllipse(face,1);
  float angle=start+pos*(end-start);auto p=centre.getPointOnCircumference(radius-39,angle),q=centre.getPointOnCircumference(radius-58,angle);g.setColour(juce::Colour(0xffff8a3d));g.drawLine({p,q},4);
 }
};
class KnockEditor : public juce::AudioProcessorEditor,private juce::Timer {
 KnockProcessor& proc;KnockLook look;juce::Slider knob;juce::TextButton bypass{"Bypass"};juce::Image logo;
 std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ka;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
 void timerCallback() override {repaint();}
public:
 explicit KnockEditor(KnockProcessor&);
 ~KnockEditor() override {knob.setLookAndFeel(nullptr);}
 void paint(juce::Graphics&) override;
 void resized() override {knob.setBounds(100,178,270,270);bypass.setBounds(352,25,88,35);}
};
class KnockProcessor : public juce::AudioProcessor {
public:
 juce::AudioProcessorValueTreeState state;
 juce::SmoothedValue<float> amount,enabled;
 KnockDSP dsp;
 std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
 juce::dsp::DryWetMixer<float> mixer{64};
 KnockProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"Knock",layout()){}
 static juce::AudioProcessorValueTreeState::ParameterLayout layout(){juce::AudioProcessorValueTreeState::ParameterLayout l;l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"knock",1},"Knock",juce::NormalisableRange<float>(0,100,.1f),0));l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));return l;}
 const juce::String getName() const override{return "Swell Knock";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}double getTailLengthSeconds()const override{return 0;}
 bool isBusesLayoutSupported(const BusesLayout& l)const override{return (l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo())&&l.getMainOutputChannelSet()==l.getMainInputChannelSet();}
 void prepareToPlay(double sr,int block) override {
  auto channels=(size_t)getTotalNumInputChannels();oversampling=std::make_unique<juce::dsp::Oversampling<float>>(channels,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);oversampling->initProcessing((size_t)block);
  auto latency=oversampling->getLatencyInSamples();setLatencySamples((int)latency);mixer.prepare({sr,(juce::uint32)block,(juce::uint32)channels});mixer.setWetLatency(latency);mixer.setMixingRule(juce::dsp::DryWetMixingRule::linear);mixer.reset();
  dsp.prepare(sr*4);amount.reset(sr*4,.03);amount.setCurrentAndTargetValue(state.getRawParameterValue("knock")->load()/100);enabled.reset(sr,.015);enabled.setCurrentAndTargetValue(state.getRawParameterValue("bypass")->load()>.5f?0.f:1.f);
 }
 void releaseResources() override{}
 void render(juce::AudioBuffer<float>& b,bool hostBypass) {
  juce::ScopedNoDenormals noDenormals;if(b.getNumSamples()==0)return;
  juce::dsp::AudioBlock<float> block(b);mixer.pushDrySamples(block);amount.setTargetValue(state.getRawParameterValue("knock")->load()/100);
  // Mixer smooths bypass transitions and aligns dry audio with oversampling latency.
  mixer.setWetMixProportion(hostBypass||state.getRawParameterValue("bypass")->load()>.5f||state.getRawParameterValue("knock")->load()==0?0.f:1.f);
  auto up=oversampling->processSamplesUp(block);for(size_t i=0;i<up.getNumSamples();++i){float frame[2]{};for(size_t c=0;c<up.getNumChannels();++c)frame[c]=up.getSample((int)c,(int)i);dsp.tick(frame,(int)up.getNumChannels(),amount.getNextValue());for(size_t c=0;c<up.getNumChannels();++c)up.setSample((int)c,(int)i,frame[c]);}
  oversampling->processSamplesDown(block);mixer.mixWetSamples(block);
 }
 juce::AudioProcessorParameter* getBypassParameter() const override{return state.getParameter("bypass");}
 void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override{render(b,false);}
 void processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override{render(b,true);}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override{return new KnockEditor(*this);}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock& out)override{auto xml=state.copyState().createXml();copyXmlToBinary(*xml,out);}
 void setStateInformation(const void* data,int size)override{auto xml=getXmlFromBinary(data,size);if(xml&&xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
};
KnockEditor::KnockEditor(KnockProcessor& p):AudioProcessorEditor(p),proc(p){setSize(470,550);logo=juce::ImageCache::getFromMemory(BinaryData::swellmark_png,BinaryData::swellmark_pngSize);knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);knob.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);knob.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);knob.setDoubleClickReturnValue(true,0);knob.setLookAndFeel(&look);knob.setName("Knock intensity");addAndMakeVisible(knob);bypass.setClickingTogglesState(true);bypass.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff25262b));bypass.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffff8a3d));addAndMakeVisible(bypass);ka=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.state,"knock",knob);ba=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.state,"bypass",bypass);startTimerHz(20);}
void KnockEditor::paint(juce::Graphics& g){g.fillAll(juce::Colour(0xff1b1c20));g.setColour(juce::Colour(0xff34353b));g.drawHorizontalLine(85,0,470);g.drawImageWithin(logo,30,28,32,29,juce::RectanglePlacement::centred);g.setColour(juce::Colour(0xffeeeef1));g.setFont(juce::FontOptions(25.f,juce::Font::bold));g.drawText("swell",74,20,130,32,juce::Justification::left);g.setFont(juce::FontOptions(10.f));g.drawText("AUDIO",75,51,120,15,juce::Justification::left);g.setFont(juce::FontOptions(56.f,juce::Font::bold));g.drawText("KNOCK",0,105,470,65,juce::Justification::centred);
 g.setColour(juce::Colour(0xff9a9ba3));g.setFont(juce::FontOptions(13.f));g.drawText("Un bouton. Plus d'impact.",0,167,470,20,juce::Justification::centred);
 // Values are painted above the knob by a child-independent central label area below.
 g.setColour(juce::Colour(0xffff8a3d));g.setFont(juce::FontOptions(23.f,juce::Font::bold));g.drawText(juce::String((int)knob.getValue())+" %",0,450,470,30,juce::Justification::centred);g.setFont(juce::FontOptions(13.f));g.setColour(juce::Colour(0xff9a9ba3));g.drawText(bypass.getToggleState()?"Bypass":knob.getValue()==0?"Clean":knob.getValue()<34?"Defined":knob.getValue()<70?"Punchy":"Hard",0,490,470,25,juce::Justification::centred);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new KnockProcessor();}
