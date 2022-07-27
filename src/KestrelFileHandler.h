#ifndef KestrelFileHandler_h
#define KestrelFileHandler_h

#include <SdFat.h>
#include <Particle.h>
#include <MB85RC256V-FRAM-RK.h>
#include <Kestrel.h>

namespace DestCodes{
    constexpr uint8_t None = 0x00;
    constexpr uint8_t SD = 0x01;
    constexpr uint8_t Particle = 0x02;
    constexpr uint8_t Both = 0x03; 
    constexpr uint8_t SDRetry = 0x81;
    constexpr uint8_t ParticleRetry = 0x82;
    constexpr uint8_t BothRetry = 0x83;
};

namespace DataType{
    constexpr uint8_t Error = 1;
    constexpr uint8_t Data = 0;
    constexpr uint8_t Diagnostic = 2;
    constexpr uint8_t Metadata = 3;
};

struct dataFRAM {
    uint8_t destCode;
    uint32_t blockEnd;
    uint8_t destLen;
    char dest[64];
    uint16_t dataLen;
    char data[1024];
};



class KestrelFileHandler {

    public:
        KestrelFileHandler(Kestrel& logger_); //FIX! Should we pass in the max length of packets??
        /**
        * @brief Initialize the system and generate new file paths for each type on the SD card
        * * @param tryBackhaul defaults to true, specifies if the initalization should try to backhaul the unsent logs
        * @return lvl 2 diagnostic 
        */    
        String begin(bool tryBackhaul = true);
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
        * * @param[in] dataStr: String of data to be queued up
        * * @param[in] destStr: String which provides either the file path on the SD card or the Particle publish descriptor 
        * * @param[in] destination: Destination code which deliniates where the data should be sent when dumped (SD, Particle, etc)
        * @details If newlines are used to seperate packet chunks, these will be broken up and sent as individual packets in the particle cloud
        */  
        bool writeToFRAM(String dataStr, String destStr, uint8_t destination);
        /**
        * @brief Que data in the FRAM to be dumped to given locations later
        * * @param[in] dataStr: String of data to be queued up
        * * @param[in] dataType: Specifies what kind of data (data, error, diagnostic, metadata) is being sent and creates the destinations appropriately  
        * * @param[in] destination: Destination code which deliniates where the data should be sent when dumped (SD, Particle, etc)
        * @details If newlines are used to seperate packet chunks, these will be broken up and sent as individual packets in the particle cloud
        */  
        bool writeToFRAM(String dataStr, uint8_t dataType, uint8_t destination);
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
       /**
        * @brief Attempts to backhaul old logs 
        * @details Tries to backhaul unsent logs if there is a network connection
        * @return bool, success or failure of backhaul
        */  
       bool tryBackhaul();
       /**
        * @brief Erases FRAM data
        * @details Simply calls the erase command, allows system to start from clean slate
        */  
        bool eraseFRAM();
        
        
    private:
        // String dataFilePath = ""; ///<Path describing the location of the data file on the SD card, updated each time `begin()` is run
        // String metadataFilePath = ""; ///<Path describing the location of the metadata file on the SD card, updated each time `begin()` is run
        // String errorFilePath = ""; ///<Path describing the location of the error file on the SD card, updated each time `begin()` is run
        // String diagnosticFilePath = ""; ///<Path describing the location of the diagnostic file on the SD card, updated each time `begin()` is run
        String filePaths[6] = {""}; ///<Paths for the location of the data files on the SD card (indicies mapped to DataType)
        const String fileShortNames[5] = {"Data","Err","Diag","Meta","Dump"};
        String publishTypes[5] = {"data","error","diagnostic","metadata","unsent"}; ///<Defines the values sent for particle publish names
        static constexpr int MAX_MESSAGE_LENGTH = 1024; ///<Maximum number of characters allowed for single transmission 
        //FIX! Call from particle!
        const uint16_t maxFileNum = 9999; //Max number of files allowed 
        uint32_t memSizeFRAM = 65536; ///<Default to 65536 words which corresponds to 512kB memory 
        uint8_t adrLenFRAM = 2; ///<Default to using 2 bytes to encode address pointers, corresponds to 512kB memory
        const uint32_t blockOffset = 1102; //FIX! Make variable
        const uint32_t dataBlockEnd = 0; ///<End of the data region of the FRAM (growing toawrd 0)
        SdFat sd;
        File sdFile;
        MB85RC256V fram;
        Kestrel& logger;

        uint32_t readValFRAM(uint32_t pos, uint8_t len);
        static KestrelFileHandler* selfPointer;
        static void dateTimeSD(uint16_t* date, uint16_t* time);
        void dateTimeSD_Glob(uint16_t* date, uint16_t* time);
        bool dumpToSD();
        bool backhaulUnsentLogs();
        long getStackPointer();
        const uint8_t chipSelect = SS;


};

#endif