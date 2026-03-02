#include "pixel_swizzler.hpp"
#include <stdlib.h>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <immintrin.h>
#define XSTUDIO_HAS_PEXT 1
#define XSTUDIO_HAS_SSSE3 1
#elif defined(__BMI2__)
#include <immintrin.h>
#define XSTUDIO_HAS_PEXT 1
#define XSTUDIO_HAS_SSSE3 1
#elif defined(__SSSE3__)
#include <tmmintrin.h>
#define XSTUDIO_HAS_PEXT 0
#define XSTUDIO_HAS_SSSE3 1
#else
#define XSTUDIO_HAS_PEXT 0
#define XSTUDIO_HAS_SSSE3 0
#endif

#include "xstudio/utility/chrono.hpp"

using namespace xstudio::bm_decklink_plugin_1_0;
using namespace xstudio;

namespace {

inline uint32_t bswap32(const uint32_t value) {
#if defined(_MSC_VER)
    return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(value);
#else
    return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
#endif
}

static inline bool cpu_has_bmi2() {
    if (!XSTUDIO_HAS_PEXT) {
        return false;
    }
#if defined(__GNUC__) || defined(__clang__)
    // Works on x86/x64 with GCC/Clang
    return __builtin_cpu_supports("bmi2");
#elif defined(_MSC_VER)
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 7, 0); // leaf 7, subleaf 0
    // EBX bit 8 = BMI2
    return (regs[1] & (1 << 8)) != 0;
#else
    return false;
#endif
}

static const bool __has_bmi2 = cpu_has_bmi2();

static inline bool cpu_has_ssse3() {
    if (!XSTUDIO_HAS_SSSE3) {
        return false;
    }
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_cpu_supports("ssse3");
#elif defined(_MSC_VER)
    int regs[4] = {0, 0, 0, 0};
    __cpuid(regs, 1); // leaf 1
    // ECX bit 9 = SSSE3
    return (regs[2] & (1 << 9)) != 0;
#else
    return false;
#endif
}

static const bool __has_ssse3 = cpu_has_ssse3();

inline uint64_t pack_12_bitRGB(uint64_t value) {
    return ((value & 0x000000000000fff0ull) >> 4) + ((value & 0x00000000fff00000ull) >> 8) + ((value & 0x0000fff000000000ull) >> 12);
}

} // namespace

// TODO: SIMD/BMI2 optimisation has been added and testing shows big improvements.
// Now we also want to do similar for ARM NEON instructions for MacOS if that's appropriate.

void RGBA16_to_10bitRGB::doit() {

#if XSTUDIO_HAS_SSSE3
    // Claude Opus 4.6 generated code. I haven't picked through this to
    // understand exactly how it works but it certainly gives the correct
    // result and is very fast.

    // This is about 3x faster than the simple loop that follows on
    // a XEON 6226
    if (__has_ssse3) {

        // SSSE3 path: process 4 pixels per iteration
        // Pack R10, G10, B10 into 32-bit: (R << 20) | (G << 10) | B, then bswap

        // Shuffle: gather [B,G] pairs from 2-pixel registers
        const __m128i bg_shuf_lo = _mm_set_epi8(
            -1,-1,-1,-1,-1,-1,-1,-1,  11,10,13,12,  3, 2, 5, 4);
        const __m128i bg_shuf_hi = _mm_set_epi8(
             11,10,13,12,  3, 2, 5, 4, -1,-1,-1,-1,-1,-1,-1,-1);

        // Shuffle: gather R values zero-extended to 32-bit
        const __m128i r_shuf_lo = _mm_set_epi8(
            -1,-1,-1,-1,-1,-1,-1,-1, -1,-1, 9, 8, -1,-1, 1, 0);
        const __m128i r_shuf_hi = _mm_set_epi8(
            -1,-1, 9, 8, -1,-1, 1, 0, -1,-1,-1,-1,-1,-1,-1,-1);

        // madd coefficients: B*1 + G*1024  ≡  B + (G << 10)
        const __m128i bg_coeff = _mm_set_epi16(1024,1, 1024,1, 1024,1, 1024,1);

        // Byte-swap mask for 32-bit elements
        const __m128i bswap_shuf = _mm_set_epi8(
            12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);

        while (n >= 4) {
            __m128i px01 = _mm_loadu_si128((const __m128i *)_src);
            __m128i px23 = _mm_loadu_si128((const __m128i *)(_src + 8));

            // Truncate to 10-bit
            __m128i v01 = _mm_srli_epi16(px01, 6);
            __m128i v23 = _mm_srli_epi16(px23, 6);

            // Gather B,G pairs and compute B + (G << 10) via madd
            __m128i bg = _mm_or_si128(
                _mm_shuffle_epi8(v01, bg_shuf_lo),
                _mm_shuffle_epi8(v23, bg_shuf_hi));
            __m128i bg32 = _mm_madd_epi16(bg, bg_coeff);

            // Gather R, shift to position, combine
            __m128i r32 = _mm_or_si128(
                _mm_shuffle_epi8(v01, r_shuf_lo),
                _mm_shuffle_epi8(v23, r_shuf_hi));
            r32 = _mm_slli_epi32(r32, 20);

            __m128i result = _mm_or_si128(bg32, r32);
            result = _mm_shuffle_epi8(result, bswap_shuf);

            _mm_storeu_si128((__m128i *)_dst, result);
            _src += 16;
            _dst += 4;
            n -= 4;
        }
    }
#endif

    // Scalar tail (original loop used to generate the above)
    while (n--) {
        *(_dst++) = bswap32(((_src[0]&(0x0000ffc0)) << 14) + ((_src[1]&(0x0000ffc0)) << 4) + ((_src[2]&(0x0000ffc0)) >> 6));
        _src+=4;
    }

}

void RGBA16_to_10bitRGBX::doit() {

#if XSTUDIO_HAS_SSSE3
    // Claude Opus 4.6 generated code.

    // This is about 3x faster than the simple loop that follows on
    // a XEON 6226

    if (__has_ssse3) {
        // SSSE3 path: process 4 pixels per iteration
        // Video range: 64 + ((src >> 6) * 876) >> 10  ≡  64 + mulhi_epu16(src >> 6, 876<<6)
        const __m128i scale    = _mm_set1_epi16(static_cast<short>(876 << 6)); // 56064
        const __m128i offset   = _mm_set1_epi16(64);

        // Shuffle: extract [B0,G0,B1,G1] from [R0,G0,B0,A0,R1,G1,B1,A1]
        const __m128i bg_shuf_lo = _mm_set_epi8(
            -1,-1,-1,-1,-1,-1,-1,-1,  11,10,13,12,  3, 2, 5, 4);
        const __m128i bg_shuf_hi = _mm_set_epi8(
             11,10,13,12,  3, 2, 5, 4, -1,-1,-1,-1,-1,-1,-1,-1);

        // Shuffle: extract [R0,0,R1,0] zero-extended to 32-bit
        const __m128i r_shuf_lo = _mm_set_epi8(
            -1,-1,-1,-1,-1,-1,-1,-1, -1,-1, 9, 8, -1,-1, 1, 0);
        const __m128i r_shuf_hi = _mm_set_epi8(
            -1,-1, 9, 8, -1,-1, 1, 0, -1,-1,-1,-1,-1,-1,-1,-1);

        // madd coefficients: B*4 + G*4096  ≡  (B<<2) + (G<<12)
        const __m128i bg_coeff = _mm_set_epi16(4096,4, 4096,4, 4096,4, 4096,4);

        // Byte-swap mask for 32-bit elements (LE→BE)
        const __m128i bswap_shuf = _mm_set_epi8(
            12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);

        while (n >= 4) {
            // Load 4 RGBA16 pixels (2 × 128-bit = 32 bytes)
            __m128i px01 = _mm_loadu_si128((const __m128i *)_src);
            __m128i px23 = _mm_loadu_si128((const __m128i *)(_src + 8));

            // Truncate to 10-bit then map to video range
            __m128i v01 = _mm_srli_epi16(px01, 6);
            __m128i v23 = _mm_srli_epi16(px23, 6);
            __m128i m01 = _mm_add_epi16(_mm_mulhi_epu16(v01, scale), offset);
            __m128i m23 = _mm_add_epi16(_mm_mulhi_epu16(v23, scale), offset);

            // Gather B,G pairs: [B0,G0,B1,G1,B2,G2,B3,G3]
            __m128i bg = _mm_or_si128(
                _mm_shuffle_epi8(m01, bg_shuf_lo),
                _mm_shuffle_epi8(m23, bg_shuf_hi));

            // (B<<2) + (G<<12) via signed multiply-add into 32-bit
            __m128i bg32 = _mm_madd_epi16(bg, bg_coeff);

            // Gather R values zero-extended to 32-bit, shift into position
            __m128i r32 = _mm_or_si128(
                _mm_shuffle_epi8(m01, r_shuf_lo),
                _mm_shuffle_epi8(m23, r_shuf_hi));
            r32 = _mm_slli_epi32(r32, 22);

            // Combine and byte-swap to big-endian
            __m128i result = _mm_or_si128(bg32, r32);
            result = _mm_shuffle_epi8(result, bswap_shuf);

            _mm_storeu_si128((__m128i *)_dst, result);
            _src += 16;
            _dst += 4;
            n -= 4;
        }
    }
#endif

    // Scalar tail
    // This is the original loop fed to the LLM
    while (n--) {
        uint32_t red = *(_src++) >> 6;
        uint32_t green = *(_src++) >> 6;
        uint32_t blue = *(_src++) >> 6;
        red = 64 + ((red*876) >> 10);
        green = 64 + ((green*876) >> 10);
        blue = 64 + ((blue*876) >> 10);
        _src++;
        *(_dst++) = bswap32((blue << 2) + (green << 12) + (red << 22));
    }

}


void RGBA16_to_10bitRGBXLE::doit() {

#if XSTUDIO_HAS_SSSE3
    // Claude Opus 4.6 generated code.

    // This is about 2x faster than the simple loop that follows on
    // a XEON 6226

    if (__has_ssse3) {
        // SSSE3 path: process 4 pixels per iteration (same as RGBX but no byte-swap)
        const __m128i scale    = _mm_set1_epi16(static_cast<short>(876 << 6)); // 56064
        const __m128i offset   = _mm_set1_epi16(64);

        const __m128i bg_shuf_lo = _mm_set_epi8(
            -1,-1,-1,-1,-1,-1,-1,-1,  11,10,13,12,  3, 2, 5, 4);
        const __m128i bg_shuf_hi = _mm_set_epi8(
             11,10,13,12,  3, 2, 5, 4, -1,-1,-1,-1,-1,-1,-1,-1);

        const __m128i r_shuf_lo = _mm_set_epi8(
            -1,-1,-1,-1,-1,-1,-1,-1, -1,-1, 9, 8, -1,-1, 1, 0);
        const __m128i r_shuf_hi = _mm_set_epi8(
            -1,-1, 9, 8, -1,-1, 1, 0, -1,-1,-1,-1,-1,-1,-1,-1);

        const __m128i bg_coeff = _mm_set_epi16(4096,4, 4096,4, 4096,4, 4096,4);

        while (n >= 4) {
            __m128i px01 = _mm_loadu_si128((const __m128i *)_src);
            __m128i px23 = _mm_loadu_si128((const __m128i *)(_src + 8));

            __m128i v01 = _mm_srli_epi16(px01, 6);
            __m128i v23 = _mm_srli_epi16(px23, 6);
            __m128i m01 = _mm_add_epi16(_mm_mulhi_epu16(v01, scale), offset);
            __m128i m23 = _mm_add_epi16(_mm_mulhi_epu16(v23, scale), offset);

            __m128i bg = _mm_or_si128(
                _mm_shuffle_epi8(m01, bg_shuf_lo),
                _mm_shuffle_epi8(m23, bg_shuf_hi));

            __m128i bg32 = _mm_madd_epi16(bg, bg_coeff);

            __m128i r32 = _mm_or_si128(
                _mm_shuffle_epi8(m01, r_shuf_lo),
                _mm_shuffle_epi8(m23, r_shuf_hi));
            r32 = _mm_slli_epi32(r32, 22);

            __m128i result = _mm_or_si128(bg32, r32);

            _mm_storeu_si128((__m128i *)_dst, result);
            _src += 16;
            _dst += 4;
            n -= 4;
        }
    }
#endif

    // Scalar tail
    while (n--) {
        uint32_t red = *(_src++) >> 6;
        uint32_t green = *(_src++) >> 6;
        uint32_t blue = *(_src++) >> 6;
        red = 64 + ((red*876) >> 10);
        green = 64 + ((green*876) >> 10);
        blue = 64 + ((blue*876) >> 10);
        _src++;
        *(_dst++) = (blue << 2) + (green << 12) + (red << 22);
    }

}

#define PACK_12BIT_RBG_MASK 0x0000fff0fff0fff0ull

void RGBA16_to_12bitRGBLE::doit() {

    // BMD wants 12 bit tightly packed RGB values, so RGB bits in the output will span
    // byte boundaries. This problem doesn't seem to lend itself to SIMD optimisation
    // at all but if the CPU supports BMI2 we can use the pext instruction to do the 
    // bit gathering efficiently. If not we fall back to a manual bit extraction loop 
    // which is slower but still real-time on modern CPUs.
    // BTW I did this, not an LLM!
    uint64_t *src = reinterpret_cast<uint64_t *>(_src);
    uint64_t *dst = reinterpret_cast<uint64_t *>(_dst);
#if XSTUDIO_HAS_PEXT        
    if (__has_bmi2) {
        while (n>=16) {
            dst[0] = _pext_u64(src[0], PACK_12BIT_RBG_MASK) + (_pext_u64(src[1], PACK_12BIT_RBG_MASK) << 36);
            dst[1] = (_pext_u64(src[1], PACK_12BIT_RBG_MASK) >> 28) + (_pext_u64(src[2], PACK_12BIT_RBG_MASK) << 8) + (_pext_u64(src[3], PACK_12BIT_RBG_MASK) << 44);
            dst[2] = (_pext_u64(src[3], PACK_12BIT_RBG_MASK) >> 20) + (_pext_u64(src[4], PACK_12BIT_RBG_MASK) << 16) + (_pext_u64(src[5], PACK_12BIT_RBG_MASK) << 52);
            dst[3] = (_pext_u64(src[5], PACK_12BIT_RBG_MASK) >> 12) + (_pext_u64(src[6], PACK_12BIT_RBG_MASK) << 24) + (_pext_u64(src[7], PACK_12BIT_RBG_MASK) << 60);
            dst[4] = (_pext_u64(src[7], PACK_12BIT_RBG_MASK) >> 4) + (_pext_u64(src[8], PACK_12BIT_RBG_MASK) << 32);
            dst[5] = (_pext_u64(src[8], PACK_12BIT_RBG_MASK) >> 32) + (_pext_u64(src[9], PACK_12BIT_RBG_MASK) << 4) + (_pext_u64(src[10], PACK_12BIT_RBG_MASK) << 40);
            dst[6] = (_pext_u64(src[10], PACK_12BIT_RBG_MASK) >> 24) + (_pext_u64(src[11], PACK_12BIT_RBG_MASK) << 12) + (_pext_u64(src[12], PACK_12BIT_RBG_MASK) << 48);
            dst[7] = (_pext_u64(src[12], PACK_12BIT_RBG_MASK) >> 16) + (_pext_u64(src[13], PACK_12BIT_RBG_MASK) << 20) + (_pext_u64(src[14], PACK_12BIT_RBG_MASK) << 56);
            dst[8] = (_pext_u64(src[14], PACK_12BIT_RBG_MASK) >> 8) + (_pext_u64(src[15], PACK_12BIT_RBG_MASK) << 28);
            src+=16;
            dst+=9;
            n-=16;
        }
        return;
    }
#endif  

    while (n>=16) {
        dst[0] = pack_12_bitRGB(src[0]) + (pack_12_bitRGB(src[1]) << 36);
        dst[1] = (pack_12_bitRGB(src[1]) >> 28) + (pack_12_bitRGB(src[2]) << 8) + (pack_12_bitRGB(src[3]) << 44);
        dst[2] = (pack_12_bitRGB(src[3]) >> 20) + (pack_12_bitRGB(src[4]) << 16) + (pack_12_bitRGB(src[5]) << 52);
        dst[3] = (pack_12_bitRGB(src[5]) >> 12) + (pack_12_bitRGB(src[6]) << 24) + (pack_12_bitRGB(src[7]) << 60);
        dst[4] = (pack_12_bitRGB(src[7]) >> 4) + (pack_12_bitRGB(src[8]) << 32);
        dst[5] = (pack_12_bitRGB(src[8]) >> 32) + (pack_12_bitRGB(src[9]) << 4) + (pack_12_bitRGB(src[10]) << 40);
        dst[6] = (pack_12_bitRGB(src[10]) >> 24) + (pack_12_bitRGB(src[11]) << 12) + (pack_12_bitRGB(src[12]) << 48);
        dst[7] = (pack_12_bitRGB(src[12]) >> 16) + (pack_12_bitRGB(src[13]) << 20) + (pack_12_bitRGB(src[14]) << 56);
        dst[8] = (pack_12_bitRGB(src[14]) >> 8) + (pack_12_bitRGB(src[15]) << 28);
        src+=16;
        dst+=9;
        n-=16;
    }

    // Here's my first implementation, doing the conversion on individual channels. This
    // is pretty slow on old-ish CPUs but runs very fast (~1ms per frame) on latest gen
    // intel XEONs for example
    /*
    int shift = 0;
    while (n>=4) {             

        // 4 channels worth of pixel data. truncated to 12 bits each
        uint16_t q = *(_src++) >> 4;
        uint16_t r = *(_src++) >> 4;
        uint16_t s = *(_src++) >> 4;
        _src++; // skip alpha
        uint16_t t = *(_src++) >> 4;

        *(_dst++) = q + ((r&15) << 12);
        *(_dst++) = (r >> 4) + ((s&255) << 8);
        *(_dst++) = (s >> 8) + (t << 4);

        q = *(_src++) >> 4;
        r = *(_src++) >> 4;
        _src++; // skip alpha
        s = *(_src++) >> 4;
        t = *(_src++) >> 4;

        *(_dst++) = q + ((r&15) << 12);
        *(_dst++) = (r >> 4) + ((s&255) << 8);
        *(_dst++) = (s >> 8) + (t << 4);

        q = *(_src++) >> 4;
        _src++; // skip alpha
        r = *(_src++) >> 4;
        s = *(_src++) >> 4;
        t = *(_src++) >> 4;

        *(_dst++) = q + ((r&15) << 12);
        *(_dst++) = (r >> 4) + ((s&255) << 8);
        *(_dst++) = (s >> 8) + (t << 4);

        _src++; // skip alpha
        n-=4;

    }*/

}

void RGBA16_to_12bitRGB::doit() {

    uint32_t * _mdst = (uint32_t *)_dst;
    size_t mn = (n*9)/8;

    uint64_t *src = reinterpret_cast<uint64_t *>(_src);
    uint64_t *dst = reinterpret_cast<uint64_t *>(_dst);
    if (__has_bmi2) {
#if XSTUDIO_HAS_PEXT        
        while (n>=16) {
            dst[0] = _pext_u64(src[0], PACK_12BIT_RBG_MASK) + (_pext_u64(src[1], PACK_12BIT_RBG_MASK) << 36);
            dst[1] = (_pext_u64(src[1], PACK_12BIT_RBG_MASK) >> 28) + (_pext_u64(src[2], PACK_12BIT_RBG_MASK) << 8) + (_pext_u64(src[3], PACK_12BIT_RBG_MASK) << 44);
            dst[2] = (_pext_u64(src[3], PACK_12BIT_RBG_MASK) >> 20) + (_pext_u64(src[4], PACK_12BIT_RBG_MASK) << 16) + (_pext_u64(src[5], PACK_12BIT_RBG_MASK) << 52);
            dst[3] = (_pext_u64(src[5], PACK_12BIT_RBG_MASK) >> 12) + (_pext_u64(src[6], PACK_12BIT_RBG_MASK) << 24) + (_pext_u64(src[7], PACK_12BIT_RBG_MASK) << 60);
            dst[4] = (_pext_u64(src[7], PACK_12BIT_RBG_MASK) >> 4) + (_pext_u64(src[8], PACK_12BIT_RBG_MASK) << 32);
            dst[5] = (_pext_u64(src[8], PACK_12BIT_RBG_MASK) >> 32) + (_pext_u64(src[9], PACK_12BIT_RBG_MASK) << 4) + (_pext_u64(src[10], PACK_12BIT_RBG_MASK) << 40);
            dst[6] = (_pext_u64(src[10], PACK_12BIT_RBG_MASK) >> 24) + (_pext_u64(src[11], PACK_12BIT_RBG_MASK) << 12) + (_pext_u64(src[12], PACK_12BIT_RBG_MASK) << 48);
            dst[7] = (_pext_u64(src[12], PACK_12BIT_RBG_MASK) >> 16) + (_pext_u64(src[13], PACK_12BIT_RBG_MASK) << 20) + (_pext_u64(src[14], PACK_12BIT_RBG_MASK) << 56);
            dst[8] = (_pext_u64(src[14], PACK_12BIT_RBG_MASK) >> 8) + (_pext_u64(src[15], PACK_12BIT_RBG_MASK) << 28);
            src+=16;
            dst+=9;
            n-=16;
        }
#endif  
    } else {
        while (n>=16) {
            dst[0] = pack_12_bitRGB(src[0]) + (pack_12_bitRGB(src[1]) << 36);
            dst[1] = (pack_12_bitRGB(src[1]) >> 28) + (pack_12_bitRGB(src[2]) << 8) + (pack_12_bitRGB(src[3]) << 44);
            dst[2] = (pack_12_bitRGB(src[3]) >> 20) + (pack_12_bitRGB(src[4]) << 16) + (pack_12_bitRGB(src[5]) << 52);
            dst[3] = (pack_12_bitRGB(src[5]) >> 12) + (pack_12_bitRGB(src[6]) << 24) + (pack_12_bitRGB(src[7]) << 60);
            dst[4] = (pack_12_bitRGB(src[7]) >> 4) + (pack_12_bitRGB(src[8]) << 32);
            dst[5] = (pack_12_bitRGB(src[8]) >> 32) + (pack_12_bitRGB(src[9]) << 4) + (pack_12_bitRGB(src[10]) << 40);
            dst[6] = (pack_12_bitRGB(src[10]) >> 24) + (pack_12_bitRGB(src[11]) << 12) + (pack_12_bitRGB(src[12]) << 48);
            dst[7] = (pack_12_bitRGB(src[12]) >> 16) + (pack_12_bitRGB(src[13]) << 20) + (pack_12_bitRGB(src[14]) << 56);
            dst[8] = (pack_12_bitRGB(src[14]) >> 8) + (pack_12_bitRGB(src[15]) << 28);
            src+=16;
            dst+=9;
            n-=16;
        }
    }

    // now run through the output and swap the bytes to get to big endian format.
    while (mn--) {
        *_mdst = bswap32(*_mdst);
        _mdst++;
    }

}

