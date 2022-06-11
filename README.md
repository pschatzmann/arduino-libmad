# A MP3 Decoder using libmad

[MAD from Underbit](https://www.underbit.com/products/mad/) is a high-quality MPEG audio decoder. It currently supports MPEG-1 and the MPEG-2 extension to lower sampling frequencies, as well as the de facto MPEG 2.5 format. All three audio layers — Layer I, Layer II, and Layer III (i.e. MP3) — are fully implemented. MP3 is a compressed audio file formats based on PCM. A 2.6 MB wav file can be compressed down to 476 kB MP3.

I am providing the [MAD MP3 decoder](hhttps://www.underbit.com/products/mad/) as a __Arduino Library__ with a __simple Arduino Style API__. 

This project can be used stand alone or together with the [arduino-audio_tools library](https://github.com/pschatzmann/arduino-audio-tools). It can also be used __outside of Arduino with the help of cmake__.


### Callback API Example

The API provides the decoded data to a Arduino Stream or alternatively to a callback function. Here is a MP3 example using the callback:

```

#include "MP3DecoderMAD.h"
#include "BabyElephantWalk60_mp3.h"

using namespace libmad;

void pcmDataCallback(MadAudioInfo &info, int16_t *pwm_buffer, size_t len) {
    for (size_t i=0; i<len; i+=info.channels){
        for (int j=0;j<info.channels;j++){
            Serial.print(pwm_buffer[i+j]);
            Serial.print(" ");
        }
        Serial.println();
    }
}

MP3DecoderMAD mp3(pcmDataCallback);

void setup() {
    Serial.begin(115200);
    mp3.begin();
}

void loop() {
    Serial.println("writing...");
    mp3.write(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);    

    // restart from the beginning
    delay(2000);
    mp3.begin();
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

- The [Class Documentation can be found here](https://pschatzmann.github.io/arduino-libmad/html/classlibmad_1_1_m_p3_decoder_m_a_d.html). 
- I also suggest that you have a look at [my related Blog](https://www.pschatzmann.ch/home/2021/08/13/audio-decoders-for-microcontrollers/)

I recommend to use this library together with my [Arduino Audio Tools](https://github.com/pschatzmann/arduino-audio-tools). 
This is just one of many __codecs__ that I have collected so far: Further details can be found in the [Encoding and Decoding Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/Encoding-and-Decoding-of-Audio) of the Audio Tools.


