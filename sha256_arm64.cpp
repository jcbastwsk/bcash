// ARM64 hardware-accelerated SHA-256 Transform
// Uses ARMv8 Crypto Extensions (SHA2 instructions)
// Based on public domain noloader/SHA-Intrinsics implementation
// (same approach adopted by Bitcoin Core)
//
// All Apple Silicon (M1/M2/M3/M4) supports these instructions.

#if defined(__aarch64__) && defined(__ARM_FEATURE_SHA2)

#include <arm_neon.h>
#include <stdint.h>

static const uint32_t K256[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Process one 64-byte block using ARM SHA-256 hardware instructions.
// state: 8 x uint32_t working hash state (modified in place)
// data:  16 x uint32_t message words (already in big-endian/host order
//        as expected by CryptoPP::SHA256::Transform)
extern "C"
void SHA256_Transform_ARM(uint32_t *state, const uint32_t *data)
{
    // The ARM SHA-256 intrinsics vsha256hq_u32 / vsha256h2q_u32 expect
    // two state registers in the layout:
    //   STATE0 = { s[0], s[1], s[4], s[5] }  (A, B, E, F)
    //   STATE1 = { s[2], s[3], s[6], s[7] }  (C, D, G, H)

    // Load current hash state
    uint32x4_t ABCD = vld1q_u32(&state[0]);
    uint32x4_t EFGH = vld1q_u32(&state[4]);

    // Rearrange into the layout the SHA instructions expect
    // ABCD = [A B C D], EFGH = [E F G H]
    // We need STATE0 = [A B E F], STATE1 = [C D G H]
    uint32x4_t STATE0 = vcombine_u32(vget_low_u32(ABCD), vget_low_u32(EFGH));
    uint32x4_t STATE1 = vcombine_u32(vget_high_u32(ABCD), vget_high_u32(EFGH));

    // Save for final addition
    uint32x4_t ABEF_SAVE = STATE0;
    uint32x4_t CDGH_SAVE = STATE1;

    uint32x4_t MSG0, MSG1, MSG2, MSG3;
    uint32x4_t TMP0, TMP2;

    MSG0 = vld1q_u32(&data[0]);
    MSG1 = vld1q_u32(&data[4]);
    MSG2 = vld1q_u32(&data[8]);
    MSG3 = vld1q_u32(&data[12]);

    // Rounds 0-3
    TMP0 = vaddq_u32(MSG0, vld1q_u32(&K256[0]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG0 = vsha256su0q_u32(MSG0, MSG1);

    // Rounds 4-7
    TMP0 = vaddq_u32(MSG1, vld1q_u32(&K256[4]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);
    MSG1 = vsha256su0q_u32(MSG1, MSG2);

    // Rounds 8-11
    TMP0 = vaddq_u32(MSG2, vld1q_u32(&K256[8]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);
    MSG2 = vsha256su0q_u32(MSG2, MSG3);

    // Rounds 12-15
    TMP0 = vaddq_u32(MSG3, vld1q_u32(&K256[12]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);
    MSG3 = vsha256su0q_u32(MSG3, MSG0);

    // Rounds 16-19
    TMP0 = vaddq_u32(MSG0, vld1q_u32(&K256[16]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);
    MSG0 = vsha256su0q_u32(MSG0, MSG1);

    // Rounds 20-23
    TMP0 = vaddq_u32(MSG1, vld1q_u32(&K256[20]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);
    MSG1 = vsha256su0q_u32(MSG1, MSG2);

    // Rounds 24-27
    TMP0 = vaddq_u32(MSG2, vld1q_u32(&K256[24]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);
    MSG2 = vsha256su0q_u32(MSG2, MSG3);

    // Rounds 28-31
    TMP0 = vaddq_u32(MSG3, vld1q_u32(&K256[28]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);
    MSG3 = vsha256su0q_u32(MSG3, MSG0);

    // Rounds 32-35
    TMP0 = vaddq_u32(MSG0, vld1q_u32(&K256[32]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);
    MSG0 = vsha256su0q_u32(MSG0, MSG1);

    // Rounds 36-39
    TMP0 = vaddq_u32(MSG1, vld1q_u32(&K256[36]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);
    MSG1 = vsha256su0q_u32(MSG1, MSG2);

    // Rounds 40-43
    TMP0 = vaddq_u32(MSG2, vld1q_u32(&K256[40]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);
    MSG2 = vsha256su0q_u32(MSG2, MSG3);

    // Rounds 44-47
    TMP0 = vaddq_u32(MSG3, vld1q_u32(&K256[44]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);
    MSG3 = vsha256su0q_u32(MSG3, MSG0);

    // Rounds 48-51
    TMP0 = vaddq_u32(MSG0, vld1q_u32(&K256[48]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
    MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);

    // Rounds 52-55
    TMP0 = vaddq_u32(MSG1, vld1q_u32(&K256[52]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);

    // Rounds 56-59
    TMP0 = vaddq_u32(MSG2, vld1q_u32(&K256[56]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);

    // Rounds 60-63
    TMP0 = vaddq_u32(MSG3, vld1q_u32(&K256[60]));
    TMP2 = STATE0;
    STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
    STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);

    // Add back saved state
    STATE0 = vaddq_u32(STATE0, ABEF_SAVE);
    STATE1 = vaddq_u32(STATE1, CDGH_SAVE);

    // Rearrange back: STATE0=[A,B,E,F] STATE1=[C,D,G,H] -> [A,B,C,D] [E,F,G,H]
    ABCD = vcombine_u32(vget_low_u32(STATE0), vget_low_u32(STATE1));
    EFGH = vcombine_u32(vget_high_u32(STATE0), vget_high_u32(STATE1));

    vst1q_u32(&state[0], ABCD);
    vst1q_u32(&state[4], EFGH);
}

#endif // __aarch64__ && __ARM_FEATURE_SHA2
