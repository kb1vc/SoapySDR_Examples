#include <SoapySDR/Device.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Errors.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#define _USE_MATH_DEFINES
#include <cmath>

const char * start_message[3] = { "1kHz CW Upper Sideband -- \n\tnotice 3rd order spur 3kHz below nominal carrier.",
			    "Zero amplitude envelope. Slight chirp at start of interval, \n\tperhaps leftover buffer from 1kHz tone.", 
			    "2kHz CW Upper Sideband -- \n\tnotice 3rd order spur 6kHz below nominal carrier."
}; 
			    
const char * end_message[3] = {   "End of 1kHz tone - no modulation present\n\tdead carrier amplitude is from last transmitted sample.",
			    "",
			    "End of 2kHz tone - no modulation present\n\tdead carrier amplitude is from last transmitted sample."
};
			      
int main()
{
  SoapySDR::Device * dev; 
  int stat; 

  // set the clock to allow a 625 k sample/sec sample rate, as SoDaRadio does. 
  double master_clock_rate = 40.0e6; 
  // set the sample rate
  double sample_rate = 625000.0; 

  SoapySDR::KwargsList kwl = SoapySDR::Device::enumerate(std::string("driver=lime"));
  if(kwl.size() == 0) {
    std::cerr << "No LimeSDR device was found.\n";
    exit(-1);
  }

  dev = SoapySDR::Device::make(kwl[0]);

  dev->setMasterClockRate(master_clock_rate); 
  dev->setFrequency(SOAPY_SDR_TX, 0, 144.295e6);
  dev->setSampleRate(SOAPY_SDR_TX, 0, sample_rate);
  dev->setFrequency(SOAPY_SDR_RX, 0, 144.295e6);
  dev->setSampleRate(SOAPY_SDR_RX, 0, sample_rate);
  dev->setGain(SOAPY_SDR_TX, 0, 52);
  dev->setAntenna(SOAPY_SDR_TX, 0, "BAND1");

  // make buffers
  int buflen = 30000;     
  std::complex<float> mybuf[3][buflen];
  std::complex<float> * txbufs[1];

  double ang = 0.0; 
  double angincr = 2.0 * M_PI / 625.0; 

  // write a 1kHz and 2kHz sinusoid to buffers 0 and 2.  Write silence to buffer 1
  for(int i = 0; i < buflen; i++) {
    std::complex<float> half(0.5,0.0);
    mybuf[0][i] = half * std::complex<float>(cos(ang), sin(ang)); 
    mybuf[1][i] = std::complex<float>(0.0, 0.0);
    mybuf[2][i] = half * std::complex<float>(cos(ang*2.0), sin(ang*2.0));     
    ang += angincr; 
    if (ang > M_PI) {
      ang -= (2.0 * M_PI);       
    }
  }

  // create a streamer.
  std::vector<size_t> chans; 
  chans.push_back(0);
  SoapySDR::Stream * txstr = dev->setupStream(SOAPY_SDR_TX, "CF32", chans); 

#define ENABLE_RX_STREAM 1   
#if ENABLE_RX_STREAM  
  // also need to create RX streamer, otherwise the LMS7 hardware timer won't advance. (????)
  // (see the code in ILimeSDRStreaming.cpp that launches the RxLoopFunction and creates the
  // rxThread
  SoapySDR::Stream * rxstr = dev->setupStream(SOAPY_SDR_RX, "CF32", chans); 
  (void) rxstr; 
  // The rxstream has to be running in order to advance the time...
  stat = dev->activateStream(rxstr, 0, 0, 0); 
  if(stat < 0) {
    std::cerr << boost::format("RX activateStream got bad return stat = %d [%s]\n") 
      % stat % SoapySDR::errToStr(stat);
  }
#endif
  
  // send 5 seconds of stuff.
  int numiters = 5 * sample_rate / buflen;
  int flags = 0;
  int trips = 0;
  bool stream_active = false; 

  for(int j = 0; j < 3; j++) {
    trips = 0;     
    std::cerr << boost::format("\n\n\n\n\n%s\n\n") % start_message[j]; 
    for(int i = 0; i < numiters; i++) {
      txbufs[0] = mybuf[j];
      int togo = buflen;
      flags = 0; 
      if((i + 1) == numiters) {
	std::cerr << "Sending end burst\n";
	flags |= SOAPY_SDR_END_BURST; 
      }

      if(!stream_active) {
	// turn on the stream. 
	stat = dev->activateStream(txstr, 0, 0, 0);
	if(stat < 0) {
	  std::cerr << boost::format("TX activateStream got bad return stat = %d [%s]\n") 
	    % stat % SoapySDR::errToStr(stat);
	}
	else {
	  std::cerr << boost::format("TX activate Stream got %d [%s]\n") 
	    % stat % SoapySDR::errToStr(stat); 
	  stream_active = true; 
	}
      }

      while(togo > 0) {
	stat = dev->writeStream(txstr, (void**) txbufs, togo, flags); 
	flags = 0; 
	if(stat >= 0) {
	  togo -= stat; 
	  txbufs[0] += stat; 
	}
	else {
	  std::cerr << boost::format("writeStream got bad return stat = %d [%s] flags = %d\n")
	    % stat % SoapySDR::errToStr(stat) % flags; 
	}

	trips++; 
      }
    }

    long long timeNs = 0; 
    size_t chan_mask = 1;
    flags = 0;    
    stat = dev->readStreamStatus(txstr, chan_mask, flags, timeNs, 10 * 1000 * 1000);  // 10 second timeout
    std::cerr << boost::format("readStreamStatus got stat = %d [%s]  flags = %d  timeNs = %ld\n")
      % stat % SoapySDR::errToStr(stat) % flags % timeNs;

    std::cerr << boost::format("\n\n\n\n\n%s\n\n") % end_message[j];     
    sleep(10);     
  }

  flags = 0; 
  long long timeNs = 0; 
  size_t chan_mask = 1; 
  stat = dev->readStreamStatus(txstr, chan_mask, flags, timeNs, 10 * 1000000); 
  std::cerr << boost::format("readStreamStatus got stat = %d [%s]  flags = %d  timeNs = %ld\n")
      % stat % SoapySDR::errToStr(stat) % flags % timeNs; 

  stat = dev->deactivateStream(txstr, 0, 0); 
  if(stat < 0) {
    std::cerr << boost::format("deactivateStream (tx) got bad return stat = %d [%s]\n") 
      % stat % SoapySDR::errToStr(stat);
  }
  else {
    std::cerr << "Deactivated tx streamer.\n";
  }
  dev->closeStream(txstr);   

#if ENABLE_RX_STREAM  
  stat = dev->deactivateStream(rxstr, 0, 0); 
  if(stat < 0) {
    std::cerr << boost::format("deactivateStream (rx) got bad return stat = %d [%s]\n") 
      % stat % SoapySDR::errToStr(stat);
  }
  else {
    std::cerr << "Deactivated rx streamer.\n";
  }
  dev->closeStream(rxstr); 
#endif
  
  SoapySDR::Device::unmake(dev); 

  exit(0); 
}

