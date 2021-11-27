#ifndef __RTP_HH_
#define __RTP_HH_

#include "clock.hh"
#include "frame.hh"
#include "util.hh"

namespace uvgrtp {

    class rtp {
        public:
            rtp(rtp_format_t fmt);
            ~rtp();

            uint32_t     get_ssrc();
            uint16_t     get_sequence();
            uint32_t     get_clock_rate();
            size_t       get_payload_size();
            size_t       get_pkt_max_delay();
            rtp_format_t get_payload();

            void inc_sent_pkts();
            void inc_sequence();

            void set_clock_rate(size_t rate);
            void set_payload(rtp_format_t fmt);
            void set_dynamic_payload(uint8_t payload);
            void set_timestamp(uint64_t timestamp);
            void set_payload_size(size_t payload_size);
            void set_pkt_max_delay(size_t delay);

            void fill_header(uint8_t *buffer);
            void update_sequence(uint8_t *buffer);

            /* Validates the RTP header pointed to by "packet" */
            static rtp_error_t packet_handler(ssize_t size, void *packet, int flags, frame::rtp_frame **out);

        private:

            uint32_t ssrc_;
            uint32_t ts_;
            uint16_t seq_;
            uint8_t fmt_;
            uint8_t payload_;

            uint32_t clock_rate_;
            uint64_t wc_start_;
            uvgrtp::clock::hrc::hrc_t wc_start_2;

            size_t sent_pkts_;

            /* Use custom timestamp for the outgoing RTP packets */
            uint64_t timestamp_;

            /* What is the maximum size of the payload available for this RTP instance
             *
             * By default, the value is set to 1443
             * (maximum amount of payload bytes when MTU is 1500) */
            size_t payload_size_;

            /* What is the maximum delay allowed for each frame
             * i.e. how long does the packet receiver wait for
             * all fragments of a frame until it's considered late and dropped
             *
             * Default value is 100ms */
            size_t delay_;
    };
};

namespace uvg_rtp = uvgrtp;

#endif /* __RTP_HH_ */
