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

// danger danger Will Robinson!  This is a really crappy thing to do, as
// the mantissa of a double is not big enough to hold all the possible bits
// in a long long.  However, ts is a timestamp, and we'll be fine as long
// as we can keep 53 bits.  That's about 2^23 seconds, and I don't intend to
// hang around that long for this experiment.   
double timestampToSec(long long ts) {
  double ns = (double) ts; 
  return ns * 1.0e-9; 
}

void readBuffer(SoapySDR::Device * dev, SoapySDR::Stream * str, size_t len)
{
  std::complex<float> * v = new std::complex<float>[len];
  std::complex<float> * buf[1]; 

  buf[0] = v; 

  int flags = SOAPY_SDR_END_BURST; 
  size_t togo = len; 
  long long tstamp = 0; 
  while(togo > 0) {
    int stat = dev->readStream(str, (void**) buf, togo, flags, tstamp); 

    if(stat < 0) {
      std::cerr << boost::format("RX readStream got bad return stat = %d [%s]\n") % stat % SoapySDR::errToStr(stat);
      return; 
    }
    else {
      togo -= stat; 
      buf[0] += stat; 
    }
  }
  
  delete v; 
}


std::complex<float> * initVec(size_t len)
{
  // build a 1kHz tone... 
  double ang = 0.0; 
  double angincr = 2.0 * M_PI / 625.0; 

  std::complex<float> * v = new std::complex<float>[len]; 

  for(size_t i = 0; i < len; i++) {
    v[i] = std::complex<float>(cos(ang), sin(ang)); 
    ang += angincr; 
    if(ang > M_PI) {
      ang -= (2.0 * M_PI); 
    }
  }

  return v; 
}

void writeBuffer(SoapySDR::Device * dev, SoapySDR::Stream * str, 
		 std::complex<float> * vec, size_t len)
{
  std::complex<float> * buf[1]; 

  buf[0] = vec; 

  int stat; 
  int flags = SOAPY_SDR_END_BURST; 
  size_t togo = len; 
  long long tstamp = 0; 
  int loopcount = 0; 
  while(togo > 0) {
    stat = dev->writeStream(str, (void**) buf, len, flags, tstamp); 

    if(stat < 0) {
      std::cerr << boost::format("RX readStream got bad return stat = %d [%s] flags = 0x%x\n") % stat % SoapySDR::errToStr(stat) % flags;
      return; 
    }
    else {
      togo -= stat; 
      buf[0] += stat; 
      loopcount++; 
    }
  }

  flags = 0; 
  size_t cmsk = 1; 
  long long tns; 
  stat = dev->readStreamStatus(str, cmsk, flags, tns, 1000000); // 1 sec timeout
  std::cerr << boost::format("readStreamStatus returns %d [%s] flags = 0x%x  tns = %ld\n")
    % stat % SoapySDR::errToStr(stat) % flags % tns; 

  std::cerr << boost::format("Wrote buffer in %d chunks\n") % loopcount;
}


int main()
{
  SoapySDR::KwargsList kwl = SoapySDR::Device::enumerate(std::string("driver=lime"));
  SoapySDR::registerLogHandler(myLogHandler); 
  
  // SoapySDR::KwargsList kwl = SoapySDR::Device::enumerate(); 
  SoapySDR::Device * dev; 
  int stat; 

  size_t bufsize = 30000;   
  
  std::complex<float> * tx_buf = initVec(bufsize); 
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


  // one channel... 
  std::vector<size_t> chans; 
  chans.push_back(0);

  // Create an RX streamer, otherwise the LMS7 hardware timer won't advance. (????)
  // (see the code in ILimeSDRStreaming.cpp that launches the RxLoopFunction and creates the
  // rxThread
  SoapySDR::Stream * rxstr = dev->setupStream(SOAPY_SDR_RX, "CF32", chans); 
  
  // create a TX streamer.
  SoapySDR::Stream * txstr = dev->setupStream(SOAPY_SDR_TX, "CF32", chans); 

  // The rxstream has to be running in order to advance the time...
  stat = dev->activateStream(rxstr, 0, 0, 0); 
  if(stat < 0) {
    std::cerr << boost::format("RX activateStream got bad return stat = %d [%s]\n") % stat % SoapySDR::errToStr(stat);
  }

  // turn on the TX stream. 
  stat = dev->activateStream(txstr, 0, 0, 0);
  if(stat < 0) {
    std::cerr << boost::format("TX activateStream got bad return stat = %d [%s]\n") % stat % SoapySDR::errToStr(stat);
  }

  dev->setHardwareTime(0L);
  usleep(10); 
  double ts;
  ts = timestampToSec(dev->getHardwareTime()); 
  std::cerr << boost::format("Time after 10uS delay = %16.12g\n") % ts;
  usleep(10000);
  ts = timestampToSec(dev->getHardwareTime());     
  std::cerr << boost::format("Time after 10mS delay = %16.12g\n") % ts;

  readBuffer(dev, rxstr, bufsize); 

  ts = timestampToSec(dev->getHardwareTime()); 
  std::cerr << boost::format("Time after reading a %d byte buffer = %16.12g\n") % bufsize % ts;

  writeBuffer(dev, rxstr, tx_buf, bufsize);

  ts = timestampToSec(dev->getHardwareTime()); 
  std::cerr << boost::format("Time after writing a %d byte buffer = %16.12g\n") % bufsize % ts;
  
  stat = dev->deactivateStream(rxstr, 0, 0); 
  if(stat < 0) {
    std::cerr << boost::format("deactivateStream got bad return stat = %d [%s]\n") % stat % SoapySDR::errToStr(stat);
  }
  else {
    std::cerr << "Deactivated RX streamer." << std::endl; 
  }
  ts = timestampToSec(dev->getHardwareTime()); 
  std::cerr << boost::format("Time after deactivating rx stream = %16.12g\n") % ts;


  writeBuffer(dev, rxstr, tx_buf, bufsize);

  ts = timestampToSec(dev->getHardwareTime()); 
  std::cerr << boost::format("Time after writing a %d byte buffer = %16.12g\n") % bufsize % ts;

  usleep(10000);
  ts = timestampToSec(dev->getHardwareTime()); 
  std::cerr << boost::format("Time after 10mS delay = %16.12g\n") % ts;
  usleep(10000);
  ts = timestampToSec(dev->getHardwareTime());     
  std::cerr << boost::format("Time after 10mS delay = %16.12g\n") % ts; 
  

  SoapySDR::Device::unmake(dev); 

  exit(0); 
}
