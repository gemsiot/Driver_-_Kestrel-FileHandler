#ifndef KestrelFileHandler_h
#define KestrelFileHandler_h

#include <SdFat.h>
#include <Particle.h>

class KestrelFileHandler {

    public:
        KestrelFileHandler(); //FIX! Should we pass in the max length of packets??
        /**
        * @brief Initialize the system and generate new file paths for each type on the SD card
        */    
        String begin();
        /**
        * @brief Write the given data string to SD card
        * * @param[in] data: String of data to be written to SD card
        * * @param[in] path: String which describes the file path where the data should be placed 
        * @details If file does not exist, data is written and a warning is sent 
        */    
        bool writeToSD(String data, String path);
        /**
        * @brief Write the given data string to Particle cloud
        * * @param[in] data: String of data to be written to SD card
        * * @param[in] path: String which is the descriptor send with the publish command 
        * @details Must already be connected to the cloud, otherwise error will be returned 
        */  
        bool writeToParticle(String data, String path);
        /**
        * @brief Que data in the FRAM to be dumped to given locations later
        * * @param[in] data: String of data to be queued up
        * * @param[in] path: String which provides either the file path on the SD card or the Particle publish descriptor 
        * * @param[in] destination: Destination code which deliniates where the data should be sent when dumped (SD, Particle, etc)
        * @details If newlines are used to seperate packet chunks, these will be broken up and sent as individual packets in the particle cloud
        */  
        bool writeToFRAM(String data, String path, uint8_t destination);
        /**
        * @brief All data from the FRAM is dumped to specified locations
        * @details All data sinks (SD card, particle modem, etc) must be initialized and enabled prior to calling this function 
        */  
        bool dumpFRAM();
        /**
        * @brief Runs self diagnostic of the file system 
        * * @param[in] level: Denotes the level of diagnostic to run (5 ~ 1)
        * @details Self diagnostic result is returned as a JSON formatted string from the function  
        */  
       String selfDiagnostic(uint8_t level);
    
    private:
        String dataFilePath = ""; ///<Path describing the location of the data file on the SD card, updated each time `begin()` is run
        String metadataFilePath = ""; ///<Path describing the location of the metadata file on the SD card, updated each time `begin()` is run
        String errorFilePath = ""; ///<Path describing the location of the error file on the SD card, updated each time `begin()` is run
        String diagnosticFilePath = ""; ///<Path describing the location of the diagnostic file on the SD card, updated each time `begin()` is run

        SdFat sd;
        File sdFile;

        const uint8_t chipSelect = SS;


};

#endif