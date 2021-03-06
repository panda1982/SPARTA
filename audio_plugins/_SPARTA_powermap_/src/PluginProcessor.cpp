/*
 ==============================================================================
 
 This file is part of SPARTA; a suite of spatial audio plug-ins.
 Copyright (c) 2018 - Leo McCormack.
 
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
    ringBufferInputs = new float*[MAX_NUM_CHANNELS];
    for (int i = 0; i < MAX_NUM_CHANNELS; i++)
        ringBufferInputs[i] = new float[FRAME_SIZE];

    powermap_create(&hPm);
}

PluginProcessor::~PluginProcessor()
{
    for (int i = 0; i < MAX_NUM_CHANNELS; ++i) {
        delete[] ringBufferInputs[i];
    }
    delete[] ringBufferInputs;
    powermap_destroy(&hPm);
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
    isPlaying = false;

    for (int i = 0; i < MAX_NUM_CHANNELS; ++i)
        memset(ringBufferInputs[i], 0, FRAME_SIZE*sizeof(float));
     wIdx = 1; rIdx = 1; /* read/write indices for ring buffers */

    powermap_init(hPm, sampleRate);
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
            powermap_analysis( hPm, ringBufferInputs, nNumInputs, FRAME_SIZE, isPlaying);
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
 	XmlElement xml("POWERMAPAUDIOPLUGINSETTINGS_"+String(SH_ORDER));

    xml.setAttribute("powermapMode", powermap_getPowermapMode(hPm));
    xml.setAttribute("covAvgCoeff", powermap_getCovAvgCoeff(hPm));
    for(int i=0; i<powermap_getNumberOfBands(); i++){
        xml.setAttribute("powermapEQ"+String(i), powermap_getPowermapEQ(hPm, i));
        xml.setAttribute("anaOrder"+String(i), powermap_getAnaOrder(hPm, i));
    }
    xml.setAttribute("chOrder", powermap_getChOrder(hPm));
    xml.setAttribute("normType", powermap_getNormType(hPm));
    xml.setAttribute("numSources", powermap_getNumSources(hPm));
    xml.setAttribute("dispFov", powermap_getDispFOV(hPm));
    xml.setAttribute("aspectRatio", powermap_getAspectRatio(hPm));
    xml.setAttribute("powermapAvgCoeff", powermap_getPowermapAvgCoeff(hPm));
    
	copyXmlToBinary(xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr) {
        if (xmlState->hasTagName("POWERMAPAUDIOPLUGINSETTINGS_"+String(SH_ORDER))) {
            if(xmlState->hasAttribute("powermapMode"))
                powermap_setPowermapMode(hPm, xmlState->getIntAttribute("powermapMode", 1));
            if(xmlState->hasAttribute("covAvgCoeff"))
                powermap_setCovAvgCoeff(hPm, (float)xmlState->getDoubleAttribute("covAvgCoeff", 0.5f));
            for(int i=0; i<powermap_getNumberOfBands(); i++){
                if(xmlState->hasAttribute("powermapEQ"+String(i)))
                    powermap_setPowermapEQ(hPm, (float)xmlState->getDoubleAttribute("powermapEQ"+String(i), 0.5f), i);
                if(xmlState->hasAttribute("anaOrder"+String(i)))
                    powermap_setAnaOrder(hPm, xmlState->getIntAttribute("anaOrder"+String(i), 1), i);
            }
            if(xmlState->hasAttribute("chOrder"))
                powermap_setChOrder(hPm, xmlState->getIntAttribute("chOrder", 1));
            if(xmlState->hasAttribute("normType"))
                powermap_setNormType(hPm, xmlState->getIntAttribute("normType", 1));
            if(xmlState->hasAttribute("numSources"))
                powermap_setNumSources(hPm, xmlState->getIntAttribute("numSources", 1));
            if(xmlState->hasAttribute("dispFov"))
                powermap_setDispFOV(hPm, xmlState->getIntAttribute("dispFov", 1));
            if(xmlState->hasAttribute("aspectRatio"))
                powermap_setAspectRatio(hPm, xmlState->getIntAttribute("aspectRatio", 1));
            if(xmlState->hasAttribute("powermapAvgCoeff"))
                powermap_setPowermapAvgCoeff(hPm, (float)xmlState->getDoubleAttribute("powermapAvgCoeff", 0.5f));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

