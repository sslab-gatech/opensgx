/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009,2010 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>

#include "red_common.h"
#include "red_memslots.h"

static unsigned long __get_clean_virt(RedMemSlotInfo *info, QXLPHYSICAL addr)
{
    return addr & info->memslot_clean_virt_mask;
}

static void print_memslots(RedMemSlotInfo *info)
{
    int i;
    int x;

    for (i = 0; i < info->num_memslots_groups; ++i) {
        for (x = 0; x < info->num_memslots; ++x) {
            if (!info->mem_slots[i][x].virt_start_addr &&
                !info->mem_slots[i][x].virt_end_addr) {
                continue;
            }
            printf("id %d, group %d, virt start %lx, virt end %lx, generation %u, delta %lx\n",
                   x, i, info->mem_slots[i][x].virt_start_addr,
                   info->mem_slots[i][x].virt_end_addr, info->mem_slots[i][x].generation,
                   info->mem_slots[i][x].address_delta);
            }
    }
}

unsigned long get_virt_delta(RedMemSlotInfo *info, QXLPHYSICAL addr, int group_id)
{
    MemSlot *slot;
    int slot_id;
    int generation;

    if (group_id > info->num_memslots_groups) {
        spice_critical("group_id %d too big", group_id);
    }

    slot_id = get_memslot_id(info, addr);
    if (slot_id > info->num_memslots) {
        spice_critical("slot_id %d too big", slot_id);
    }

    slot = &info->mem_slots[group_id][slot_id];

    generation = get_generation(info, addr);
    if (generation != slot->generation) {
        spice_critical("address generation is not valid");
    }

    return (slot->address_delta - (addr - __get_clean_virt(info, addr)));
}

/* return 1 if validation successfull, 0 otherwise */
int validate_virt(RedMemSlotInfo *info, unsigned long virt, int slot_id,
                  uint32_t add_size, uint32_t group_id)
{
    MemSlot *slot;

    slot = &info->mem_slots[group_id][slot_id];
    if ((virt + add_size) < virt) {
        spice_critical("virtual address overlap");
        return 0;
    }

    if (virt < slot->virt_start_addr || (virt + add_size) > slot->virt_end_addr) {
        print_memslots(info);
        spice_critical("virtual address out of range\n"
              "    virt=0x%lx+0x%x slot_id=%d group_id=%d\n"
              "    slot=0x%lx-0x%lx delta=0x%lx",
              virt, add_size, slot_id, group_id,
              slot->virt_start_addr, slot->virt_end_addr, slot->address_delta);
        return 0;
    }
    return 1;
}

/*
 * return virtual address if successful, which may be 0.
 * returns 0 and sets error to 1 if an error condition occurs.
 */
unsigned long get_virt(RedMemSlotInfo *info, QXLPHYSICAL addr, uint32_t add_size,
                       int group_id, int *error)
{
    int slot_id;
    int generation;
    unsigned long h_virt;

    MemSlot *slot;

    *error = 0;
    if (group_id > info->num_memslots_groups) {
        spice_critical("group_id too big");
        *error = 1;
        return 0;
    }

    slot_id = get_memslot_id(info, addr);
    if (slot_id > info->num_memslots) {
        print_memslots(info);
        spice_critical("slot_id %d too big, addr=%" PRIx64, slot_id, addr);
        *error = 1;
        return 0;
    }

    slot = &info->mem_slots[group_id][slot_id];

    generation = get_generation(info, addr);
    if (generation != slot->generation) {
        print_memslots(info);
        spice_critical("address generation is not valid, group_id %d, slot_id %d, gen %d, slot_gen %d\n",
              group_id, slot_id, generation, slot->generation);
        *error = 1;
        return 0;
    }

    h_virt = __get_clean_virt(info, addr);
    h_virt += slot->address_delta;

    if (!validate_virt(info, h_virt, slot_id, add_size, group_id)) {
        *error = 1;
        return 0;
    }

    return h_virt;
}

void *validate_chunk(RedMemSlotInfo *info, QXLPHYSICAL data, uint32_t group_id,
                     uint32_t *data_size_out, QXLPHYSICAL *next_out, int *error)
{
    QXLDataChunk *chunk;
    uint32_t data_size;

    chunk = (QXLDataChunk *)get_virt(info, data, sizeof(QXLDataChunk), group_id,
                                     error);
    if (*error) {
        return NULL;
    }
    data_size = chunk->data_size;
    if (!validate_virt(info, (unsigned long)chunk->data, get_memslot_id(info, data),
                       data_size, group_id)) {
        *error = 1;
        return NULL;
    }
    *next_out = chunk->next_chunk;
    *data_size_out = data_size;

    return chunk->data;
}

void red_memslot_info_init(RedMemSlotInfo *info,
                           uint32_t num_groups, uint32_t num_slots,
                           uint8_t generation_bits,
                           uint8_t id_bits,
                           uint8_t internal_groupslot_id)
{
    uint32_t i;

    spice_assert(num_slots > 0);
    spice_assert(num_groups > 0);

    info->num_memslots_groups = num_groups;
    info->num_memslots = num_slots;
    info->generation_bits = generation_bits;
    info->mem_slot_bits = id_bits;
    info->internal_groupslot_id = internal_groupslot_id;

    info->mem_slots = spice_new(MemSlot *, num_groups);

    for (i = 0; i < num_groups; ++i) {
        info->mem_slots[i] = spice_new0(MemSlot, num_slots);
    }

    /* TODO: use QXLPHYSICAL_BITS */
    info->memslot_id_shift = 64 - info->mem_slot_bits;
    info->memslot_gen_shift = 64 - (info->mem_slot_bits + info->generation_bits);
    info->memslot_gen_mask = ~((QXLPHYSICAL)-1 << info->generation_bits);
    info->memslot_clean_virt_mask = (((QXLPHYSICAL)(-1)) >>
                                       (info->mem_slot_bits + info->generation_bits));
}

void red_memslot_info_add_slot(RedMemSlotInfo *info, uint32_t slot_group_id, uint32_t slot_id,
                               uint64_t addr_delta, unsigned long virt_start, unsigned long virt_end,
                               uint32_t generation)
{
    spice_assert(info->num_memslots_groups > slot_group_id);
    spice_assert(info->num_memslots > slot_id);

    info->mem_slots[slot_group_id][slot_id].address_delta = addr_delta;
    info->mem_slots[slot_group_id][slot_id].virt_start_addr = virt_start;
    info->mem_slots[slot_group_id][slot_id].virt_end_addr = virt_end;
    info->mem_slots[slot_group_id][slot_id].generation = generation;
}

void red_memslot_info_del_slot(RedMemSlotInfo *info, uint32_t slot_group_id, uint32_t slot_id)
{
    spice_assert(info->num_memslots_groups > slot_group_id);
    spice_assert(info->num_memslots > slot_id);

    info->mem_slots[slot_group_id][slot_id].virt_start_addr = 0;
    info->mem_slots[slot_group_id][slot_id].virt_end_addr = 0;
}

void red_memslot_info_reset(RedMemSlotInfo *info)
{
        uint32_t i;
        for (i = 0; i < info->num_memslots_groups; ++i) {
            memset(info->mem_slots[i], 0, sizeof(MemSlot) * info->num_memslots);
        }
}
