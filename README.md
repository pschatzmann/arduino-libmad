# A MP3 Decoder using libmad

MAD is a high-quality MPEG audio decoder. It currently supports MPEG-1 and the MPEG-2 extension to lower sampling frequencies, as well as the de facto MPEG 2.5 format. All three audio layers — Layer I, Layer II, and Layer III (i.e. MP3) — are fully implemented. 

I am providing the [MAD MP3 decoder from Underbit](hhttps://www.underbit.com/products/mad/) as a simple Arduino Library. Libmad is based on callbacks and therefore the most efficient way to use this library outside of Arduino is by using the callback methods for providing and processing the data. In Arduino we can alternatively use Streams instead. Finally you can also use the write method to provide the mp3 data, but this involves quite some overhead.

MP3 is a compressed audio file formats based on PCM. A 2.6 MB wav file can be compressed down to 476 kB MP3.

This project can be used stand alone or together with the [arduino-audio_tools library](https://github.com/pschatzmann/arduino-audio-tools). It can also be used from non Arduino based systems with the help of cmake.


### Callback API Example

The API provides the decoded data to a Arduino Stream or alternatively to a callback function. Here is a MP3 example using the callback:

```
#include "MP3DecoderMAD.h"
#include "BabyElephantWalk60_mp3.h"

using namespace libmad;

// Callback for decoded result
void pcmDataCallback(MadAudioInfo &info, int16_t *pwm_buffer, size_t len) {
    for (size_t i=0; i<len; i+=info.channels){
        for (int j=0;j<info.channels;j++){
            Serial.print(pwm_buffer[i+j]);
            Serial.print(" ");
        }
        Serial.println();
    }
}

// Callback to provide data
InputData inputDataCallback(){
    InputData data = {BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len};
    return data;
}

MP3DecoderMAD mp3(pcmDataCallback);

void setup() {
    mp3.setInputCallback(inputDataCallback);
    Serial.begin(115200);
    mp3.begin();
}

void loop() {
    // restart from the beginning
    delay(2000);
    if (!mp3) {
        mp3.begin();
    }
}

```
### Installation

In Arduino, you can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone pschatzmann/arduino-libmad.git

```

This project can also be built and executed on your desktop with cmake:

```
cd arduino-libmad
mkdir build
cd build
cmake ..
make
```
  

### Documentation

The [class documentation can be found here](https://pschatzmann.github.io/arduino-libmad/html/annotated.html)




