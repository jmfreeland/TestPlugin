/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <fdeep/fdeep.hpp>
#include <chrono>
#include <ctime>
#include <fstream>

//==============================================================================
TestPluginAudioProcessor::TestPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    mNetworkModel { fdeep::load_model("D:/projects/testPlugins/fdeep_Linear_model_multi.json", true, fdeep::dev_null_logger) },
    processVector { std::vector<float>(1000, 0) },
    bufferValue { std::vector<float>(256, 0) },
    testValue { std::vector<float>(256, 0) },
    mapPosition { std::vector<float>(1000, 0) }
{
    std::ifstream configFile{ "config.txt", std::ios::in };
    configFile >> modelFile;
    configFile >> numSteps;
}

TestPluginAudioProcessor::~TestPluginAudioProcessor()
{
}

//==============================================================================
const juce::String TestPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TestPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TestPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TestPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TestPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TestPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TestPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TestPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TestPluginAudioProcessor::getProgramName(int index)
{
    return {};
}

void TestPluginAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void TestPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampleHistory.setSize(2, 1256);
    //*mNetworkModel = fdeep::load_model("D:\Projects\TestPlugins\fdeep_Linear_model_multi.json");
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void TestPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TestPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void TestPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int bufferLength = buffer.getNumSamples();
    const int neuralBufferLength = mSampleHistory.getNumSamples();

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        
        auto* channelData = buffer.getWritePointer(channel);
        //const float* bufferData = buffer.getReadPointer(channel);
        const float* neuralBufferData = mSampleHistory.getReadPointer(channel);

        //copy data from main buffer to neural buffer
        if (neuralBufferLength > bufferLength + mWritePosition)
        {
            mSampleHistory.copyFrom(channel, mWritePosition, channelData, bufferLength);
        }

        else
        {
            const int bufferRemaining = neuralBufferLength - mWritePosition;
            mSampleHistory.copyFrom(channel, mWritePosition, channelData, bufferRemaining);
            mSampleHistory.copyFrom(channel, 0, channelData + bufferRemaining, bufferLength - bufferRemaining);
        }

   
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            int start = (mWritePosition + sample + 256) % neuralBufferLength;
            int end = (mWritePosition + sample);
            int stranded = end % neuralBufferLength;

            
            if (start < bufferLength) //should move form 1000 to parameter
            {
               
                processVector.assign(neuralBufferData + start, neuralBufferData + end);
            }

            else if (start < end)
            {
                processVector.assign(neuralBufferData + start, neuralBufferData + neuralBufferLength);
                processVector.insert(processVector.end(), neuralBufferData, neuralBufferData + stranded);
            }

            else if (start > end)
            {
                processVector.assign(neuralBufferData + start, neuralBufferData + neuralBufferLength);
                processVector.insert(processVector.end(), neuralBufferData, neuralBufferData + stranded);
                //std::vector<float> test_vector(neuralBufferData + mWritePosition + sample, neuralBufferData + mWritePosition + sample + remaining );
                //std::vector<float> test_vector(neuralBufferData + mWritePosition + sample, neuralBufferData + (neuralBufferLength - 1 - (mWritePosition + sample)));
                //test_vector.insert(test_vector.end(), neuralBufferData, neuralBufferData + (1000 - (neuralBufferLength - mWritePosition - sample)));
            }
                      
      
            //channelData[sample] = channelData[sample] * mGain;
            //bufferValue[sample] = channelData[sample];
            mStart = std::chrono::high_resolution_clock::now();
            channelData[sample] = mNetworkModel.predict_single_output({ fdeep::tensor(fdeep::tensor_shape(static_cast<std::size_t>(1000)), processVector) });
            mEnd = std::chrono::high_resolution_clock::now();
            mDuration = std::chrono::duration_cast<std::chrono::duration<double>>(mEnd - mStart);
        } 

        //move forward
        mIteration++;
        

    }
    mWritePosition += bufferLength;
    mWritePosition %= neuralBufferLength;

}

//==============================================================================
bool TestPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TestPluginAudioProcessor::createEditor()
{
    return new TestPluginAudioProcessorEditor (*this);
}

//==============================================================================
void TestPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void TestPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestPluginAudioProcessor();
}
