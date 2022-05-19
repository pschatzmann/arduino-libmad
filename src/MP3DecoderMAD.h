#pragma once

#include "libmad/mad.h"
#include "mad_log.h"
#include <stdint.h>
#include <climits>
#include <cassert>

namespace libmad {

#ifndef MAD_MAX_RESULT_BUFFER_SIZE 
#define MAD_MAX_RESULT_BUFFER_SIZE 1024
#endif

#ifndef MAD_MAX_BUFFER_SIZE 
#define MAD_MAX_BUFFER_SIZE 1024
#endif

/**
 * @brief Basic Audio Information (number of channels, sample rate)
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

/**
 * @brief Range with a start and an end
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
struct Range {
    int start;
    int end;
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
 * The decoded result is available either via a callback method or via an Arduino stream.
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
            if (p_result_buffer!=nullptr){
                delete [] p_result_buffer;
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

        /**
         * @brief Set the Result Buffer Size in samples
         * 
         * @param size 
         */
        void setResultBufferSize(size_t size){
            max_result_buffer_size = size;
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
            if (p_result_buffer==nullptr){
                p_result_buffer = new int16_t[max_result_buffer_size];
            }
            if (active){
                end();
            }
            mad_stream_init(&stream);
            mad_frame_init(&frame);
            mad_synth_init(&synth);

            active = true;
            buffer.size = 0;
            frame_counter = 0;
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
                    int written_len = writeFrame(ptr8+start, write_len);
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
        size_t max_buffer_size = MAD_MAX_BUFFER_SIZE;
        size_t max_result_buffer_size = MAD_MAX_RESULT_BUFFER_SIZE;
        size_t frame_counter = 0;
        bool active = false;
        struct mad_stream stream;
        struct mad_frame frame;
        struct mad_synth synth;

        MadInputBuffer buffer;
        MadAudioInfo mad_info;
        int16_t *p_result_buffer = nullptr;

        /**
         * @brief Finds the MP3 synchronization word which demarks the start of a new segment
         * 
         * @param offset 
         * @return int 
         */

        int findSyncWord(int offset) {
            for (int j=offset;j<buffer.size-1;j++){
                if ((buffer.data[j] == 0xff && (buffer.data[j+1] & 0xe0) == 0xe0)){
                    return j;
                }
            }
            return -1;
        }


        /// Decodes a frame
        virtual void decode(Range r) {
            mad_stream_buffer(&stream, buffer.data+r.start, buffer.size-r.start);

            int rc = mad_frame_decode(&frame, &stream);
            if (rc==0){
                mad_synth_frame(&synth, &frame);
                if (synth.pcm.length>0){
                    output(this, &frame.header, &synth.pcm);
                }

                int decoded = (stream.next_frame - buffer.data);
                assert(decoded>0);
                buffer.size -= decoded;
                assert(buffer.size<=maxFrameSize());
                memmove(buffer.data, buffer.data+r.start+decoded, buffer.size);

            } else {
                LOG(Warning,"-> decoding error");
                int ignore = r.end;
                buffer.size -= ignore;
                assert(buffer.size<=maxFrameSize());
                memmove(buffer.data, buffer.data+ignore, buffer.size);

            }
        }  

        /// we add the data to the buffer until it is full
        size_t appendToBuffer(const void *in_ptr, int in_size){
            LOG(Info, "appendToBuffer: %d (at %p)", in_size, buffer.data);
            int buffer_size_old = buffer.size;
            int process_size = min((int)(maxFrameSize() - buffer.size), in_size);
            memmove(buffer.data+buffer.size, in_ptr, process_size); 
            buffer.size += process_size;
            assert(buffer.size<=maxFrameSize());

            LOG(Debug, "appendToBuffer %d + %d  -> %u", buffer_size_old,  process_size, buffer.size );
            return process_size;
        }

        /// appends the data to the frame buffer and decodes 
        size_t writeFrame(const void *in_ptr, size_t in_size){
            LOG(Debug, "writeFrame %zu", in_size);
            size_t result = 0;
            // in the beginning we ingnore all data until we found the first synch word
            result = appendToBuffer(in_ptr, in_size);
            Range r = synchronizeFrame();
            // Decode if we have a valid start and end synch word
            if(r.start>=0 && r.end>r.start){
                decode(r);
            } 
            yield();
            frame_counter++;
            return result;
        }

        /// Determines the maximum frame (buffer) size
        size_t maxFrameSize(){
            return max_buffer_size;
        }

        /// Synchronizes a Frame
        Range synchronizeFrame() {
            LOG(Debug, "synchronizeFrame");
            Range range = frameRange();
            if (range.start<0){
                // there is no Synch in the buffer at all -> we can ignore all data
                range.end = -1;
                LOG(Debug, "-> no synch")
                if (buffer.size==maxFrameSize()) {
                    buffer.size = 0;
                    LOG(Debug, "-> buffer cleared");
                }
            } else if (range.start>0) {
                // make sure that buffer starts with a synch word
                LOG(Debug, "-> moving to new start %d",range.start);
                buffer.size -= range.start;
                assert(buffer.size<=maxFrameSize());

                memmove(buffer.data, buffer.data + range.start, buffer.size);
                range.end -= range.start;
                range.start = 0;
                LOG(Debug, "-> we are at beginning of synch word");
            } else if (range.start==0) {
                LOG(Debug, "-> we are at beginning of synch word");
                if (range.end<0 && buffer.size == maxFrameSize()){
                    buffer.size = 0;
                    LOG(Debug, "-> buffer cleared");
                }
            }
            return range;
        }

        /// Determines the next start and end synch word in the buffer
        Range frameRange(){
            Range result;
            result.start = findSyncWord(0);
            result.end = findSyncWord(result.start+2);
            LOG(Debug, "-> frameRange -> %d - %d", result.start, result.end);
            return result;
        }

        /// Advances the frame buffer
        void advanceFrameBuffer(int offset){
            buffer.size -= offset;
            assert(buffer.size<=maxFrameSize());
            memmove(buffer.data, buffer.data+offset, buffer.size);
        }

        /// output decoded data
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
            
            // Output the reuslut in batches of max_result_buffer_size samples
            int total_open = nsamples*nchannels;
            int i = 0;
            do {
                i = 0;
                for (int j=0;j<nsamples;j++){
                    for (int ch = 0;ch<nchannels;ch++){
                        // scale sample
                        p_result_buffer[i++] = scale(pcm->samples[ch][j]);
                        total_open--;
                        if (i>=max_result_buffer_size){
                            // output full buffer
                            outputBuffer(act_info, p_result_buffer,i);
                            i = 0;
                        }
                    }
                }
            } while(total_open>0);

            // output last buffer part
            if (i>0){
                outputBuffer(act_info, p_result_buffer,i);
            }
        }
        /// Writes an individual buffer with max max_result_buffer_size samples
        void outputBuffer(MadAudioInfo &info, int16_t *result, int len ){
            // return result via callback
            if (pwmCallback!=nullptr){
                pwmCallback(info, result, len);
            }

#ifdef ARDUINO
            // return result via stream
            if (mad_output_stream!=nullptr){
                mad_output_stream->write((uint8_t*)result, len*sizeof(int16_t));
            }
#endif
        }

        /// Scales the sample from internal MAD format to int16
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
