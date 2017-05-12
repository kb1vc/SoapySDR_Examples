#include <SoapySDR/Device.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Version.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <vector>

void myLogHandler(const SoapySDRLogLevel lev, const char * message)
{
  switch(lev) {
  case SoapySDRLogLevel::SOAPY_SDR_FATAL:
    std::cout << boost::format("Holy cow!  message = [%s]\n") % message;     
    break; 
  case SoapySDRLogLevel::SOAPY_SDR_INFO:
    // do nothing
    break; 
  default:
    std::cout << boost::format("Holy cow!  message = [%s]  level = %d\n") % message % lev;         
    break; 
  }
}

std::ostream & print_strvec(std::ostream & os, const std::string & label, const std::vector<std::string> & vec) {
  os << label; 
  BOOST_FOREACH(std::string st, vec) {
    os << " " << st; 
  }
  os << std::endl; 
  return os; 
}


std::ostream & print_infovec(std::ostream & os, const std::string & label, const SoapySDR::ArgInfoList & vec) {
  os << label; 
  BOOST_FOREACH(auto & el, vec) {
    os << boost::format("     N[%s] [K(%s), V(%s) -- D(%s)  U(%s)]\n") % el.name % el.key % el.value % el.description % el.units;
  }
  os << std::endl; 
  return os; 
}


std::ostream & print_dblvec(std::ostream & os, const std::string & label, const std::vector<double> & vec) {
  os << label; 
  BOOST_FOREACH(double st, vec) {
    os << boost::format(" %g") % st; 
  }
  os << std::endl; 
  return os; 
}


int main()
{
  SoapySDR::KwargsList kwl = SoapySDR::Device::enumerate(std::string("driver=lime"));
  // SoapySDR::KwargsList kwl = SoapySDR::Device::enumerate(); 
  int num_devices = 0; 
  std::vector<SoapySDR::Device *> devices; 

  std::cout << boost::format("SoapySDR version API [%s] ABI [%s] Lib [%s]\n")
    % SoapySDR::getAPIVersion() % SoapySDR::getABIVersion() % SoapySDR::getLibVersion() ;
  
  SoapySDR::registerLogHandler(myLogHandler); 

  BOOST_FOREACH(SoapySDR::Kwargs ar, kwl) {
    std::cout << boost::format("Device %d\n") % num_devices++; 
    BOOST_FOREACH(auto& arp, ar) {
      std::cout << boost::format("\tAttr [%s] = [%s]\n") % arp.first % arp.second; 
    }
    devices.push_back(SoapySDR::Device::make(ar));
  }

    int i = 0; 
  BOOST_FOREACH(SoapySDR::Device * dev, devices) {
    int rx_chan_count = dev->getNumChannels(SOAPY_SDR_RX);
    int tx_chan_count = dev->getNumChannels(SOAPY_SDR_TX);     
    std::cout << boost::format("Device %d\n") % i++;

    SoapySDR::Kwargs hwinfo = dev->getHardwareInfo(); 

    // std::cout << boost::format("\tThis device is a [%s]\n") % hwinfo["name"];
    print_infovec(std::cout, "  RX StreamArgs: ", dev->getStreamArgsInfo(SOAPY_SDR_RX, 0));
    print_infovec(std::cout, "  TX StreamArgs: ", dev->getStreamArgsInfo(SOAPY_SDR_TX, 0));    
    BOOST_FOREACH(auto & arp, hwinfo) {
      std::cout << boost::format("\tdev Attr [%s] = [%s]\n") % arp.first % arp.second;
    }

    print_strvec(std::cout, "  Clock Sources: ", dev->listClockSources());
    std::cout << boost::format("  Clock Source: [%s]\n") % dev->getClockSource();
    print_strvec(std::cout, "  Sensors: ", dev->listSensors());
    print_strvec(std::cout, "  GPIO Banks: ", dev->listGPIOBanks());
    print_strvec(std::cout, "  UARTs: ", dev->listUARTs());
    print_strvec(std::cout, "  Time Sources: ", dev->listTimeSources());
    std::cout << boost::format("  Master Clock Rate: %g\n") % dev->getMasterClockRate();
    dev->setMasterClockRate(40.0e6);
    std::cout << boost::format("  Master Clock Rate: %g\n") % dev->getMasterClockRate();
    
    std::cout << boost::format("  Selected Clock Source: [%s]\n") % dev->getClockSource();    
    
    for(int i = 0; i < rx_chan_count; i++) {
      std::cout << boost::format("  Channel: %d\n") % i; 
      SoapySDR::RangeList rf_l = dev->getFrequencyRange(SOAPY_SDR_RX, i, "RF");
      SoapySDR::RangeList bb_l = dev->getFrequencyRange(SOAPY_SDR_RX, i, "BB");

      print_strvec(std::cout, "\tRX Tuners:: ", dev->listFrequencies(SOAPY_SDR_RX, i)); 
      print_strvec(std::cout, "\tRX Amplifiers: ", dev->listGains(SOAPY_SDR_RX, i));
      print_dblvec(std::cout, "\tRX Rates: ", dev->listSampleRates(SOAPY_SDR_RX, i));
      print_strvec(std::cout, "\tRX Antennas: ", dev->listAntennas(SOAPY_SDR_RX, i));
      print_dblvec(std::cout, "\tRX Bandwidths: ", dev->listBandwidths(SOAPY_SDR_RX, i));
      std::cout << boost::format("\tFrontend Mapping: [%s]\n") % dev->getFrontendMapping(SOAPY_SDR_RX);
      std::cout << boost::format("\t%s Duplex\n") % ((dev->getFullDuplex(SOAPY_SDR_RX, i)) ? "Full" : "Half");      
      
      std::cout << boost::format("\tRX RF Freq Range: ");
      BOOST_FOREACH(auto & r, rf_l) {
        std::cout << boost::format("\t\t[%g..%g]\n") % r.minimum() % r.maximum(); 
      } 
      std::cout << boost::format("\tRX BB Freq Range: ");
      BOOST_FOREACH(auto & r, bb_l) {
        std::cout << boost::format("\t\t[%g..%g]\n") % r.minimum() % r.maximum(); 
      } 

      SoapySDR::Range gr = dev->getGainRange(SOAPY_SDR_RX, i);      
      std::cout << boost::format("\tRX Gain Range [%g..%g]\n") % gr.minimum() % gr.maximum(); 
    }

    for(int i = 0; i < tx_chan_count; i++) {
      std::cout << boost::format("  Channel: %d\n") % i; 

      print_strvec(std::cout, "\tTX Tuners: ", dev->listFrequencies(SOAPY_SDR_TX, i));
      print_strvec(std::cout, "\tTX Amplifiers: ", dev->listGains(SOAPY_SDR_TX, i));
      print_dblvec(std::cout, "\tTX Rates: ", dev->listSampleRates(SOAPY_SDR_TX, i));       
      print_strvec(std::cout, "\tTX Antennas: ", dev->listAntennas(SOAPY_SDR_TX, i));
      print_dblvec(std::cout, "\tTX Bandwidths: ", dev->listBandwidths(SOAPY_SDR_TX, i));      
      std::cout << boost::format("\tFrontend Mapping: [%s]\n") % dev->getFrontendMapping(SOAPY_SDR_TX);
      std::cout << boost::format("\t%s Duplex\n") % ((dev->getFullDuplex(SOAPY_SDR_TX, i)) ? "Full" : "Half");

      SoapySDR::RangeList rl = dev->getFrequencyRange(SOAPY_SDR_TX, i);      
      std::cout << boost::format("\tTX Freq Range: ");
      BOOST_FOREACH(auto & r, rl) {
        std::cout << boost::format("\t\t[%g..%g]\n") % r.minimum() % r.maximum(); 
      } 

      SoapySDR::Range gr = dev->getGainRange(SOAPY_SDR_TX, i);      
      std::cout << boost::format("\tTX Gain Range [%g..%g]\n") % gr.minimum() % gr.maximum(); 

    }
    SoapySDR::Device::unmake(dev); 
  }

  exit(0); 
}
