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

    PrefixConstructor() {}

    std::array<uint32_t, NUM_TERMS> GeneratePrefixes(std::array<uint32_t, NUM_TERMS> bitCloud) {

        // builds array of prefixes
        std::array<uint32_t, NUM_TERMS> Prefixes = {};
        uint32_t RTotal = 0; // running total
        for (int i = 0; i < NUM_TERMS; ++i) {
            uint32_t Count = __builtin_popcount(bitCloud[i]);
            RTotal += Count;
            //std::cout << "\nTerm Count: " << Count << " Running Total: " << RTotal;
            Prefixes[i] = RTotal;
        
        }
        return Prefixes;
    }


};
#endif