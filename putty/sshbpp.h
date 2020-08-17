/*
 * Abstraction of the binary packet protocols used in SSH.
 */

#ifndef PUTTY_SSHBPP_H
#define PUTTY_SSHBPP_H

typedef struct BinaryPacketProtocol BinaryPacketProtocol;

struct BinaryPacketProtocolVtable {
    void (*free)(BinaryPacketProtocol *); 
    void (*handle_input)(BinaryPacketProtocol *);
    PktOut *(*new_pktout)(int type);
    void (*format_packet)(BinaryPacketProtocol *, PktOut *);
};

struct BinaryPacketProtocol {
    const struct BinaryPacketProtocolVtable *vt;
    bufchain *in_raw, *out_raw;
    PktInQueue *in_pq;
    PacketLogSettings *pls;
    LogContext *logctx;

    int seen_disconnect;
    char *error;
};

#define ssh_bpp_free(bpp) ((bpp)->vt->free(bpp))
#define ssh_bpp_handle_input(bpp) ((bpp)->vt->handle_input(bpp))
#define ssh_bpp_new_pktout(bpp, type) ((bpp)->vt->new_pktout(type))
#define ssh_bpp_format_packet(bpp, pkt) ((bpp)->vt->format_packet(bpp, pkt))

BinaryPacketProtocol *ssh1_bpp_new(void);
void ssh1_bpp_new_cipher(BinaryPacketProtocol *bpp,
                         const struct ssh1_cipheralg *cipher,
                         const void *session_key);
void ssh1_bpp_start_compression(BinaryPacketProtocol *bpp);

BinaryPacketProtocol *ssh2_bpp_new(void);
void ssh2_bpp_new_outgoing_crypto(
    BinaryPacketProtocol *bpp,
    const struct ssh2_cipheralg *cipher, const void *ckey, const void *iv,
    const struct ssh2_macalg *mac, int etm_mode, const void *mac_key,
    const struct ssh_compression_alg *compression);
void ssh2_bpp_new_incoming_crypto(
    BinaryPacketProtocol *bpp,
    const struct ssh2_cipheralg *cipher, const void *ckey, const void *iv,
    const struct ssh2_macalg *mac, int etm_mode, const void *mac_key,
    const struct ssh_compression_alg *compression);

BinaryPacketProtocol *ssh2_bare_bpp_new(void);

/*
 * The initial code to handle the SSH version exchange is also
 * structured as an implementation of BinaryPacketProtocol, because
 * that makes it easy to switch from that to the next BPP once it
 * tells us which one we're using.
 */
struct ssh_version_receiver {
    void (*got_ssh_version)(struct ssh_version_receiver *rcv,
                            int major_version);
};
BinaryPacketProtocol *ssh_verstring_new(
    Conf *conf, Frontend *frontend, int bare_connection_mode,
    const char *protoversion, struct ssh_version_receiver *rcv);
const char *ssh_verstring_get_remote(BinaryPacketProtocol *);
const char *ssh_verstring_get_local(BinaryPacketProtocol *);
int ssh_verstring_get_bugs(BinaryPacketProtocol *);

#endif /* PUTTY_SSHBPP_H */
