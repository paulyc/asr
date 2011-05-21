#include "decoder.h"

		/*
		{
        .name = "traktor_a",
        .desc = "Traktor Scratch, side A",
        .resolution = 2000,
        .flags = SWITCH_PRIMARY | SWITCH_POLARITY | SWITCH_PHASE,
        .bits = 23,
        .seed = 0x134503,
        .taps = 0x041040,
        .length = 1500000,
        .safe = 1480000,
        .lookup = false        
    },
    {
        .name = "traktor_b",
        .desc = "Traktor Scratch, side B",
        .resolution = 2000,
        .flags = SWITCH_PRIMARY | SWITCH_POLARITY | SWITCH_PHASE,
        .bits = 23,
        .seed = 0x32066c,
        .taps = 0x041040, // same as side A 
        .length = 2110000,
        .safe = 2090000,
        .lookup = false
    },*/

/* The number of bits to form the hash, which governs the overall size
 * of the hash lookup table, and hence the amount of chaining */

#define HASH_BITS 16

#define HASH(timecode) ((timecode) & ((1 << HASH_BITS) - 1))
#define NO_SLOT ((unsigned)-1)

/* Initialise an empty hash lookup table to store the given number
 * of timecode -> position lookups */

int lut_init(struct lut_t *lut, int nslots)
{
    int n, hashes;
    size_t bytes;

    hashes = 1 << HASH_BITS;
    bytes = sizeof(struct slot_t) * nslots + sizeof(slot_no_t) * hashes;

    fprintf(stderr, "Lookup table has %d hashes to %d slots"
            " (%d slots per hash, %zuKb)\n",
            hashes, nslots, nslots / hashes, bytes / 1024);

    lut->slot = (slot_t*)malloc(sizeof(struct slot_t) * nslots);
    if (lut->slot == NULL) {
        perror("malloc");
        return -1;
    }

    lut->table = (slot_no_t*)malloc(sizeof(slot_no_t) * hashes);
    if (lut->table == NULL) {
        perror("malloc");
        return -1;
    }

    for (n = 0; n < hashes; n++)
        lut->table[n] = NO_SLOT;

    lut->avail = 0;

    return 0;
}

void lut_clear(struct lut_t *lut)
{
    free(lut->table);
}

void lut_push(struct lut_t *lut, unsigned int timecode)
{
    unsigned int hash;
    slot_no_t slot_no;
    struct slot_t *slot;

    slot_no = lut->avail++; /* take the next available slot */

    slot = &lut->slot[slot_no];
    slot->timecode = timecode;

    hash = HASH(timecode);
    slot->next = lut->table[hash];
    lut->table[hash] = slot_no;
}

unsigned int lut_lookup(struct lut_t *lut, unsigned int timecode)
{
    unsigned int hash;
    slot_no_t slot_no;
    struct slot_t *slot;

    hash = HASH(timecode);
    slot_no = lut->table[hash];

    while (slot_no != NO_SLOT) {
        slot = &lut->slot[slot_no];
        if (slot->timecode == timecode)
            return slot_no;
        slot_no = slot->next;
    }

    return (unsigned)-1;
}
