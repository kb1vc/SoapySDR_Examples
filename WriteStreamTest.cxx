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

void myLogHandler(const SoapySDRLogLevel lev, const char * message)
{
  std::cerr << boost::format("[%s]  level = %d\n") % message % lev; 
}

int main()
{
  SoapySDR::KwargsList kwl = SoapySDR::Device::enumerate(std::string("driver=lime"));
  SoapySDR::registerLogHandler(myLogHandler); 
  
  // SoapySDR::KwargsList kwl = SoapySDR::Device::enumerate(); 
  SoapySDR::Device * dev; 
  int stat; 

  if(kwl.size() == 0) {
    std::cerr << "No LimeSDR device was found.\n";
    exit(-1);
  }

  dev = SoapySDR::Device::make(kwl[0]);

  dev->setMasterClockRate(40.0e6);
  std::cout << boost::format("  Master Clock Rate: %g\n") % dev->getMasterClockRate();

  dev->setFrequency(SOAPY_SDR_TX, 0, 144.295e6);
  dev->setSampleRate(SOAPY_SDR_TX, 0, 625000);

  dev->setFrequency(SOAPY_SDR_RX, 0, 144.295e6);
  dev->setSampleRate(SOAPY_SDR_RX, 0, 625000);

  dev->setGain(SOAPY_SDR_TX, 0, 52);

  dev->setAntenna(SOAPY_SDR_TX, 0, "BAND1");


  // make buffers
  int buflen = 30000;     
  std::complex<float> mybuf[2][buflen];
  std::complex<float> * bufs[1];
  double ang = 0.0; 
  double angincr = 2.0 * M_PI / 625.0; 


  for(int i = 0; i < buflen; i++) {
    mybuf[0][i] = std::complex<float>(cos(ang), sin(ang)); 
    mybuf[1][i] = std::complex<float>(cos(ang*2.0), sin(ang*2.0)); 
    ang += angincr; 
    if (ang > M_PI) {
      ang -= (2.0 * M_PI);       
    }
  }

  // create a streamer.
  std::vector<size_t> chans; 
  chans.push_back(0);
  SoapySDR::Stream * txstr = dev->setupStream(SOAPY_SDR_TX, "CF32", chans); 

  // also need to create RX streamer, otherwise the LMS7 hardware timer won't advance. (????)
  // (see the code in ILimeSDRStreaming.cpp that launches the RxLoopFunction and creates the
  // rxThread
  SoapySDR::Stream * rxstr = dev->setupStream(SOAPY_SDR_RX, "CF32", chans); 
  (void) rxstr; 
  // The rxstream has to be running in order to advance the time...
  stat = dev->activateStream(rxstr, 0, 0, 0); 
  if(stat < 0) {
    std::cerr << boost::format("RX activateStream got bad return stat = %d [%s]\n") % stat % SoapySDR::errToStr(stat);
  }
  

  std::cerr << boost::format("Stream MTU = %ld\n") % dev->getStreamMTU(txstr); 
  std::cerr << boost::format("Has HW Time = %c\n") % ((char) (dev->hasHardwareTime()) ? 'T' : 'F');


  // turn on the stream. 
  stat = dev->activateStream(txstr, 0, 0, 0);
  if(stat < 0) {
    std::cerr << boost::format("TX activateStream got bad return stat = %d [%s]\n") % stat % SoapySDR::errToStr(stat);
  }

  if(dev->hasHardwareTime()) {
    dev->setHardwareTime(0L);
    usleep(10); 
    std::cerr << boost::format("Time after 10uS = %ld\n") % dev->getHardwareTime(); 
    usleep(10); 
    std::cerr << boost::format("Time after 10uS = %ld\n") % dev->getHardwareTime(); 
  }

  // send 5 seconds of stuff.
  int numiters = 5 * 625000 / buflen;
  int flags = 0;
  int trips = 0;
  // 100  uS shorter than the 30K sample interval.  
  long long time_interval = (((long long) buflen) * 1000L *  1000L / 625000L) - 100L; // in microseconds
  std::cerr << boost::format("time interval = %ld nS\n") % time_interval; 
  long long hwtime; 
  // long long timeout = 1000000;
  for(int j = 0; j < 2; j++) {
    for(int i = 0; i < numiters; i++) {
      bufs[0] = mybuf[j]; 
      int togo = buflen;
      flags = SOAPY_SDR_HAS_TIME; 
      if((i + 1) == numiters) {
	flags |= SOAPY_SDR_END_BURST; 
	std::cerr << "$"; 
      }
      while(togo > 0) {
	stat = dev->writeStream(txstr, (void**) bufs, togo, flags); // , hwtime + 100000L, timeout);
	flags = 0; 
	if(stat >= 0) {
	  togo -= stat; 
	  bufs[0] += stat; 
	}
	else {
	  std::cerr << boost::format("writeStream got bad return stat = %d [%s] flags = %d\n")
	    % stat % SoapySDR::errToStr(stat) % flags; 
	}
	trips++; 
	std::cerr << "."; 
	hwtime += time_interval;
	usleep(time_interval); 
      }
      //hwtime += time_interval; 
    }
    flags = 0;
    std::cerr << boost::format("Time after a bunch of stuff... = %ld\n") % dev->getHardwareTime();
    stat = dev->deactivateStream(rxstr);
    long long timeNs = 0; 
    size_t chan_mask = 1;
    stat = dev->readStreamStatus(txstr, chan_mask, flags, timeNs, 10 * 1000000); 
    std::cerr << boost::format("readStreamStatus got stat = %d [%s]  flags = %d  timeNs = %ld\n")
      % stat % SoapySDR::errToStr(stat) % flags % timeNs;
    
    std::cerr << boost::format("Took %d trips in %d iterations\n") % trips % numiters;
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
    std::cerr << boost::format("deactivateStream got bad return stat = %d [%s]\n") % stat % SoapySDR::errToStr(stat);
  }

  SoapySDR::Device::unmake(dev); 

  exit(0); 
}
