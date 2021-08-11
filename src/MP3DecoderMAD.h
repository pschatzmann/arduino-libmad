#pragma once

#include "mad_config.h"
#include "libmad/mad.h"
#include "mad_log.h"
#include <stdint.h>

namespace libmad {

/**
 * @brief Basic Info abmad_output_stream the Audio Data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#define MAD_READ_DATA_MAX_SIZE 1024

struct MadAudioInfo {
    MadAudioInfo() = default;
    MadAudioInfo(const MadAudioInfo&) = default;
    MadAudioInfo(mad_pcm &pcm){
        sample_rate = pcm.samplerate;
        channels = pcm.channels;
    }
    int sample_rate = 0;    // undefined
    int channels = 0;       // undefined
    int bits_per_sample=16; // we assume int16_t

    bool operator==(MadAudioInfo alt){
        return sample_rate==alt.sample_rate && channels == alt.channels && bits_per_sample == alt.bits_per_sample;
    }
    bool operator!=(MadAudioInfo alt){
        return !(*this == alt);
    }       
};

/**
 * @brief Individual chunk of encoded MP3 data which is submitted to the decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct MadInputData {
    uint8_t* data=nullptr;
    size_t size=0;
    bool cleanup = false;

    MadInputData() = default;

    MadInputData(void* data, size_t len){
        this->data = (uint8_t*) data;
        this->size = len;    
    }

    MadInputData(const void* data, size_t len){
        this->data = (uint8_t*)data;
        this->size = len;    
    }

};

/// Callbacks
typedef void (*MP3DataCallback)(MadAudioInfo &info,short *pwm_buffer, size_t len);
typedef void (*MP3InfoCallback)(MadAudioInfo &info);
typedef MadInputData (*MP3MadInputDataCallback)();

/// Global Data to be used by static callback methods
MadInputData mad_buffer;
MP3DataCallback pwmCallback = nullptr;
MP3InfoCallback infoCallback = nullptr;
MP3MadInputDataCallback inputCallback = nullptr;
bool is_mad_running = false;
bool is_mad_stopped = true;
MadAudioInfo mad_info;

#ifdef ARDUINO
Stream *mad_input_stream = nullptr;
Print *mad_output_stream = nullptr;
#endif

/**
 * @brief A simple Arduino API for the libMAD MP3 decoder. The data is provided with the help of write() calls.
 * The decoded result is available either via a callback method or via an mad_mad_output_streamput_streamput stream.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3DecoderMAD  {

    public:

        MP3DecoderMAD(){
        }

        ~MP3DecoderMAD(){
            if (mad_buffer.cleanup && mad_buffer.data != nullptr){
                delete [] mad_buffer.data;
            }
        }

        MP3DecoderMAD(MP3DataCallback dataCallback, MP3InfoCallback infoCB=nullptr){
            setDataCallback(dataCallback);
            setInfoCallback(infoCB);
        }

#ifdef ARDUINO
        MP3DecoderMAD(Print &mad_output_streamput, MP3InfoCallback infoCB = nullptr){
            setOutput(mad_output_streamput);
            setInfoCallback(infoCB);
        }

        void setOutput(Print &out){
            mad_output_stream = &out;
        }

        void setInput(Stream &input){
            mad_input_stream = &input;
        }
#endif

        /// Defines the callback which receives the decoded data
        void setDataCallback(MP3DataCallback cb){
            pwmCallback = cb;
        }

        /// Defines the callback which receives the Info changes
        void setInfoCallback(MP3InfoCallback cb){
            infoCallback = cb;
        }

        /// Defines the callback which provides input data
        void setInputCallback(MP3MadInputDataCallback input){
            inputCallback = input;
        }

         /// Starts the processing
        void begin(){
            LOG(Debug, "begin");
            is_mad_running = false;
            is_mad_stopped = false;
            mad_decoder_init(&decoder, this,
                    input, 0 /* header */, 0 /* filter */, output,
                    error, 0 /* message */);

            // if we got an input callback
            if (inputCallback!=nullptr){
                mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
                is_mad_running = true;
            }

#ifdef ARDUINO
            // if we got an input stream we start the decoding
            if (mad_input_stream!=nullptr){
                mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
                is_mad_running = true;
            }
#endif
        }

        /// Releases the reserved memory
        void end(){
            LOG(Debug, "end");
            mad_decoder_finish(&decoder);
            is_mad_running = false;
            is_mad_stopped = true;
        }

        /// Provides the last valid audio information
        MadAudioInfo audioInfo(){
            return mad_info;
        }

        /// Makes the mp3 data available for decoding: however we recommend to provide the data via a callback or input stream
        size_t write(const void *data, size_t len){
            return write((void *)data, len);
        }

        /// Makes the mp3 data available for decoding: however we recommend to provide the data via a callback or input stream
        size_t write(void *data, size_t len){
            LOG(Debug, "write: %lu", len);

            mad_buffer.data = (uint8_t*)data;
            mad_buffer.size = len;
            // start the decoder after we have some data
            if (!is_mad_running){
                mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
                is_mad_running = true;
            }
            return len;
        }

        /// Returns true as long as we are processing data
        operator bool(){
            return !is_mad_stopped;
        }

    protected:
        struct mad_decoder decoder;
        /*
        * This is the input callback. The purpose of this callback is to (re)fill
        * the stream buffer which is to be decoded. We try to refill it 1) from the 
        * callback, 2) from the arduino stream or 3) from the buffer
        */

        static enum mad_flow input(void *data, struct mad_stream *stream) {
            LOG(Debug, "input");
            MP3DecoderMAD *mad = (MP3DecoderMAD *) data;

            if (inputCallback!=nullptr){
                MadInputData input = inputCallback();
                mad_stream_buffer(stream, (uint8_t*)input.data, input.size);
                return MAD_FLOW_CONTINUE;
            }

#ifdef ARDUINO
            // We take the data from the input stream by filling the buffer
            if (mad_input_stream!=nullptr){
                // allocate buffer
                if (mad_buffer.data==nullptr){
                    mad_buffer.data = new uint8_t[MAD_READ_DATA_MAX_SIZE];
                    mad_buffer.cleanup = true;
                }
                // Make data available
                int max_len = min(MAD_READ_DATA_MAX_SIZE, mad_input_stream->available());
                int len_read = mad_input_stream->readBytes(mad_buffer.data, max_len);
                mad_buffer.size = len_read;
            }
#endif            

            // If buffer is empty we stop to give the system the chance to provide more data
            int len = mad_buffer.size;
            if (len==0) {
                is_mad_stopped = true;
                return MAD_FLOW_STOP;
            }

            // provide data from buffer
            mad_stream_buffer(stream, (const uint8_t*)mad_buffer.data, mad_buffer.size);

            // mark data as consumed
            mad_buffer.size = 0;
            return MAD_FLOW_CONTINUE;
        }


        /*
        * This is the mad_output_streamput callback function. It is called after each frame of
        * MPEG audio data has been completely decoded. The purpose of this callback
        * is to mad_output_streamput (or play) the decoded PCM audio.
        */

        static enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm) {
            LOG(Debug, "output");
            MP3DecoderMAD *mad = (MP3DecoderMAD *) data;
            unsigned int nchannels, nsamples;
            mad_fixed_t const *left_ch, *right_ch;
            MadAudioInfo act_info(*pcm);
            
            /// notify abmad_output_stream changes
            if (act_info != mad_info){
                if (infoCallback!=nullptr){
                    infoCallback(act_info);
                }
                mad_info = act_info;
            }

            // convert to int16_t
            nchannels = pcm->channels;
            nsamples  = pcm->length;
            int16_t result[nchannels*nsamples];
            int i = 0;
            for (int j=0;j<nsamples;j++){
                for (int ch = 0;ch<nchannels;ch++){
                    result[i++] = pcm->samples[0][j];
                }
            }

            // return result via callback
            if (pwmCallback!=nullptr){
                pwmCallback(act_info, result, nchannels*nsamples);
            }

#ifdef ARDUINO
            // return result via stream
            if (mad_output_stream!=nullptr){
                mad_output_stream->write((uint8_t*)result, nchannels*nsamples*sizeof(int16_t));
            }
#endif
            return MAD_FLOW_CONTINUE;
        }

        /*
        * This is the error callback function. It is called whenever a decoding
        * error occurs. The error is indicated by stream->error; the list of
        * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
        * header file.
        */

        static enum mad_flow error(void *data, struct mad_stream *stream, struct mad_frame *frame) {
            char msg[80];
            sprintf(msg, "decoding error 0x%04x (%s)", stream->error, mad_stream_errorstr(stream));
            LOG(Error, msg);

            /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
            return MAD_FLOW_CONTINUE;
        }

};

}