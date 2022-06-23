#include <KestrelFileHandler.h>

KestrelFileHandler::KestrelFileHandler() 
{

}

String KestrelFileHandler::begin()
{
    
    return ""; //DEBUG!
}

bool KestrelFileHandler::writeToSD(String data, String path)
{
    if (!sd.begin(chipSelect, SPI_FULL_SPEED)) { //Initialize SD card, assume power has been cycled since last time
        sd.initErrorHalt(); //DEBUG!??
    }

    if (!sdFile.open(path, O_RDWR | O_CREAT | O_AT_END)) { //Try to open the specified file
        sd.errorHalt("opening test.txt for write failed");
        sdFile.close();
        return false; //Return fail if not able to write
        //FIX! ThrowError!
    }
    else {
        sdFile.println(data); //Append data to end
    }
    sdFile.close(); //Regardless of access, close file when done 
    return true; //If get to this point, should have been success
    //FIX! Read back??
    
}

bool KestrelFileHandler::writeToParticle(String data, String path)
{
    if(!Particle.connected()) {
        //FIX! Throw error
        return false; //If not connected to the cloud already, throw error and exit with fault
    }
    if(data.length() > Particle.maxEventDataSize() && data.indexOf('\n') < 0) {
        //FIX! Throw error
        return false; //If string is longer than can be transmitted in one packet, AND there are not line breaks to work with, throw error and exit
    }
    else if(data.length() < Particle.maxEventDataSize() && data.indexOf('\n') < 0) { //If less than max length and no line breaks, perform simple transmit
        bool sent = Particle.publish(path, data, WITH_ACK);
        // if(!sent) {
        //     //FIX! Throw error if send did not work
        // }
        return sent; //Return the pass fail result after attempting a transmission with acknowledge 
    }
    else if(data.indexOf('\n') > 0) { //If there are line breaks, seperate them, regardless of total length
        if(data.charAt(data.length() - 1) == '\n') data.remove(data.length() - 1); //If string is terminated with newline, remove this
        String temp = ""; //Make temp string to hold substrings 
        bool sent = true; //Keep track if any of the send attempts fail
        while(data.indexOf('\n') > 0) {
            temp = data.substring(data.lastIndexOf('\n')); 
            //FIX! test if this substring is still too long because of bad parsing before, if so, throw error
            sent = sent & Particle.publish(path, temp, WITH_ACK); //If any of the sends fail, sent will be cleared
            data.remove(data.lastIndexOf('\n') - 1); //Clear end substring from data, including newline return 
        }
        sent = sent & Particle.publish(path, data, WITH_ACK); //Send last string
        return sent; //Return the cumulative result
    }
    return false; //If it gets to this point, there is an error 
}

bool KestrelFileHandler::writeToFRAM(String data, String path, uint8_t destination)
{
    return false; //DEBUG!
}

bool KestrelFileHandler::dumpFRAM()
{
    return false; //DEBUG!
}

String KestrelFileHandler::selfDiagnostic(uint8_t level)
{
    return ""; //DEBUG!
}