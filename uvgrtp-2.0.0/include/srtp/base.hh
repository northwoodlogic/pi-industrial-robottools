#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "../debug.hh"
#include "../frame.hh"
#include "../rtp.hh"
#include "../util.hh"
#include "../zrtp.hh"

#define AES_KEY_LENGTH      16 /* 128 bits */
#define HMAC_KEY_LENGTH     32 /* 256 bits */
#define SALT_LENGTH         14 /* 112 bits */
#define AUTH_TAG_LENGTH     10
#define SRTCP_INDEX_LENGTH   4

namespace uvgrtp {

    /* Vector of buffers that contain a full RTP frame */
    typedef std::vector<std::pair<size_t, uint8_t *>> buf_vec;

    enum STYPE {
        SRTP  = 0,
        SRTCP = 1
    };

    enum ETYPE {
        AES_128 = 0,
    };

    enum HTYPE {
        HMAC_SHA1 = 0,
    };

    enum LABELS {
        SRTP_ENCRYPTION      = 0x0,
        SRTP_AUTHENTICATION  = 0x1,
        SRTP_SALTING         = 0x2,
        SRTCP_ENCRYPTION     = 0x3,
        SRTCP_AUTHENTICATION = 0x4,
        SRTCP_SALTING        = 0x5
    };

    /* Key context for SRTP keys,
     * ZRTP generates two keys: one for initiator and one for responder
     *
     * Master key is not directly used to encrypt packets but it is used
     * to create session keys for the SRTP/SRTCP */
    typedef struct srtp_key_ctx {
        /* Keys negotiated by ZRTP */
        struct {
            /* Our master key and salt */
            uint8_t local_key[AES_KEY_LENGTH];
            uint8_t local_salt[SALT_LENGTH];

            /* Remote's master key and salt */
            uint8_t remote_key[AES_KEY_LENGTH];
            uint8_t remote_salt[SALT_LENGTH];
        } master;

        /* Used to encrypt/authenticate packets sent by us */
        struct {
            uint8_t enc_key[AES_KEY_LENGTH];
            uint8_t auth_key[AES_KEY_LENGTH];
            uint8_t salt_key[AES_KEY_LENGTH]; /* TODO: make sure this is correct */
        } local;

        /* Used to decrypt/Authenticate packets sent by remote */
        struct {
            uint8_t enc_key[AES_KEY_LENGTH];
            uint8_t auth_key[AES_KEY_LENGTH];
            uint8_t salt_key[AES_KEY_LENGTH];
        } remote;

    } srtp_key_ctx_t;

    typedef struct srtp_ctx {
        int type;     /* srtp or srtcp */
        uint32_t roc; /* roll-over counter */
        uint32_t rts; /* timestamp of the frame that causes ROC update */

        int enc;   /* identifier for encryption algorithm */
        int hmac;  /* identifier for message authentication algorithm */

        bool mki_present; /* is MKI present in SRTP packets */
        size_t mki_size;  /* length of the MKI field in bytes if it's present */
        uint8_t *mki;     /* master key identifier */

        uint8_t *master_key;  /* master key */
        uint8_t *master_salt; /* master salt */
        size_t  mk_cnt;       /* how many packets have been encrypted with master key */

        size_t n_e; /* size of encryption key */
        size_t n_a; /* size of hmac key */

        /* following fields are receiver-only */
        uint16_t s_l;    /* highest received sequence number */
        uint8_t *replay; /* list of recently received and authenticated SRTP packets */

        int flags; /* context configuration flags */

        srtp_key_ctx_t key_ctx;
    } srtp_ctx_t;

    class base_srtp {
        public:
            base_srtp();
            virtual ~base_srtp();

            /* Setup Secure RTP/RTCP connection using ZRTP
             *
             * Return RTP_OK if SRTP setup was successful
             * Return RTP_INVALID_VALUE if "zrtp" is nullptr
             * Return RTP_MEMORY allocation failed */
            rtp_error_t init_zrtp(int type, int flags, uvgrtp::rtp *rtp, uvgrtp::zrtp *zrtp);

            /* Setup Secure RTP/RTCP connection using user-managed keys
             *
             * Length of "key" must be AES_KEY_LENGTH (16 bytes)
             * Length of "salt" must be SALT_LENGTH   (14 bytes)
             *
             * Return RTP_OK if SRTP setup was successful
             * Return RTP_INVALID_VALUE if "key" or "salt" is nullptr
             * Return RTP_MEMORY allocation failed */
            rtp_error_t init_user(int type, int flags, uint8_t *key, uint8_t *salt);

            /* Has RTP packet encryption been disabled? */
            bool use_null_cipher();

            /* Has RTP packet authentication been enabled? */
            bool authenticate_rtp();

            /* Get reference to the SRTP context (including session keys) */
            srtp_ctx_t *get_ctx();

            /* Returns true if the packet having this HMAC digest is replayed
             * Returns false if replay protection has not been enabled */
            bool is_replayed_packet(uint8_t *digest);

        protected:
            rtp_error_t derive_key(int label, uint8_t *key, uint8_t *salt, uint8_t *out, size_t len);

            /* Create IV for the packet that is about to be encrypted
             *
             * Return RTP_OK on success and place the iv to "out"
             * Return RTP_INVALID_VALUE if one of the parameters is invalid */
            rtp_error_t create_iv(uint8_t *out, uint32_t ssrc, uint64_t index, uint8_t *salt);

            /* Internal init method that initialize the SRTP context using values in key_ctx_.master */
            rtp_error_t init(int type, int flags);
            /* SRTP context containing all session information and keys */
            srtp_ctx_t *srtp_ctx_;

            /* Map containing all authentication tags of received packets (separate for SRTP and SRTCP)
             * Used to implement replay protection */
            std::unordered_set<uint64_t> replay_list_;

            /* If NULL cipher is enabled, it means that RTP packets are not
             * encrypted but other security mechanisms described in RFC 3711 may be used */
            bool use_null_cipher_;

            /* By default RTP packet authentication is disabled but by
             * giving RCE_SRTP_AUTHENTICATE_RTP to create_stream() user can enable it.
             *
             * The authentication tag will occupy the last 8 bytes of the RTP packet */
            bool authenticate_rtp_;
    };
};

namespace uvg_rtp = uvgrtp;
