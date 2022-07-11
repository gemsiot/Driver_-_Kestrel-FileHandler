#include <KestrelFileHandler.h>
#include <Kestrel.h>

KestrelFileHandler* KestrelFileHandler::selfPointer;

// KestrelFileHandler::KestrelFileHandler(Kestrel const& logger_) : logger{logger_}, fram(Wire, 0) //Get copy of logger object for time calls and SD control
KestrelFileHandler::KestrelFileHandler(Kestrel& logger_) : logger(logger_), fram(Wire, 0) //Get copy of logger object for time calls and SD control
{
    // logger = logger_;
}

String KestrelFileHandler::begin()
{
    selfPointer = this;
    SdFile::dateTimeCallback(dateTimeSD);
    logger.enableI2C_Global(false); //Disable external I2C
    logger.enableI2C_OB(true); //Turn on internal I2C
	logger.enableSD(true); //Turn on power to SD card
    // Serial.println("File Handler Init"); //DEBUG!
    // Serial.println(logger.updateTime()); //DEBUG!
    uint32_t currentPointer;
    fram.get(memSizeFRAM - sizeof(memSizeFRAM), currentPointer);
    if(currentPointer > memSizeFRAM) {
        Serial.println("ERROR: FRAM Pointer Overrun Reset"); //DEBUG!
        fram.put(memSizeFRAM - sizeof(memSizeFRAM), memSizeFRAM); //Write default value in if not initialized already
        fram.get(memSizeFRAM - sizeof(memSizeFRAM), currentPointer); //DEBUG! Read back
        // Serial.print("New PointerA: "); //DEBUG!
        // Serial.println(currentPointer); //DEBUG!
    }
    if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
        sd.initErrorHalt();
        //Throw Error!
    }
    sd.mkdir("GEMS");
    sd.chdir("/GEMS"); //Move into GEMS directory
    // if(sd.isDir(String(Particle.deviceID()))) {
    //     //FIX! Throw error
    //     Serial.println("SD Dir not present");
    // }
    sd.mkdir(String(Particle.deviceID())); //Make directory from device ID if does not already exist
    sd.chdir(String(Particle.deviceID()), false); //DEBUG! Restore
    for(int i = 0; i < 5; i++) {
        sd.mkdir(publishTypes[i]); //Make sub folders for each data type
    }
    sd.chdir(); //DEBUG! Go to root
    //FIX! Add year beakdown??
    // for(int i = 0; i < sizeof(publishTypes); i++) { //FIX! Causes assertion failure, not sure why??
    for(int i = 0; i < 4; i++) { //DEBUG!
        filePaths[i] = "/GEMS/" + String(Particle.deviceID()) + "/" + publishTypes[i]; //Create each file path base 
        uint16_t testFileIndex = 1; 
        String testFile = filePaths[i] + "/" + fileShortNames[i] + String(testFileIndex) + ".json";
        while(sd.exists(testFile) && testFileIndex < maxFileNum) {
            testFileIndex += 1; //Increment file index and try again
            testFile = filePaths[i] + "/" + fileShortNames[i] + String(testFileIndex) + ".json";
        }
        if(testFileIndex == maxFileNum) {
            //FIX! Throw error
            Serial.println("ERROR: SD Max File Num Exceeded");
        }
        else {
            filePaths[i] = testFile; //Copy back test file to main file name source
        }
        Serial.println(filePaths[i]); //DEBUG! Print out SD file paths
    }
    // sd.chdir(String(Particle.deviceID()), false); //Move to device ID folder in order to make dump folder at right level
    
    filePaths[4] = "/GEMS/" + String(Particle.deviceID()) + "/" + publishTypes[4] + "/" + fileShortNames[4] + ".txt"; //Create base file path for unsent logs, use normal text file 
    filePaths[5] = "/GEMS/" + String(Particle.deviceID()) + "/" + publishTypes[4] + "/" + fileShortNames[4] + "Temp"; //Create TEMP file path for unsent logs, no file extension since it should not be seen by eyes of man
    // filePaths[4] = "/GEMS/" + String(Particle.deviceID()) + "/" + publishTypes[4] + fileShortNames[4] + ".txt"; //DEBUG!
    Serial.println(filePaths[4]); //DEBUG! Print out Backhaul file path
    // sd.chdir("/"); //Move back to root
    sd.ls(); //DEBUG!
    if(sd.exists(filePaths[4])) { //Check if there exits a unsent log already, if so try to backhaul this
        //FIX! Throw error
        Serial.println("Backhaul Unsent Logs"); //DEBUG!
        backhaulUnsentLogs(); 
    }
    logger.enableSD(false); //Turn SD back off
    fram.get(memSizeFRAM - sizeof(memSizeFRAM), currentPointer); //DEBUG! Read back
    // Serial.print("New PointerB: "); //DEBUG!
    // Serial.println(currentPointer); //DEBUG!
    return ""; //DEBUG!
}

bool KestrelFileHandler::writeToSD(String data, String path)
{
    logger.enableSD(true); //Turn on power to SD card
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
    logger.enableSD(false); //Turn SD back off
    return true; //If get to this point, should have been success
    //FIX! Read back??
    
}

bool KestrelFileHandler::writeToParticle(String data, String path)
{
    if(!Particle.connected()) {
        //FIX! Throw error
        return false; //If not connected to the cloud already, throw error and exit with fault
    }
    if(data.length() > MAX_MESSAGE_LENGTH && data.indexOf('\n') < 0) {
        //FIX! Throw error
        return false; //If string is longer than can be transmitted in one packet, AND there are not line breaks to work with, throw error and exit
    }
    else if(data.length() < MAX_MESSAGE_LENGTH && data.indexOf('\n') < 0) { //If less than max length and no line breaks, perform simple transmit
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
            data.remove(data.lastIndexOf('\n')); //Clear end substring from data, including newline return 
        }
        sent = sent & Particle.publish(path, data, WITH_ACK); //Send last string
        return sent; //Return the cumulative result
    }
    return false; //If it gets to this point, there is an error 
}

bool KestrelFileHandler::writeToFRAM(String dataStr, String destStr, uint8_t destination)
{
    logger.enableI2C_Global(false); //Disable external I2C
    logger.enableI2C_OB(true); //Turn on internal I2C
    
    // uint32_t stackPointer = readValFRAM(memSizeFRAM - adrLenFRAM, adrLenFRAM); //Read from bottom bytes to get position to start actual read from
    if(dataStr.length() > MAX_MESSAGE_LENGTH && dataStr.indexOf('\n') < 0) {
        //FIX! Throw error
        return false; //If string is longer than can be transmitted in one packet, AND there are not line breaks to work with, throw error and exit
    }
    else if(dataStr.length() < MAX_MESSAGE_LENGTH && dataStr.indexOf('\n') < 0) { //If less than max length and no line breaks, perform simple transmit
        // bool sent = Particle.publish(destStr, dataStr, WITH_ACK);
        // if(!sent) {
        //     //FIX! Throw error if send did not work
        // }
        // return sent; //Return the pass fail result after attempting a transmission with acknowledge 
        //CONTINUE??
    }
    else if(dataStr.indexOf('\n') > 0) { //If there are line breaks, seperate them, regardless of total length
        if(dataStr.charAt(dataStr.length() - 1) == '\n') dataStr.remove(dataStr.length() - 1); //If string is terminated with newline, remove this
        String temp = ""; //Make temp string to hold substrings 
        bool sent = true; //Keep track if any of the send attempts fail
        while(dataStr.indexOf('\n') > 0) {
            temp = dataStr.substring(dataStr.lastIndexOf('\n')); 
            //FIX! test if this substring is still too long because of bad parsing before, if so, throw error
            // sent = sent & Particle.publish(destStr, temp, WITH_ACK); //If any of the sends fail, sent will be cleared
            sent = sent & writeToFRAM(destStr, temp, destination); //Pass the line off for recursive processing 
            dataStr.remove(dataStr.lastIndexOf('\n')); //Clear end substring from data, including newline return 
        }
        // sent = sent & Particle.publish(destStr, dataStr, WITH_ACK); //Send last string
        sent = sent & writeToFRAM(destStr, temp, destination); //Pass last line off for recursive processing
        // return sent; //Return the cumulative result
    }

    uint32_t stackPointer;
    fram.get(memSizeFRAM - sizeof(stackPointer), stackPointer); //Grab current value of stack pointer
    // Serial.print("New PointerD: "); //DEBUG!
    // Serial.println(stackPointer); //DEBUG!
    if((stackPointer - blockOffset) < dataBlockEnd || (stackPointer - blockOffset) > memSizeFRAM) { //Check if overfun will occour, if so, dump the FRAM
        if(dumpToSD()) fram.get(memSizeFRAM - sizeof(stackPointer), stackPointer); //If sucessfully dumped FRAM, grab new stack pointer and proceed
        else return false; //Otherwise stop trying to enter this log and return failure 
    }
    // uint32_t blockEnd = 0;
    uint32_t blockEnd = stackPointer;
    // stackPointer = stackPointer - (4 + destStr.length() + dataStr.length() + adrLenFRAM); 
    stackPointer -= blockOffset; //DEBUG! FIX!!!
    dataFRAM temp = {destination, blockEnd, destStr.length(), {0}, dataStr.length(), {0}};
    strcpy(temp.dest, destStr.c_str());
    strcpy(temp.data, dataStr.c_str());
    fram.put(stackPointer, temp); //Place object
    Serial.print("WRITE STACK POINTER: "); //DEBUG!
    Serial.println(stackPointer);
    fram.put(memSizeFRAM - sizeof(stackPointer), stackPointer); //Replace updated stack pointer at end of FRAM

    return false; //DEBUG!
}

bool KestrelFileHandler::writeToFRAM(String dataStr, uint8_t dataType, uint8_t destination)
{
    
    logger.enableI2C_Global(false); //Disable external I2C
    logger.enableI2C_OB(true); //Turn on internal I2C
    // Wire.reset(); //DEBUG!
    // delay(10); //DEBUG!
    // uint32_t stackPointer = readValFRAM(memSizeFRAM - adrLenFRAM, adrLenFRAM); //Read from bottom bytes to get position to start actual read from
    String destStr = publishTypes[dataType]; //Intelegently assign destination string
    uint32_t stackPointer;
    // fram.get(memSizeFRAM - sizeof(stackPointer), stackPointer); //Grab current value of stack pointer
    // fram.get(memSizeFRAM - 4, stackPointer); //Grab current value of stack pointer //DEBUG! Duplicate to try to fix no read problem, DUMB!
    fram.get(memSizeFRAM - sizeof(stackPointer), stackPointer); //Grab current value of stack pointer 
    // Serial.print("New PointerC: "); //DEBUG!
    // Serial.println(stackPointer); //DEBUG!
    if((stackPointer - blockOffset) < dataBlockEnd || (stackPointer - blockOffset) > memSizeFRAM) { //Check if overfun will occour, if so, dump the FRAM
        if(dumpToSD()) fram.get(memSizeFRAM - sizeof(stackPointer), stackPointer); //If sucessfully dumped FRAM, grab new stack pointer and proceed
        else return false; //Otherwise stop trying to enter this log and return failure 
    }
    // uint32_t blockEnd = 0;
    uint32_t blockEnd = stackPointer;
    // stackPointer = stackPointer - (4 + destStr.length() + dataStr.length() + adrLenFRAM); 
    stackPointer -= blockOffset; 
    dataFRAM temp = {destination, blockEnd, destStr.length(), {0}, dataStr.length(), {0}};
    strcpy(temp.dest, destStr.c_str());
    strcpy(temp.data, dataStr.c_str());
    fram.put(stackPointer, temp); //Place object
    Serial.print("WRITE STACK POINTER: "); //DEBUG!
    Serial.println(stackPointer);
    fram.put(memSizeFRAM - sizeof(stackPointer), stackPointer); //Replace updated stack pointer at end of FRAM
    // Serial.println(String(temp.data)); //DEBUG!
    // Serial.print("STACKPOINTER-Write: ");
    // Serial.println(stackPointer);
    // Serial.print("Destination: "); //DEBUG!
    // Serial.println(temp.destCode); 
    // for(int i = 0; i < 1024; i++) { //DEBUG!
    //     Serial.write(temp.data[i]);
    // }
    // Serial.print('\n');
    // uint8_t destCode = destination;
    // const char *dest[64] = {0};
    // const char *data[1024] = {0};
    // *dest = destStr.c_str();
    // *data = dataStr.c_str();
    // uint8_t destLen = destStr.length();
    // uint16_t dataLen = dataStr.length();
    
    // uint32_t blockLen = 4 + destLen + dataLen + adrLenFRAM;
    // uint32_t blockEnd = stackPointer; //New end point, it top of current stack
    // stackPointer = stackPointer - blockLen; //Reduce stack pointer to allocate space

    // fram.put(stackPointer, destCode);
    // stackPointer += sizeof(destCode);
    // fram.put(stackPointer, blockEnd);
    // stackPointer += sizeof(blockEnd);
    // fram.put(stackPointer, dest)

    

    
    return false; //DEBUG!
}

uint32_t KestrelFileHandler::readValFRAM(uint32_t pos, uint8_t len)
{
    uint8_t temp[4] = {0};
    bool read = fram.readData(pos, (uint8_t *)&temp, len); //Read data into array
    uint32_t val = 0;
    for(int i = 0; i < 4; i++) {
        val = val | (temp[i] << i*8); //Concatonate vals from array 
    }
    if(read) {
        return val; //If read correctly, return as normal
    }
    else {
        //FIX! Throw error
        return 0; 
    }
}

bool KestrelFileHandler::dumpFRAM()
{
    logger.enableI2C_Global(false); //Disable external I2C
    logger.enableI2C_OB(true); //Turn on internal I2C
    // uint32_t stackPointer = readValFRAM(memSizeFRAM - adrLenFRAM, adrLenFRAM); //Read from bottom bytes to get position to start actual read from
    uint32_t stackPointer;
    fram.get(memSizeFRAM - sizeof(stackPointer), stackPointer); //Replace updated stack pointer at end of FRAM
    Serial.print("STACK POINTER: "); //DEBUG!
    Serial.println(stackPointer);
    // Serial.print("STACKPOINTER-READ: ");
    // Serial.println(stackPointer);
    // uint32_t blockEnd = 0;
    // uint8_t destCode = 0;
    // uint8_t destLen = 0;
    // uint8_t dest[64] = {0};
    // uint16_t dataLen = 0;
    // uint8_t data[1024] = {0};
    logger.enableSD(true);
    if (!sd.begin(chipSelect, SPI_FULL_SPEED)) { //Initialize SD card, assume power has been cycled since last time
        sd.initErrorHalt(); //DEBUG!??
    }
    // Serial.println("BACKHAUL"); //DEBUG!
    bool sentLocal = false; //Keep track if local storage was success
    bool sentRemote = false; //Keep track is remote sent was success 
    bool sent = true; //Start as true and clear if any sends fail
    
    while(stackPointer < memSizeFRAM) {
        // bool sentTemp = false;
        dataFRAM temp;
        fram.get(stackPointer, temp);
        if(temp.destCode == DestCodes::Both) {
            sentLocal = false; //Default both to false
            sentRemote = false; 
        }
        if(temp.destCode == DestCodes::SD) {
            sentLocal = false;
            sentRemote = true;
        }
        if(temp.destCode == DestCodes::Particle) {
            sentLocal = true;
            sentRemote = false;
        }
        // Serial.print("DEST: ");
        // Serial.print(temp.destCode);
        // Serial.print("\tDEST LEN: ");
        // Serial.print(temp.destLen);
        // Serial.print("\tDATA LEN: ");
        // Serial.print(temp.dataLen);
        // Serial.print("\tBLOCK END: ");
        // Serial.println(temp.blockEnd);
        // for(int i = 0; i < 1024; i++) { //DEBUG!
        //     Serial.write(temp.data[i]);
        // }
        // Serial.print('\n');
        // Serial.write((char*)temp.data, 1024); //DEBUG!
        
        // destCode = readValFRAM(stackPointer, 1); //Read in destination code
        // stackPointer += 1; //Increment pointer
        // blockEnd = readValFRAM(stackPointer, adrLenFRAM);
        // stackPointer += adrLenFRAM;
        // destLen = readValFRAM(stackPointer, 1);
        // stackPointer += 1;
        // fram.readData(stackPointer, (uint8_t *)&dest, destLen); //Read in dest array
        // stackPointer += destLen;
        // dataLen = readValFRAM(stackPointer, 2);
        // stackPointer += 2;
        // fram.readData(stackPointer, (uint8_t *)&data, dataLen); //Read in data array
        
        if(temp.destCode == DestCodes::SD || temp.destCode == DestCodes::Both) {
            String fileName = "";
            // for(int i = 0; i < sizeof(publishTypes); i++) {
            for(int i = 0; i < 4; i++) { //FIX! don't use magic number, checking against sizeof(publishTypes) causes overrun and crash
                // Serial.println("SD String Vals:"); //DEBUG!
                // Serial.write((const uint8_t*)temp.dest, temp.destLen);
                // Serial.print("\n");
                // Serial.println(publishTypes[i]);
                if(strcmp(temp.dest, publishTypes[i].c_str()) == 0) {
                    fileName = filePaths[i]; //Once the destination is found, match it with the destination file
                    // Serial.println("\tMATCH");
                    break; //Exit for loop once match is found
                }
            }
            if (!sdFile.open(fileName, O_RDWR | O_CREAT | O_AT_END)) { //Try to open the specified file
                sd.errorHalt("opening test.txt for write failed");
                sdFile.close();
                sentLocal = false; //Clear global flag
                // sentTemp = false; //Clear flag on fail
                // return false; //Return fail if not able to write
                //FIX! ThrowError!
            }
            else {
                sdFile.write(temp.data, temp.dataLen); //Append data to end
                sdFile.write('\n'); //Place newline at end of each entry
                sentLocal = true;
            }
            sdFile.close(); //Regardless of access, close file when done     
        }
        if(temp.destCode == DestCodes::Particle || temp.destCode == DestCodes::Both) {
            if(!Particle.connected()) {
                //FIX! Throw error
                // return false; //If not connected to the cloud already, throw error and exit with fault
                Serial.println("ERROR: Particle Disconnected"); //DEBUG!
                sentRemote = false; //DEBUG!
            }
            else {
                // Serial.print("PARTICLE PUBLISH: ");
                static unsigned long lastPublish = millis();
                // delay(1000);
                if((millis() - lastPublish) < 1000) { //If less than one second since last publish, wait to not overload particle 
                    Serial.print("Publish Delay: ");
                    Serial.println(1000 - (millis() - lastPublish));
                    delay(1000 - (millis() - lastPublish)); //Wait for the remainder of 1 second in order to space out publish events
                }
                // else {
                sentRemote = Particle.publish((const char*)temp.dest, (const char*)temp.data, WITH_ACK);
                // }
                lastPublish = millis();
                // Serial.println(sent);
                //FIX! If sent fail, throw error
            }
            
        }
        
        
        // if(sent == true) { //Only increment pointer if 
        //     // stackPointer += temp.dataLen + temp.destLen + 5; //Increment length of packet
        //     stackPointer += blockOffset;
        //     fram.put(memSizeFRAM - sizeof(stackPointer), stackPointer); //Replace updated stack pointer at end of FRAM
        //     // Serial.print("STACKPOINTER-READ+: ");
        //     // Serial.println(stackPointer);
        // }
        // else return false; //If any send fails, just break out
        

        if(sentLocal == false && sentRemote == false) { //Whichever method was intended, it failed. Just write it back on the stack
            //Throw error - critical!
            // writeToFRAM(String(temp.data), String(temp.dest), temp.destCode);
            sent = false;
        }
        else if(sentLocal == false && sentRemote == true) { //Failed to write to SD card, but either did not try to write to cell or succeded
            // if(temp.destCode == DestCodes::SD) writeToFRAM(String(temp.data), String(temp.dest), DestCodes::Both); //If we just tried to write to SD and it failed, lets try to write to both to see if we can get the data back at all
            // if(temp.destCode == DestCodes::Both) writeToFRAM(String(temp.data), String(temp.dest), DestCodes::SD); //If we failed to write to the SD, but tried both, just write back to the SD
            if(temp.destCode == DestCodes::SD) fram.writeData(stackPointer, (const uint8_t *)&DestCodes::BothRetry, sizeof(DestCodes::BothRetry)); //If we just tried to write to SD and it failed, lets try to write to both to see if we can get the data back at all
            if(temp.destCode == DestCodes::Both) fram.writeData(stackPointer, (const uint8_t *)&DestCodes::SDRetry, sizeof(DestCodes::SDRetry)); //If we failed to write to the SD, but tried both, just write back to the SD
            sent = false;
        }
        else if(sentLocal == true && sentRemote == false) { //Failed to write to cell, but either did not try to write to SD or succeded
            if(temp.destCode == DestCodes::Particle) fram.writeData(stackPointer, (const uint8_t *)&DestCodes::BothRetry, sizeof(DestCodes::BothRetry)); //If we just tried to write to cell and it failed, lets try to write to both to see if we can get the data back at all
            if(temp.destCode == DestCodes::Both) {
                Serial.print("Cell Backhaul: "); //DEBUG!
                Serial.println(String(temp.data)); //DEBUG!
                // writeToFRAM(String(temp.data), String(temp.dest), DestCodes::Particle); //If we failed to write to the cell, but tried both, just write back to the cell
                // fram.put(stackPointer, DestCodes::Particle);
                fram.writeData(stackPointer, (const uint8_t *)&DestCodes::ParticleRetry, sizeof(DestCodes::ParticleRetry));
            }
            sent = false;
        }
        else if(sentLocal == true && sentRemote == true)  { //If THIS write is good, nullify the destination, regardless of state of previous writes
            // fram.put(stackPointer, DestCodes::None);
            fram.writeData(stackPointer, (const uint8_t *)&DestCodes::None, sizeof(DestCodes::None));
        }
        stackPointer += blockOffset; //Increment local pointer
        if(sentLocal == true && sentRemote == true && sent == true) { //If good write (and have been no failures) pop entry from stack
            fram.put(memSizeFRAM - sizeof(stackPointer), stackPointer); //Replace updated stack pointer at end of FRAM
        }
        
        // sent = sent & sentTemp; //Update pervasive sent 
    }
    logger.enableSD(false); //Turn SD back off
    return sent; //DEBUG!
}

bool KestrelFileHandler::dumpToSD() //In case of FRAM filling up, dumps all entries to unsent log on SD
{
    logger.enableI2C_Global(false); //Disable external I2C
    logger.enableI2C_OB(true); //Turn on internal I2C
    Serial.println("DUMP FRAM TO SD");
    // uint32_t stackPointer = readValFRAM(memSizeFRAM - adrLenFRAM, adrLenFRAM); //Read from bottom bytes to get position to start actual read from
    uint32_t stackPointer;
    fram.get(memSizeFRAM - sizeof(stackPointer), stackPointer); //Get updated stack pointer at end of FRAM
    Serial.print("STACK POINTER: "); //DEBUG!
    Serial.println(stackPointer);
    // Serial.print("STACKPOINTER-READ: ");
    // Serial.println(stackPointer);
    // uint32_t blockEnd = 0;
    // uint8_t destCode = 0;
    // uint8_t destLen = 0;
    // uint8_t dest[64] = {0};
    // uint16_t dataLen = 0;
    // uint8_t data[1024] = {0};
    logger.enableSD(true);
    if (!sd.begin(chipSelect, SPI_FULL_SPEED)) { //Initialize SD card, assume power has been cycled since last time
        //FIX! Throw error!
        //FIX! Write error to EEPROM cause we can't seem to work with SD card or telemetry...
        sd.initErrorHalt(); //DEBUG!??

    }
    // Serial.println("BACKHAUL"); //DEBUG!
    bool sent = true; //Begin as true, clear if any Particle sends fail
    while(stackPointer < memSizeFRAM) {
        // bool sentTemp = false;
        dataFRAM temp;
        fram.get(stackPointer, temp);
        // Serial.print("DEST: ");
        // Serial.print(temp.destCode);
        // Serial.print("\tDEST LEN: ");
        // Serial.print(temp.destLen);
        // Serial.print("\tDATA LEN: ");
        // Serial.print(temp.dataLen);
        // Serial.print("\tBLOCK END: ");
        // Serial.println(temp.blockEnd);
        // for(int i = 0; i < 1024; i++) { //DEBUG!
        //     Serial.write(temp.data[i]);
        // }
        // Serial.print('\n');
        // Serial.write((char*)temp.data, 1024); //DEBUG!
        
        // destCode = readValFRAM(stackPointer, 1); //Read in destination code
        // stackPointer += 1; //Increment pointer
        // blockEnd = readValFRAM(stackPointer, adrLenFRAM);
        // stackPointer += adrLenFRAM;
        // destLen = readValFRAM(stackPointer, 1);
        // stackPointer += 1;
        // fram.readData(stackPointer, (uint8_t *)&dest, destLen); //Read in dest array
        // stackPointer += destLen;
        // dataLen = readValFRAM(stackPointer, 2);
        // stackPointer += 2;
        // fram.readData(stackPointer, (uint8_t *)&data, dataLen); //Read in data array
        

        // String fileName = "";
        // for(int i = 0; i < sizeof(publishTypes); i++) {
        // for(int i = 0; i < 4; i++) { //FIX! don't use magic number, checking against sizeof(publishTypes) causes overrun and crash
        //     // Serial.println("SD String Vals:"); //DEBUG!
        //     // Serial.write((const uint8_t*)temp.dest, temp.destLen);
        //     // Serial.print("\n");
        //     // Serial.println(publishTypes[i]);
        //     if(strcmp(temp.dest, publishTypes[i].c_str()) == 0) {
        //         fileName = filePaths[i]; //Once the destination is found, match it with the destination file
        //         // Serial.println("\tMATCH");
        //         break; //Exit for loop once match is found
        //     }
        // }
        if (!sdFile.open(filePaths[4], O_RDWR | O_CREAT | O_AT_END)) { //Try to open the specified file
            sd.errorHalt("opening test.txt for write failed"); //DEBUG!
            sdFile.close();
            sent = false; //Clear global flag
            // sentTemp = false; //Clear flag on fail
            // return false; //Return fail if not able to write
            //FIX! ThrowError!
        }
        else {
            // if(temp.destCode != DestCodes::None) { //If entry has not already been backhauled //DEBUG!!!!
                sdFile.print(temp.destCode); //Print destination code
                sdFile.write('\t'); //Tab deliniate data
                sdFile.write(temp.dest, temp.destLen); //Write out destination 
                sdFile.print('\t'); //Tab deliniate data
                sdFile.write(temp.data, temp.dataLen); //Write out data
                sdFile.write('\n'); //Place newline at end of each entry
            // }
        }
        sdFile.close(); //Regardless of access, close file when done     
        logger.enableSD(false); //Turn SD back off
        if(sent == true) { //Only increment pointer if 
            // stackPointer += temp.dataLen + temp.destLen + 5; //Increment length of packet
            stackPointer += blockOffset;
            fram.put(memSizeFRAM - sizeof(stackPointer), stackPointer); //Replace updated stack pointer at end of FRAM
            // Serial.print("STACKPOINTER-READ+: ");
            // Serial.println(stackPointer);
        }
        else return false; //If any send fails, just break out //FIX! Throw error
        // sent = sent & sentTemp; //Update pervasive sent 
    }

    return sent; //DEBUG!
}

bool KestrelFileHandler::backhaulUnsentLogs()
{
    logger.enableSD(true);
    if (!sd.begin(chipSelect, SPI_FULL_SPEED)) { //Initialize SD card, assume power has been cycled since last time
        //FIX! Throw error!
        //FIX! Write error to EEPROM cause we can't seem to work with SD card or telemetry...
        sd.initErrorHalt(); //DEBUG!??

    }
    // Serial.println("BACKHAUL"); //DEBUG!
    bool sent = true; //Begin as true, clear if any Particle sends fail
    bool sentLocal = false; //Keep track if local storage was success
    bool sentRemote = false; //Keep track is remote sent was success 
    dataFRAM temp; //Instantiate struct to use as storage location
    
    // dataFRAM temp = {destination, blockEnd, destStr.length(), {0}, dataStr.length(), {0}};
    // strcpy(temp.dest, destStr.c_str());
    // strcpy(temp.data, dataStr.c_str());
    File tempFile;
    File normalLogFile;
    if (!sdFile.open(filePaths[4], O_RDONLY) || !tempFile.open(filePaths[5], O_RDWR | O_CREAT | O_AT_END)) { //Open existing file as read only (then delete at the end), open temp file normally 
            sd.errorHalt("opening backhaul file or temp file failed"); //DEBUG!
            sdFile.close();
            tempFile.close();
            sent = false; //Clear global flag
            // sentTemp = false; //Clear flag on fail
            // return false; //Return fail if not able to write
            //FIX! ThrowError!
    }
    while(sdFile.available() > 0) { //While there is still data to read
        
        // else {
            char destCodeStr[5] = {0}; //Allow for max of 3 numbers (plus null terminator, plus one to make sure it reads far enough - see fgets doc), since max val is 255
            sdFile.fgets(destCodeStr, sizeof(destCodeStr), "\t"); //Grab destination code
            temp.destCode = atoi(destCodeStr); //Convert to int and pass on to temp struct
            // temp.destCode = temp.destCode & 0x0F; //Clear top nibble in order to read retry and normal as the same
            temp.destLen = sdFile.fgets(temp.dest, sizeof(temp.dest), "\t"); //Grab destination string //FIX! Test for failure
            temp.destLen = strcspn(temp.dest, "\n\t"); //Find first occourance of tab or newline (this is the end of the string, and the new length)
            temp.dest[temp.destLen] = 0; //Remove any trailing stuff (tabs or newline)
            // temp.dest[strcspn(temp.dest, "\n")] = 0; //Remove newline from returned value
            temp.dataLen = sdFile.fgets(temp.data, sizeof(temp.data), "\n"); //Grab data string //FIX! Test for failure
            temp.dataLen = strcspn(temp.data, "\n\t"); //Find first occourance of tab or newline (this is the end of the string, and the new length)
            temp.data[temp.dataLen] = 0; //Remove any trailing stuff (tabs or newline)
            
            // temp.data[strcspn(temp.data, "\n\t")] = 0; //Remove any trailing stuff (tabs or newline)
            // temp.data[strcspn(temp.data, "\n")] = 0; //Remove newline from returned value
            Serial.print("Dest Code: "); //DEBUG!
            Serial.print(temp.destCode);
            Serial.print("\tDest: ");
            Serial.print(String(temp.dest));
            Serial.print("\tData: ");
            Serial.println(String(temp.data));
            if(temp.destCode == DestCodes::Both || temp.destCode == DestCodes::BothRetry) {
                sentLocal = false; //Default both to false
                sentRemote = false; 
            }
            if(temp.destCode == DestCodes::SD || temp.destCode == DestCodes::SDRetry) {
                sentLocal = false;
                sentRemote = true;
            }
            if(temp.destCode == DestCodes::Particle || temp.destCode == DestCodes::ParticleRetry) {
                sentLocal = true;
                sentRemote = false;
            }
            //Once read in, follow normal backhaul process
            if(temp.destCode == DestCodes::SD || temp.destCode == DestCodes::SDRetry || temp.destCode == DestCodes::Both || temp.destCode == DestCodes::BothRetry) { //First, write to SD file if needed
                String fileName = "";
                // for(int i = 0; i < sizeof(publishTypes); i++) {
                for(int i = 0; i < 4; i++) { //FIX! don't use magic number, checking against sizeof(publishTypes) causes overrun and crash
                    // Serial.println("SD String Vals:"); //DEBUG!
                    // Serial.write((const uint8_t*)temp.dest, temp.destLen);
                    // Serial.print("\n");
                    // Serial.println(publishTypes[i]);
                    if(strcmp(temp.dest, publishTypes[i].c_str()) == 0) {
                        fileName = filePaths[i]; //Once the destination is found, match it with the destination file
                        // Serial.println("\tMATCH");
                        Serial.print("SD File Dest: "); //DEBUG!
                        Serial.println(fileName);
                        break; //Exit for loop once match is found
                    }
                }
                if (!normalLogFile.open(fileName, O_RDWR | O_CREAT | O_AT_END)) { //Try to open the specified file
                    sd.errorHalt("opening normal log file failed");
                    normalLogFile.close();
                    sentLocal = false; //Clear global flag
                    // sentTemp = false; //Clear flag on fail
                    // return false; //Return fail if not able to write
                    //FIX! ThrowError!
                }
                else {
                    Serial.print("SD Backhaul to: "); //DEBUG!
                    Serial.println(fileName);
                    normalLogFile.write(temp.data, temp.dataLen); //Append data to end
                    normalLogFile.write('\n'); //Place newline at end of each entry
                    sentLocal = true;
                }
                normalLogFile.close(); //Regardless of access, close file when done     
                // if(temp.destCode == DestCodes::Both && sentLocal == true) temp.destCode = DestCodes::Particle; //If succeded in writing to SD, set destCode just to particle
                // if(temp.destCode == DestCodes::BothRetry && sentLocal == true) temp.destCode = DestCodes::BothRetry; //if succeded in switing to SD from retry val, set destCode to retry particle 
            }
            if(temp.destCode == DestCodes::Particle || temp.destCode == DestCodes::ParticleRetry || temp.destCode == DestCodes::Both || temp.destCode == DestCodes::BothRetry) {
                if(!Particle.connected()) {
                    //FIX! Throw error
                    // return false; //If not connected to the cloud already, throw error and exit with fault
                    // Serial.println("ERROR: Particle Disconnected"); //DEBUG!
                    sentRemote = false; //DEBUG!
                }
                else {
                    Serial.print("Particle Backhaul");
                    static unsigned long lastPublish = millis();
                    // delay(1000);
                    if((millis() - lastPublish) < 1000) { //If less than one second since last publish, wait to not overload particle 
                        // Serial.print("Publish Delay: ");
                        // Serial.println(1000 - (millis() - lastPublish));
                        delay(1000 - (millis() - lastPublish)); //Wait for the remainder of 1 second in order to space out publish events
                    }
                    // else {
                    sentRemote = Particle.publish((const char*)temp.dest, (const char*)temp.data, WITH_ACK);
                    // }
                    lastPublish = millis();
                    // Serial.println(sent);
                    //FIX! If sent fail, throw error
                }
                // if(temp.destCode == DestCodes::Both && sentLocal == true) temp.destCode = DestCodes::Particle; //If succeded in writing to SD, set destCode just to particle
                // if(temp.destCode == DestCodes::BothRetry && sentLocal == true) temp.destCode = DestCodes::BothRetry; //if succeded in switing to SD from retry val, set destCode to retry particle 
                
            }

            if(sentLocal == false && sentRemote == false) { //Whichever method was intended, it failed. Just write it back on the stack
                //Throw error - critical!
                // writeToFRAM(String(temp.data), String(temp.dest), temp.destCode);
                sent = false;
            }
            else if(sentLocal == false && sentRemote == true) { //Failed to write to SD card, but either did not try to write to cell or succeded
                // if(temp.destCode == DestCodes::SD) writeToFRAM(String(temp.data), String(temp.dest), DestCodes::Both); //If we just tried to write to SD and it failed, lets try to write to both to see if we can get the data back at all
                // if(temp.destCode == DestCodes::Both) writeToFRAM(String(temp.data), String(temp.dest), DestCodes::SD); //If we failed to write to the SD, but tried both, just write back to the SD
                temp.destCode = DestCodes::SDRetry; //If cell works, but SD does not (really weird here), just tell it to try SD again
                // if(temp.destCode == DestCodes::SD || temp.destCode == DestCodes::SDRetry) temp.destCode = DestCodes::SDRetry; //If we just tried to write to SD and it failed, lets try to write to both to see if we can get the data back at all
                // if(temp.destCode == DestCodes::SD) temp.destCode = DestCodes::BothRetry; //If this was the first attempt to send this, have it 
                // if(temp.destCode == DestCodes::Both || temp.destCode == DestCodes::BothRetry) temp.destCode = DestCodes::SDRetry; //If we failed to write to the SD, but tried both, just write back to the SD
                sent = false;
            }
            else if(sentLocal == true && sentRemote == false) { //Failed to write to cell, but either did not try to write to SD or succeded
                temp.destCode = DestCodes::ParticleRetry; //If SD works but cell fails, just try cell again
                // if(temp.destCode == DestCodes::Particle) fram.writeData(stackPointer, (const uint8_t *)&DestCodes::BothRetry, sizeof(DestCodes::BothRetry)); //If we just tried to write to cell and it failed, lets try to write to both to see if we can get the data back at all
                // if(temp.destCode == DestCodes::Both) {
                //     Serial.print("Cell Backhaul: "); //DEBUG!
                //     Serial.println(String(temp.data)); //DEBUG!
                //     // writeToFRAM(String(temp.data), String(temp.dest), DestCodes::Particle); //If we failed to write to the cell, but tried both, just write back to the cell
                //     // fram.put(stackPointer, DestCodes::Particle);
                //     fram.writeData(stackPointer, (const uint8_t *)&DestCodes::ParticleRetry, sizeof(DestCodes::ParticleRetry));
                // }
                // sent = false;
            }
            else if(sentLocal == true && sentRemote == true)  { //If THIS write is good, nullify the destination, regardless of state of previous writes
                // fram.put(stackPointer, DestCodes::None);
                // fram.writeData(stackPointer, (const uint8_t *)&DestCodes::None, sizeof(DestCodes::None));
                temp.destCode = DestCodes::None; //If both pass, set entry as completed!
            }
            // stackPointer += blockOffset; //Increment local pointer
            // if(sentLocal == true && sentRemote == true && sent == true) { //If good write (and have been no failures) pop entry from stack
            //     fram.put(memSizeFRAM - sizeof(stackPointer), stackPointer); //Replace updated stack pointer at end of FRAM
            // }
            if(temp.destCode != DestCodes::None) { //If entry has not been backhauled correctly, write it over to the temp file
                tempFile.print(temp.destCode); //Print destination code
                tempFile.write('\t'); //Tab deliniate data
                tempFile.write(temp.dest, temp.destLen); //Write out destination 
                tempFile.print('\t'); //Tab deliniate data
                tempFile.write(temp.data, temp.dataLen); //Write out data
                tempFile.write('\n'); //Place newline at end of each entry
            }
        // }
    } 

    sdFile.close(); //Regardless of access, close file when done     
    tempFile.close();
    if(sent) {
        Serial.println("File Swap!"); //DEBUG!
        sd.remove(filePaths[4]); //Delete unsent file
        sd.rename(filePaths[5], filePaths[4]); //Rename temp file to unsent file
        sd.remove(filePaths[5]); //Delete temp file
    }
    logger.enableSD(false); //Power SD back off
    return true; //DEBUG!
}

bool KestrelFileHandler::eraseFRAM()
{
    return fram.erase();
}

String KestrelFileHandler::selfDiagnostic(uint8_t level)
{
    return ""; //DEBUG!
}

void KestrelFileHandler::dateTimeSD(uint16_t* date, uint16_t* time) {
//  DateTime now = RTC.now();
//  sprintf(timestamp, "%02d:%02d:%02d %2d/%2d/%2d \n", now.hour(),now.minute(),now.second(),now.month(),now.day(),now.year()-2000);
//  Serial.println("yy");
//  Serial.println(timestamp);

uint8_t source = selfPointer->logger.updateTime();
if(source > TimeSource::NONE) { //Only write time back if time is legit
    Serial.print("SD Date: "); //DEBUG!
    Serial.print(selfPointer->logger.currentDateTime.year);
    Serial.print("/");
    Serial.print(selfPointer->logger.currentDateTime.month);
    Serial.print("/");
    Serial.print(selfPointer->logger.currentDateTime.day);
    Serial.print("\t");
    Serial.print(selfPointer->logger.currentDateTime.hour);
    Serial.print(":");
    Serial.print(selfPointer->logger.currentDateTime.minute);
    Serial.print(":");
    Serial.print(selfPointer->logger.currentDateTime.second);
    Serial.print(" - ");
    Serial.println(selfPointer->logger.currentDateTime.source);
    // return date using FAT_DATE macro to format fields
    *date = FAT_DATE(selfPointer->logger.currentDateTime.year, selfPointer->logger.currentDateTime.month, selfPointer->logger.currentDateTime.day);

    // return time using FAT_TIME macro to format fields
    *time = FAT_TIME(selfPointer->logger.currentDateTime.hour, selfPointer->logger.currentDateTime.minute, selfPointer->logger.currentDateTime.second);
}

}

void KestrelFileHandler::dateTimeSD_Glob(uint16_t* date, uint16_t* time) {selfPointer->dateTimeSD(date, time);}