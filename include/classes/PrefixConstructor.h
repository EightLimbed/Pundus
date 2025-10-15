#ifndef PREFIXER_H
#define PREFIXER_H

// need to rewrite so lists are pre-made (faster for block editing, slower for full regenerations)

#include <array>
#include <vector>
#include <bit>
#include <cstdint>
#include <iostream>

class PrefixConstructor {
public:
    // ts shizzle pmo but I only have one chunk atm so it needs to be tunable.
    static constexpr int CHUNK_SIZE = 32;
    static constexpr int NUM_VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    static constexpr int TERM_BITS  = 32;
    static constexpr int NUM_TERMS  = NUM_VOXELS / TERM_BITS; // 32768 / 32 = 1024
    static constexpr int PREFIX_BITS = 10; // 10-bit prefix values
    static constexpr int PREFIX_TERMS = (NUM_TERMS * PREFIX_BITS) / 32; // 320 packed uint32s

    PrefixConstructor() {}

    std::array<uint32_t, PREFIX_TERMS> GeneratePrefixes(std::array<uint32_t, NUM_TERMS> bitCloud) {

        // builds array of prefixes
        std::array<int, NUM_TERMS> Prefixes = {};
        uint32_t RTotal = 0; // running total
        for (int i = 0; i < NUM_TERMS; ++i) {
            uint32_t Count = __builtin_popcount(bitCloud[i]);
            RTotal += Count;
            std::cout << "\nTerm Count: " << Count << " Running Total: " << RTotal;
            Prefixes[i] = RTotal;
        
        }

        // packs array of prefixes to 10 bits each
        std::array<uint32_t, PREFIX_TERMS> prefixPacked = {};
        uint32_t bitPos = 0;
        for (uint32_t val : Prefixes) {
            uint32_t wordIdx = bitPos / 32;
            uint32_t bitOffset = bitPos % 32;

            // mask value to 10 bits
            uint32_t maskedVal = val & 0x3FF;

            prefixPacked[wordIdx] |= maskedVal << bitOffset;

            // handle packing between terms (didnt write this part, stole from stack overflow after getting angry trying to think about it)
            if (bitOffset + PREFIX_BITS > 32) {
                uint32_t spillBits = (bitOffset + PREFIX_BITS) - 32;
                prefixPacked[wordIdx + 1] |= maskedVal >> (PREFIX_BITS - spillBits);
            }

            bitPos += PREFIX_BITS;
        }

        return prefixPacked;
    }


};
#endif