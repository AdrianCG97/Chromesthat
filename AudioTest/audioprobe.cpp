// audioprobe.cpp
 
#include <iostream>
#include "RtAudio.h"
 
int main()
{
  RtAudio audio;
 
  // Get the list of device IDs
  std::vector< unsigned int > ids = audio.getDeviceIds();
  if ( ids.size() == 0 ) {
    std::cout << "No devices found." << std::endl;
    return 0;
  }
 
  // Scan through devices for various capabilities
  RtAudio::DeviceInfo info;
  for ( unsigned int n=0; n<ids.size(); n++ ) {
 
    info = audio.getDeviceInfo( ids[n] );
 
    // Print, for example, the name and maximum number of output channels for each device
    std::cout << "device name = " << info.name << std::endl;
    std::cout << ": maximum output channels = " << info.outputChannels << std::endl;
    std::cout << ": maximum input channels = " << info.inputChannels << std::endl;
  }
 
  return 0;
}