/**
 * \file
 * \brief Module to set the System Memory Page Tables of the Xeon Phi
 */

/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <string.h>
#include <barrelfish/barrelfish.h>

#include <dev/xeon_phi/xeon_phi_smpt_dev.h>

#include "xeon_phi_internal.h"
#include "smpt.h"

/*
 * XXX: Setting a system memory pagetable entry basically enables the coprocessor
 *      to access host memory directly. This is an open issue and should be
 *      restricted by capabilities somehow.
 *
 *      Anyhow, since the access to this memory range is controlled as normal
 *      memory, we can trust the kernels on the card not to introduce potential
 *      vulnerabilities...
 *
 *      Note: the system memory pagetables are rather coarse grained (16GB)
 */

/**
 * \brief calculates the base address of the Xeon Phi GDDR
 *
 *        This function will return 0 if the ID is the local card.
 *
 * \param phi   the local xeon phi
 * \param id    the xeon phi id of the other card
 *
 * \returns base address of GDDR (0 if local)
 */
lpaddr_t smpt_get_coprocessor_address(struct xeon_phi *phi,
                                      uint8_t id)
{
    if (id == phi->id) {
        return 0;
    }

    uint8_t slot = xeon_phi_smpt_system_page_num - 1;
    if (id < phi->id) {
        slot = slot - id;
    } else {
        slot = slot - id + 1;
    }

    return XEON_PHI_SYSMEM_BASE + ((lpaddr_t)slot * XEON_PHI_SYSMEM_PAGE_SIZE);
}

/**
 * \brief sets the entry of the SMPT for the Xeon Phi with given id
 *
 * \param phi   the local Xeon Phi
 * \param id    ID of the other Xeon Phi
 * \param addr  the physical (host)address
 *
 * \returns 1 on SUCCESS
 *          0 on attempt to set the own SMPT entry
 */
uint8_t smpt_set_coprocessor_address(struct xeon_phi *phi,
                                     uint8_t id,
                                     lpaddr_t addr)
{
    if (id == phi->id) {
        return 0;
    }

    uint8_t slot = xeon_phi_smpt_system_page_num - 1;
    if (id < phi->id) {
        slot = slot - id;
    } else {
        slot = slot - id + 1;
    }

    smpt_set_address(phi, slot, addr, 1);

    return 1;
}

/**
 * \brief Sets an entry in the system memory pagetable to a give address
 *
 * \param phi      reference to the card structure
 * \param slot     pagetable entry to set
 * \param address  host address to set
 * \param snooping enable snooping = 1, disable snooping = 0
 *
 * \return SYS_ERR_OK on success
 */
void smpt_set_address(struct xeon_phi *phi,
                      uint8_t slot,
                      lpaddr_t address,
                      uint8_t snooping)
{
    lpaddr_t e_address = (address >> xeon_phi_smpt_system_page_shift);
    xeon_phi_smpt_entry_t e = xeon_phi_smpt_entry_default;
    e = xeon_phi_smpt_entry_host_address_insert(e, (uint32_t) e_address);
    if (snooping) {
        snooping = xeon_phi_smpt_snooping_enabled;
    } else {
        snooping = xeon_phi_smpt_snooping_disabled;
    }
    e = xeon_phi_smpt_entry_snoop_disabled_insert(e, snooping);

    smpt_set_entry(phi, slot, e);
}

/**
 * \brief Resets the system memory page tables
 */
void smpt_reset(struct xeon_phi *phi)
{
    for (uint32_t i = 0; i < xeon_phi_smpt_system_page_num; ++i) {
        smpt_clear_entry(phi, i);
    }
}

/**
 * \brief initializes the system memory page tables with a
 *        1:1 mapping
 *
 * \return SYS_ERR_OK on success
 */
errval_t smpt_init(struct xeon_phi *phi)
{
    if (phi->smpt) {
        if (phi->smpt->smpt_enabled) {
            debug_printf("WARNING: SMPT already setup");
            return SYS_ERR_OK;
        }
    }
    phi->smpt = calloc(1, sizeof(struct smpt_info));
    if (!phi->smpt) {
        return LIB_ERR_MALLOC_FAIL;
    }

    xeon_phi_smpt_initialize(&phi->smpt->smpt_register, XEON_PHI_MMIO_TO_SBOX(phi));

    lpaddr_t host_address = 0;

    uint32_t netries = xeon_phi_smpt_system_page_num - XEON_PHI_NUM_MAX;

    for (uint32_t i = 0; i < netries; ++i) {
        smpt_set_address(phi, i, host_address, 1);
        host_address += xeon_phi_smpt_system_page_size;
    }

    phi->smpt->smpt_enabled = 1;

    return SYS_ERR_OK;
}
