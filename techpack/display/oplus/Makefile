# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2018-2020 Oplus. All rights reserved.
ccflags-y += -I$(srctree)/include/drm -I$(srctree)/techpack/display/msm -I$(srctree)/techpack/display/msm/dsi -I$(srctree)/techpack/display/msm/dp
ccflags-y += -I$(srctree)/techpack/display/msm/sde
ccflags-y += -I$(srctree)/techpack/display/rotator
ccflags-y += -I$(srctree)/techpack/display/oplus
ifeq ($(OPLUS_FEATURE_PXLW_IRIS5),yes)
ccflags-y += -I$(srctree)/techpack/display/iris
ccflags-y += -DOPLUS_FEATURE_PXLW_IRIS5
endif #/* OPLUS_FEATURE_PXLW_IRIS5 */

obj-$(CONFIG_DRM_OPLUS) += oppo_dsi_support.o \
	oppo_display_private_api.o \
	oppo_dc_diming.o \
	oppo_onscreenfingerprint.o \
	oppo_aod.o \
	oppo_ffl.o \
	oppo_display_panel.o \
	oppo_display_panel_power.o \
	oppo_display_panel_seed.o \
	oplus_display_panel_cabc.o \
	oppo_display_panel_common.o \
	oppo_display_panel_hbm.o
