#pragma once

#include "libmad/mad.h"
#include "mad_log.h"
#include <stdint.h>

namespace libmad {


/**
 * @brief Basic Audio Information (number of hannels, sample rate)
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
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

// Callback methods
typedef void (*MP3DataCallback)(MadAudioInfo &info,short *pwm_buffer, size_t len);
typedef void (*MP3InfoCallback)(MadAudioInfo &info);
MP3DataCallback pwmCallback = nullptr;
MP3InfoCallback infoCallback = nullptr;
#ifdef ARDUINO
Print *mad_output_stream = nullptr;
#endif


/**
 * @brief Individual chunk of encoded MP3 data which is submitted to the decoder
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct MadInputBuffer {
    uint8_t* data=nullptr;
    size_t size=0;
    bool cleanup = false;

    MadInputBuffer() = default;

    MadInputBuffer(void* data, size_t len){
        this->data = (uint8_t*) data;
        this->size = len;    
    }

    MadInputBuffer(const void* data, size_t len){
        this->data = (uint8_t*)data;
        this->size = len;    
    }
};



/**
 * @brief A simple Arduino API for the libMAD MP3 decoder. The data is provided with the help of write() calls.
 * The decoded result is available either via a callback method or via an mad_mad_output_streamput_streamput stream.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3DecoderMAD  {

    public:

        MP3DecoderMAD(){
        }

        ~MP3DecoderMAD(){
            end();
            if (buffer.data!=nullptr){
                delete [] buffer.data;
                buffer.data = nullptr;
            }

        }

        MP3DecoderMAD(MP3DataCallback dataCallback, MP3InfoCallback infoCB=nullptr){
            setDataCallback(dataCallback);
            setInfoCallback(infoCB);
        }

        /**
         * @brief Set the maximum buffer size which is used for parsing the data
         * 
         * @param size 
         */
        void setBufferSize(size_t size){
            max_buffer_size = size;
        }

#ifdef ARDUINO
        MP3DecoderMAD(Print &mad_output_streamput, MP3InfoCallback infoCB = nullptr){
            setOutput(mad_output_streamput);
            setInfoCallback(infoCB);
        }

        void setOutput(Print &out){
            mad_output_stream = &out;
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

        // mad low lever interface - start
        void begin() {
            if (buffer.data==nullptr){
                buffer.data = new uint8_t[max_buffer_size];
            } 
            if (active){
                end();
            }
            mad_stream_init(&stream);
            mad_frame_init(&frame);
            mad_synth_init(&synth);

            next_frame=nullptr;
            this_frame=nullptr;
            active = true;
            buffer.size = 0;
        }

        // mad low lever interface - end
        void end(){
            if (active){
                mad_synth_finish(&synth);
                mad_frame_finish(&frame);
                mad_stream_finish(&stream);
                active = false;
            }
        }

        /// Provides the last valid audio information
        MadAudioInfo audioInfo(){
            return mad_info;
        }

        /// Makes the mp3 data available for decoding: however we recommend to provide the data via a callback or input stream
        size_t write(const void *in_ptr, size_t in_size) {
            size_t result = 0;
            if (active){
                LOG(Debug, "write %zu", in_size);
                size_t start = 0;
                uint8_t* ptr8 = (uint8_t* )in_ptr;
                // we can not write more then the AAC_MAX_FRAME_SIZE 
                size_t write_len = min(in_size, max_buffer_size);
                while(start<in_size){
                        // we have some space left in the buffer
                    int written_len = writeBuffer(ptr8+start, write_len);
                    start += written_len;
                    LOG(Info,"-> Written %zu of %zu", start, in_size);
                    write_len = min(in_size - start, max_buffer_size);
    #ifdef ARDUINO                    
                    yield();
    #endif
                }
                result = start;
            }    
            return result;
        }

        /// Returns true as long as we are processing data
        operator bool(){
            return active;
        }

    protected:
        size_t max_buffer_size = 1024;
        bool active;
        struct mad_stream stream;
        struct mad_frame frame;
        struct mad_synth synth;

        uint8_t* next_frame=nullptr;
        uint8_t* this_frame=nullptr;

        MadInputBuffer buffer;
        MadAudioInfo mad_info;

        /**
         * @brief Uses MAD low level interface to process a submitted buffer
         * 
         * @param data 
         * @param len 
         * @return size_t 
         */
        size_t writeBuffer(uint8_t *data, size_t len){
            // keep unprocessed data in buffer
            int start = 0;
            if (this_frame!=nullptr){
                int consumed = this_frame - buffer.data;
                if (consumed<=0){
                    consumed = next_frame - buffer.data;
                }

                assert(consumed<max_buffer_size);
                assert(consumed>=0);

                if (consumed>0){
                    buffer.size -= consumed;
                    start = buffer.size;
                    memmove(buffer.data, buffer.data+consumed, buffer.size);
                } 
            }

            // append data to buffer
            int write_len = min(len, max_buffer_size - buffer.size);
            memmove(buffer.data+start, data, write_len);
            buffer.size += write_len;

            // submit it to MAD
            mad_stream_buffer(&stream, buffer.data, buffer.size);

            int rc = 0;
            bool had_output = false;
            while (rc == 0) {
                this_frame = (uint8_t *) stream.this_frame;
                next_frame = (uint8_t *) stream.next_frame;

                // decode
                rc = mad_frame_decode(&frame, &stream);
                if (rc!=0){
                    // resynchronize and decode
                    mad_stream_sync(&stream);
                    rc = mad_frame_decode(&frame, &stream);
                }
                if (rc==0){
                    mad_synth_frame(&synth, &frame);
                    if (synth.pcm.length>0){
                        output(this, &frame.header, &synth.pcm);
                        had_output = true;
                    }
                }
            }

            if (write_len==0 && !had_output){
                // request to resend data
                write_len = 0;
                // ingore current buffer
                buffer.size = 0;
                LOG(Warning, "data was ignored");
            } 

            return write_len;
        }

        /*
        * This is the mad_output_streamput callback function. It is called after each frame of
        * MPEG audio data has been completely decoded. The purpose of this callback
        * is to mad_output_streamput (or play) the decoded PCM audio.
        */

        void output(void *data, struct mad_header const *header, struct mad_pcm *pcm) {
            LOG(Debug, "output");
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
            int16_t result[nsamples*nchannels];
            int i = 0;
            for (int j=0;j<nsamples;j++){
                for (int ch = 0;ch<nchannels;ch++){
                    result[i++] = scale(pcm->samples[ch][j]);
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
        }

        /**
         * @brief 
         * 
         * @param sample 
         * @return int16_t 
         */
        static int16_t scale(mad_fixed_t sample) {
            /* round */
            if(sample>=MAD_F_ONE)
                return(SHRT_MAX);
            if(sample<=-MAD_F_ONE)
                return(-SHRT_MAX);

            /* Conversion. */
            sample=sample>>(MAD_F_FRACBITS-15);
            return((signed short)sample);
        }
};

}
