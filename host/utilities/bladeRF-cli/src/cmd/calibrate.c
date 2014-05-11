/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <string.h>
#include <libbladeRF.h>

#include "common.h"
#include "cmd.h"

/* The TXLPF calibration appears to fail the first couple times it is run.
 * Until this is resolved, this retry loop is at least better than forcing
 * the user to manually perform the retries. */
static int txlpf_workaround(struct bladerf *dev)
{
    int status, cal_status;
    unsigned int i;
    const unsigned int max_retries = 5;

    cal_status = BLADERF_ERR_UNEXPECTED;
    for (i = 0; cal_status != 0 && i < max_retries; i++) {
        status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
        if (status != 0) {
            return status;
        }

        if (i != 0) {
            printf("Retrying TXLPF calibration...\n");
        }

        /* Calibrate TX LPF Filter */
        cal_status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_TX_LPF);

        status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
        if (status != 0) {
            return status;
        }
    }

    return cal_status == 0 ? status : cal_status;
}

int cmd_calibrate(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        calibrate [module]
    */
    int status = 0;
    int fpga_status;

    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    /* The FPGA needs to be loaded */
    fpga_status = bladerf_is_fpga_configured(state->dev);
    if (fpga_status < 0) {
        state->last_lib_error = fpga_status;
        return CMD_RET_LIBBLADERF;
    } else if (fpga_status != 1) {
        return CMD_RET_NOFPGA;
    }


    if (argc == 1) {

        /* Calibrate LPF Tuning Module */
        status = bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_LPF_TUNING);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Enable the RX module */
        status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Calibrate RX LPF Filter */
        status = bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RX_LPF);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Calibrate TX LPF Filter */
        status = txlpf_workaround(state->dev);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Calibrate RX VGA2 */
        status = bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RXVGA2);
        if (status != 0) {
            goto cmd_calibrate_err;
        }

        /* Disable RX module */
        status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, false);

    } else if (argc == 2) {
        bladerf_cal_module module;

        /* Figure out which module we are calibrating */
        if (strcasecmp(argv[1], "tuning") == 0) {
            module = BLADERF_DC_CAL_LPF_TUNING;
        } else if (strcasecmp(argv[1], "txlpf") == 0) {
            status = txlpf_workaround(state->dev);
            goto cmd_calibrate_err;
        } else if (strcasecmp(argv[1], "rxlpf") == 0) {
            module = BLADERF_DC_CAL_RX_LPF;
            status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        } else if (strcasecmp(argv[1], "rxvga2") == 0) {
            module = BLADERF_DC_CAL_RXVGA2;
            status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, true);
        } else {
            cli_err(state, argv[0], "Invalid module provided (%s)", argv[1]);
            return CMD_RET_INVPARAM;
        }

        /* Calibrate it */
        status = bladerf_calibrate_dc(state->dev, module);

        if (module != BLADERF_DC_CAL_LPF_TUNING) {
            status = bladerf_enable_module(state->dev, BLADERF_MODULE_RX, false);
        }

    } else {
        return CMD_RET_INVPARAM;
    }

cmd_calibrate_err:
    if (status != 0) {
        state->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

    return status;
}
