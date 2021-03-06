/*
 ==============================================================================
 
 This file is part of SPARTA; a suite of spatial audio plug-ins.
 Copyright (c) 2017/2018 - Leo McCormack.
 
 SPARTA is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 SPARTA is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with SPARTA.  If not, see <http://www.gnu.org/licenses/>.
 
 ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
{
	nHostBlockSize = FRAME_SIZE;
	nSampleRate = 48000;

	ringBufferInputs = new float*[MAX_NUM_CHANNELS];
	for (int i = 0; i < MAX_NUM_CHANNELS; i++)
		ringBufferInputs[i] = new float[FRAME_SIZE];
 
	sldoa_create(&hSld);
}

PluginProcessor::~PluginProcessor()
{
	sldoa_destroy(&hSld);

	for (int i = 0; i < MAX_NUM_CHANNELS; ++i) {
		delete[] ringBufferInputs[i];
	}
	delete[] ringBufferInputs;
}

void PluginProcessor::setParameter (int index, float newValue)
{
	switch (index)
	{
		default: break;
	}
}

void PluginProcessor::setCurrentProgram (int index)
{
}

float PluginProcessor::getParameter (int index)
{
    switch (index)
	{
		default: return 0.0f;
	}
}

int PluginProcessor::getNumParameters()
{
	return k_NumOfParameters;
}

const String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

const String PluginProcessor::getParameterName (int index)
{
    switch (index)
	{
		default: return "NULL";
	}
}

const String PluginProcessor::getParameterText(int index)
{
	return String(getParameter(index), 1);    
}

const String PluginProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String PluginProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 0;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

const String PluginProcessor::getProgramName (int index)
{
    return String::empty;
}

bool PluginProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool PluginProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

void PluginProcessor::changeProgramName (int index, const String& newName)
{
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	nHostBlockSize = samplesPerBlock;
	nSampleRate = (int)(sampleRate + 0.5);

    nNumInputs = getNumInputChannels();

	setPlayConfigDetails(nNumInputs, 0, (double)nSampleRate, nHostBlockSize);
	numChannelsChanged(); 

	for (int i = 0; i < MAX_NUM_CHANNELS; ++i)
		memset(ringBufferInputs[i], 0, FRAME_SIZE * sizeof(float));
	
	isPlaying = false; 
	sldoa_init(hSld, sampleRate);
}

void PluginProcessor::releaseResources()
{
    isPlaying = false;
}

void PluginProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
	int nCurrentBlockSize = buffer.getNumSamples();
	float** bufferData = buffer.getArrayOfWritePointers();
 
    if(nCurrentBlockSize % FRAME_SIZE == 0){ /* divisible by frame size */
        for (int frame = 0; frame < nCurrentBlockSize/FRAME_SIZE; frame++) {
            for (int ch = 0; ch < nNumInputs; ch++)
                for (int i = 0; i < FRAME_SIZE; i++)
                    ringBufferInputs[ch][i] = bufferData[ch][frame*FRAME_SIZE + i];
            
            /* determine if there is actually audio in the damn buffer */
            playHead = getPlayHead();
            bool PlayHeadAvailable = playHead->getCurrentPosition(currentPosition);
            if (PlayHeadAvailable == true)
                isPlaying = currentPosition.isPlaying;
            else
                isPlaying = false;
            
            /* perform processing */
            sldoa_analysis(hSld, ringBufferInputs, nNumInputs, FRAME_SIZE, isPlaying);
        }
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; 
}

AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (this);
}

//==============================================================================
void PluginProcessor::getStateInformation (MemoryBlock& destData)
{
	XmlElement xml("SLDOAAUDIOPLUGINSETTINGS_O" + String(SH_ORDER));

	/* add attributes */
    xml.setAttribute("MaxFreq", sldoa_getMaxFreq(hSld));
    xml.setAttribute("MinFreq", sldoa_getMinFreq(hSld));
    xml.setAttribute("Avg", sldoa_getAvg(hSld));
    for(int i=0; i<sldoa_getNumberOfBands(); i++)
        xml.setAttribute("AnaOrder" + String(i), sldoa_getAnaOrder(hSld, i));
    xml.setAttribute("ChOrder", (int)sldoa_getChOrder(hSld));
    xml.setAttribute("Norm", (int)sldoa_getNormType(hSld));
    
	copyXmlToBinary(xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	ScopedPointer<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr) {
        /* make sure that it's actually the correct type of XML object */
        if (xmlState->hasTagName("SLDOAAUDIOPLUGINSETTINGS_O" + String(SH_ORDER))) {
            /* pull attributes */
            if(xmlState->hasAttribute("MaxFreq"))
                sldoa_setMaxFreq(hSld, (float)xmlState->getDoubleAttribute("MaxFreq", 5000.0));
            if(xmlState->hasAttribute("MinFreq"))
                sldoa_setMinFreq(hSld, (float)xmlState->getDoubleAttribute("MinFreq", 500.0));
            if(xmlState->hasAttribute("Avg"))
                sldoa_setAvg(hSld, (float)xmlState->getDoubleAttribute("Avg", 100.0));
            for(int i=0; i<sldoa_getNumberOfBands(); i++){
                if(xmlState->hasAttribute("AnaOrder"+String(i)))
                    sldoa_setAnaOrder(hSld, (float)xmlState->getIntAttribute("AnaOrder"+String(i), SH_ORDER), i);
            }
            if(xmlState->hasAttribute("ChOrder"))
                sldoa_setChOrder(hSld, xmlState->getIntAttribute("ChOrder", 1));
            if(xmlState->hasAttribute("Norm"))
                sldoa_setNormType(hSld, xmlState->getIntAttribute("Norm", 1));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

