/*
 * M4KK1 4P1 - utils.c
 * Description: Swap2 utility functions — CRC32C, UUID,
 *              compression stubs, endian helpers.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "swap2.h"
#include "../include/swap2.h"
#include "../../y4ku/include/console.h"

uint32_t
mkrn_swap2_checksum_crc32c(const void *pData,
                           uint32_t u32Size)
{
    if (!pData || u32Size == 0)
        return 0;

    const uint8_t *pBytes = (const uint8_t *)pData;
    uint32_t u32Crc = 0xFFFFFFFF;

    static const uint32_t u32Crc32cTable[256] = {
        0x00000000, 0xF26B8303, 0xE13B70F7,
        0x1350F3F4, 0xC79A971F, 0x35F1141C,
        0x26A1E7E8, 0xD4CA64EB, 0x8AD958CF,
        0x78B2DBCC, 0x6BE22838, 0x9989AB3B,
        0x4D43CFD0, 0xBF284CD3, 0xAC78BF27,
        0x5E133C24, 0x105EC76F, 0xE235446C,
        0xF165B798, 0x030E349B, 0xD7C45070,
        0x25AFD373, 0x36FF2087, 0xC494A384,
        0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57,
        0x89D76C54, 0x5D1D08BF, 0xAF768BBC,
        0xBC267848, 0x4E4DFB4B, 0x20BD8EDE,
        0xD2D60DDD, 0xC186FE29, 0x33ED7D2A,
        0xE72719C1, 0x154C9AC2, 0x061C6936,
        0xF477EA35, 0xAA64D611, 0x580F5512,
        0x4B5FA6E6, 0xB93425E5, 0x6DFE410E,
        0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
        0x30E349B1, 0xC288CAB2, 0xD1D83946,
        0x23B3BA45, 0xF779DEAE, 0x05125DAD,
        0x1642AE59, 0xE4292D5A, 0xBA3A117E,
        0x4851927D, 0x5B416189, 0xA92AE28A,
        0x7DE08661, 0x8F8B0562, 0x9CDBF696,
        0x6EB07595, 0x417B1DBC, 0xB3109EBF,
        0xA0406D4B, 0x522BEE48, 0x86E18AA3,
        0x748A09A0, 0x67DAFA54, 0x95B17957,
        0xCBA24573, 0x39C9C670, 0x2A993584,
        0xD8F2B687, 0x0C38D26C, 0xFE53516F,
        0xED03A29B, 0x1F682198, 0x5125DAD3,
        0xA34E59D0, 0xB01EAA24, 0x42752927,
        0x96BF4DCC, 0x64D4CECF, 0x77843D3B,
        0x85EFBE38, 0xDBFC821C, 0x2997011F,
        0x3AC7F2EB, 0xC8AC71E8, 0x1C661503,
        0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
        0x61C69362, 0x93AD1061, 0x80FDE395,
        0x72966096, 0xA65C047D, 0x5437877E,
        0x4767748A, 0xB50CF789, 0xEB1FCBAD,
        0x197448AE, 0x0A24BB5A, 0xF84F3859,
        0x2C855CB2, 0xDEEFDFB1, 0xCDBF2C45,
        0x3FD4AF46, 0x7198540D, 0x83F3D70E,
        0x90A324FA, 0x62C8A7F9, 0xB602C312,
        0x44694011, 0x5739B3E5, 0xA55230E6,
        0xF34E6F75, 0x0125EC76, 0x12751F82,
        0xE01E9C81, 0x34D4F86A, 0xC6BF7B69,
        0xD5EF889D, 0x27840B9E, 0x799737BA,
        0x8BFCB4B9, 0x98AC474D, 0x6AC7C44E,
        0xBE0DA0A5, 0x4C4623A6, 0x5F16D052,
        0xAD7D5351, 0x082F6BB7, 0xFA44E8B4,
        0xE9141B40, 0x1B7F9843, 0xCFB5FCA8,
        0x3DDE7FAB, 0x2E8E8C5F, 0xDCE50F5C,
        0x92A8F417, 0x60C37714, 0x739384E0,
        0x81F807E3, 0x55326308, 0xA759E00B,
        0xB40913FF, 0x466290FC, 0x1871ACD8,
        0xEA1A2FD9, 0xF94ADC2D, 0x0B215F2E,
        0xDFEB3BC5, 0x2D80A8C6, 0x3ED05B32,
        0xCCBBD831, 0xA24BAD84, 0x50002E87,
        0x4350DD73, 0xB13B5E70, 0x65F13A9B,
        0x979AB998, 0x84CA4A6C, 0x76A1C96F,
        0x28B2F54B, 0xDAD97648, 0xC98985BC,
        0x3BE206BF, 0xEF286254, 0x1D43E157,
        0x0E1312A3, 0xFC7891A0, 0xB2356AEB,
        0x405EE9E8, 0x530E1A1C, 0xA165991F,
        0x75AFFDF4, 0x87C47EF7, 0x94D48D03,
        0x66FF0E00, 0x38EC3224, 0xCA87B127,
        0xD9D742D3, 0x2BBCC1D0, 0xFF76A53B,
        0x0D1D2638, 0x1E4DD5CC, 0xEC2656CF,
        0xC38D26C4, 0x31E6A5C7, 0x22B65633,
        0xD0DDD530, 0x0417B1DB, 0xF67C32D8,
        0xE52CC12C, 0x1747422F, 0x49547E0B,
        0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF,
        0x8ECEE914, 0x7CA56A17, 0x6FF599E3,
        0x9D9E1AE0, 0xD3D3E1AB, 0x21B862A8,
        0x32E8915C, 0xC083125F, 0x144976B4,
        0xE622F5B7, 0xF5720643, 0x07198540,
        0x590AB964, 0xAB613A67, 0xB831C993,
        0x4A5A4A90, 0x9E902E7B, 0x6CFBAD78,
        0x7FAB5E8C, 0x8DC0DD8F, 0xE330A81A,
        0x115B2B19, 0x020BD8ED, 0xF0605BEE,
        0x24AA3F05, 0xD6C19C06, 0xC5916FF2,
        0x37FAECF1, 0x69E9D0D5, 0x9B8253D6,
        0x88D2A022, 0x7AB92321, 0xAE7347CA,
        0x5C18ECC9, 0x4F68173D, 0xBD03943E,
        0xF34E6F75, 0x0125EC76, 0x12751F82,
        0xE01E9C81, 0x34D4F86A, 0xC6BF7B69,
        0xD5EF889D, 0x27840B9E, 0x799737BA,
        0x8BFCB4B9, 0x98AC474D, 0x6AC7C44E,
        0xBE0DA0A5, 0x4C4623A6, 0x5F16D052,
        0xAD7D5351,
    };

    for (uint32_t i = 0; i < u32Size; i++)
        u32Crc =
            (u32Crc >> 8)
            ^ u32Crc32cTable[(u32Crc ^ pBytes[i])
                             & 0xFF];

    return u32Crc ^ 0xFFFFFFFF;
}

void
mkrn_swap2_uuid_generate(uint8_t *pUuid)
{
    if (!pUuid)
        return;

    uint64_t u64Seed = mkrn_swap2_time_current();
    u64Seed ^= (uint64_t)pUuid;

    for (int i = 0; i < 16; i++) {
        u64Seed = u64Seed * 1103515245 + 12345;
        pUuid[i] = (uint8_t)(u64Seed >> 16);

        if (i == 6)
            pUuid[i] = (pUuid[i] & 0x0F) | 0x40;
        else if (i == 8)
            pUuid[i] = (pUuid[i] & 0x3F) | 0x80;
    }
}

uint64_t
mkrn_swap2_time_current(void)
{
    return 1234567890ULL;
}

void *
mkrn_swap2_memcpy(void *pDest, const void *pSrc,
                  size_t n)
{
    uint8_t *pD = (uint8_t *)pDest;
    const uint8_t *pS = (const uint8_t *)pSrc;

    for (size_t i = 0; i < n; i++)
        pD[i] = pS[i];

    return pDest;
}

void *
mkrn_swap2_memset(void *pDest, int value, size_t n)
{
    uint8_t *pD = (uint8_t *)pDest;

    for (size_t i = 0; i < n; i++)
        pD[i] = (uint8_t)value;

    return pDest;
}

int
mkrn_swap2_memcmp(const void *pS1, const void *pS2,
                  size_t n)
{
    const uint8_t *pA = (const uint8_t *)pS1;
    const uint8_t *pB = (const uint8_t *)pS2;

    for (size_t i = 0; i < n; i++) {
        if (pA[i] != pB[i])
            return pA[i] - pB[i];
    }

    return 0;
}

uint64_t
mkrn_swap2_align_pages(uint64_t u64Size)
{
    if (u64Size == 0)
        return 0;

    uint64_t u64PageSize = SWAP2_DEFAULT_PAGE_SIZE;
    return (u64Size + u64PageSize - 1)
           & ~(u64PageSize - 1);
}

uint32_t
mkrn_swap2_get_page_count(uint64_t u64Size)
{
    if (u64Size == 0)
        return 0;
    return (uint32_t)(
        (u64Size + SWAP2_DEFAULT_PAGE_SIZE - 1)
        / SWAP2_DEFAULT_PAGE_SIZE);
}

uint64_t
mkrn_swap2_get_swap_size(uint32_t u32Pages)
{
    return (uint64_t)u32Pages
           * SWAP2_DEFAULT_PAGE_SIZE;
}

int
mkrn_swap2_compress_page(void *pInput,
                         uint32_t u32InputSize,
                         void *pOutput,
                         uint32_t *pOutputSize,
                         uint32_t u32Algorithm)
{
    if (!pInput || !pOutput || !pOutputSize)
        return -1;

    switch (u32Algorithm) {
    case SWAP2_COMPRESSION_NONE:
    case SWAP2_COMPRESSION_LZ4:
    case SWAP2_COMPRESSION_ZSTD:
    case SWAP2_COMPRESSION_LZMA:
        if (*pOutputSize < u32InputSize)
            return -1;
        mkrn_swap2_memcpy(pOutput, pInput,
                          u32InputSize);
        *pOutputSize = u32InputSize;
        break;
    default:
        return -1;
    }

    return 0;
}

int
mkrn_swap2_decompress_page(void *pInput,
                           uint32_t u32InputSize,
                           void *pOutput,
                           uint32_t *pOutputSize,
                           uint32_t u32Algorithm)
{
    if (!pInput || !pOutput || !pOutputSize)
        return -1;

    switch (u32Algorithm) {
    case SWAP2_COMPRESSION_NONE:
    case SWAP2_COMPRESSION_LZ4:
    case SWAP2_COMPRESSION_ZSTD:
    case SWAP2_COMPRESSION_LZMA:
        if (*pOutputSize < u32InputSize)
            return -1;
        mkrn_swap2_memcpy(pOutput, pInput,
                          u32InputSize);
        *pOutputSize = u32InputSize;
        break;
    default:
        return -1;
    }

    return 0;
}

uint32_t
mkrn_swap2_calculate_checksum(void *pData,
                              uint32_t u32Size,
                              uint32_t u32Algorithm)
{
    if (!pData || u32Size == 0)
        return 0;

    switch (u32Algorithm) {
    case SWAP2_CHECKSUM_NONE:
        return 0;
    case SWAP2_CHECKSUM_CRC32C:
    case SWAP2_CHECKSUM_SHA256:
    case SWAP2_CHECKSUM_BLAKE3:
        return mkrn_swap2_checksum_crc32c(pData,
                                          u32Size);
    default:
        return 0;
    }
}

uint16_t
mkrn_swap2_htole16(uint16_t u16Value)
{
    return u16Value;
}

uint32_t
mkrn_swap2_htole32(uint32_t u32Value)
{
    return u32Value;
}

uint64_t
mkrn_swap2_htole64(uint64_t u64Value)
{
    return u64Value;
}

uint16_t
mkrn_swap2_le16toh(uint16_t u16Value)
{
    return mkrn_swap2_htole16(u16Value);
}

uint32_t
mkrn_swap2_le32toh(uint32_t u32Value)
{
    return mkrn_swap2_htole32(u32Value);
}

uint64_t
mkrn_swap2_le64toh(uint64_t u64Value)
{
    return mkrn_swap2_htole64(u64Value);
}

uint32_t
mkrn_swap2_ffs(uint32_t u32Value)
{
    if (u32Value == 0)
        return 0;

    uint32_t u32Bit = 1;
    for (int i = 0; i < 32; i++) {
        if (u32Value & u32Bit)
            return i + 1;
        u32Bit <<= 1;
    }

    return 0;
}

uint32_t
mkrn_swap2_fls(uint32_t u32Value)
{
    if (u32Value == 0)
        return 0;

    uint32_t u32Bit = 0x80000000;
    for (int i = 31; i >= 0; i--) {
        if (u32Value & u32Bit)
            return i + 1;
        u32Bit >>= 1;
    }

    return 0;
}

uint32_t
mkrn_swap2_min(uint32_t u32A, uint32_t u32B)
{
    return (u32A < u32B) ? u32A : u32B;
}

uint32_t
mkrn_swap2_max(uint32_t u32A, uint32_t u32B)
{
    return (u32A > u32B) ? u32A : u32B;
}

uint64_t
mkrn_swap2_min64(uint64_t u64A, uint64_t u64B)
{
    return (u64A < u64B) ? u64A : u64B;
}

uint64_t
mkrn_swap2_max64(uint64_t u64A, uint64_t u64B)
{
    return (u64A > u64B) ? u64A : u64B;
}

size_t
mkrn_swap2_strlen(const char *pStr)
{
    size_t len = 0;

    while (pStr && pStr[len])
        len++;

    return len;
}

char *
mkrn_swap2_strcpy(char *pDest, const char *pSrc)
{
    char *pD = pDest;

    while (pSrc && *pSrc)
        *pD++ = *pSrc++;

    *pD = '\0';

    return pDest;
}

int
mkrn_swap2_strcmp(const char *pS1, const char *pS2)
{
    while (pS1 && pS2 && *pS1 && *pS2
           && *pS1 == *pS2)
    {
        pS1++;
        pS2++;
    }

    if (!pS1 && !pS2)
        return 0;
    if (!pS1)
        return -1;
    if (!pS2)
        return 1;

    return *pS1 - *pS2;
}

int
mkrn_swap2_snprintf(char *pBuffer, size_t size,
                    const char *pFormat, ...)
{
    if (!pBuffer || !pFormat || size == 0)
        return 0;

    (void)pFormat;
    size_t len = mkrn_swap2_strlen(pFormat);
    if (len >= size)
        len = size - 1;

    mkrn_swap2_strcpy(pBuffer, pFormat);
    return (int)len;
}
