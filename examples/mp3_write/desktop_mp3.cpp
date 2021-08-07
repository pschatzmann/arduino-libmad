
#ifdef EMULATOR
#include "Arduino.h"
#include "mp3_write.ino"

int main(){
    setup();
    while(true){
        loop();
    }
    return -1;
}

#endif