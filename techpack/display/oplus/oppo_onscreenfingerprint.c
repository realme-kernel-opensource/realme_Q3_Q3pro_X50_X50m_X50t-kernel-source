// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include "sde_crtc.h"
#include "oppo_onscreenfingerprint.h"
#include "oppo_display_private_api.h"

#define DSI_PANEL_OPPO_DUMMY_VENDOR_NAME  "PanelVendorDummy"
#define DSI_PANEL_OPPO_DUMMY_MANUFACTURE_NAME  "dummy1024"

bool oppo_pcc_enabled = false;
bool oppo_skip_pcc = false;
bool apollo_backlight_enable = false;
struct drm_msm_pcc oppo_save_pcc;
int oppo_dimlayer_hbm = 0;

extern int oppo_underbrightness_alpha;
extern int oppo_dimlayer_dither_threshold;
extern u32 oppo_last_backlight;
extern int oppo_panel_alpha;
extern int hbm_mode;
extern bool oppo_ffl_trigger_finish;
extern int dynamic_osc_clock;
int oppo_dimlayer_hbm_vblank_count = 0;
atomic_t oppo_dimlayer_hbm_vblank_ref = ATOMIC_INIT(0);
struct backlight_apollo_maplist *p_apollo_maplist = NULL;
struct backlight_apollo_vmaplist *p_apollo_vmaplist = NULL;
bool oppo_enhance_mipi_strength = false;
#ifdef OPLUS_BUG_STABILITY
int oppo_dfps_idle_off = 0;

bool is_lcd_cabc_support = false;
#endif /*OPLUS_BUG_STABILITY*/

static struct oppo_brightness_alpha brightness_alpha_lut[] = {
	{0, 0xff},
	{1, 0xee},
	{2, 0xe8},
	{3, 0xe6},
	{4, 0xe5},
	{6, 0xe4},
	{10, 0xe0},
	{20, 0xd5},
	{30, 0xce},
	{45, 0xc6},
	{70, 0xb7},
	{100, 0xad},
	{150, 0xa0},
	{227, 0x8a},
	{300, 0x80},
	{400, 0x6e},
	{500, 0x5b},
	{600, 0x50},
	{800, 0x38},
	{1023, 0x18},
};

static struct oppo_brightness_alpha brightness_alpha_lut_dc[] = {
	{0, 0xff},
	{1, 0xE0},
	{2, 0xd1},
	{3, 0xd0},
	{4, 0xcf},
	{5, 0xc9},
	{6, 0xc7},
	{8, 0xbe},
	{10, 0xb6},
	{15, 0xaa},
	{20, 0x9c},
	{30, 0x92},
	{45, 0x7c},
	{70, 0x5c},
	{100, 0x40},
	{120, 0x2c},
	{140, 0x20},
	{160, 0x1c},
	{180, 0x16},
	{200, 0x8},
	{223, 0x0},
};

int oppo_get_panel_brightness(void)
{
	struct dsi_display *display = get_main_display();

	if (!display)
		return 0;

	return display->panel->bl_config.bl_level;
}

static int bl_to_alpha(int brightness)
{
	struct dsi_display *display = get_main_display();
	struct oppo_brightness_alpha *lut = NULL;
	int count = 0;
	int i = 0;
	int alpha;

	if (!display)
		return 0;

	if (display->panel->ba_seq && display->panel->ba_count) {
		count = display->panel->ba_count;
		lut = display->panel->ba_seq;
	} else {
		count = ARRAY_SIZE(brightness_alpha_lut);
		lut = brightness_alpha_lut;
	}

	for (i = 0; i < count; i++){
		if (lut[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = lut[0].alpha;
	else if (i == count)
		alpha = lut[count - 1].alpha;
	else
		alpha = interpolate(brightness, lut[i-1].brightness,
				    lut[i].brightness, lut[i-1].alpha,
				    lut[i].alpha, display->panel->oppo_priv.bl_interpolate_nosub);

	return alpha;
}

static int bl_to_alpha_dc(int brightness)
{
	int level = ARRAY_SIZE(brightness_alpha_lut_dc);
	int i = 0;
	int alpha;

	for (i = 0; i < ARRAY_SIZE(brightness_alpha_lut_dc); i++){
		if (brightness_alpha_lut_dc[i].brightness >= brightness)
			break;
	}

	if (i == 0)
		alpha = brightness_alpha_lut_dc[0].alpha;
	else if (i == level)
		alpha = brightness_alpha_lut_dc[level - 1].alpha;
	else
		alpha = interpolate(brightness,
			brightness_alpha_lut_dc[i-1].brightness,
			brightness_alpha_lut_dc[i].brightness,
			brightness_alpha_lut_dc[i-1].alpha,
			brightness_alpha_lut_dc[i].alpha, false);
	return alpha;
}

static int brightness_to_alpha(int brightness)
{
	int alpha;
	struct dsi_display *display = get_main_display();

	if (!display)
		return 0;

	if (brightness == 0 || brightness == 1) {
		if (!strcmp(display->panel->oppo_priv.vendor_name, "AMS643YE01") ||
		!strcmp(display->panel->oppo_priv.vendor_name, "AMS643YE01IN20057")) {
			/*do nothing*/
		} else {
			brightness = oppo_last_backlight;
		}
	}

	if (oppo_dimlayer_hbm)
		alpha = bl_to_alpha(brightness);
	else
		alpha = bl_to_alpha_dc(brightness);

	return alpha;
}


int oplus_apollo_backlight_list_alloc(void)
{
	if (p_apollo_maplist == NULL) {
		p_apollo_maplist = kmalloc(sizeof(struct backlight_apollo_maplist), GFP_KERNEL);
		if (p_apollo_maplist == NULL) {
			pr_err("can not malloc p_apollo_maplist");
			return -EINVAL;
		}
		pr_err("%s success, size = %d\n", __func__, sizeof(struct backlight_apollo_maplist));
	}

	if (p_apollo_vmaplist == NULL) {
		p_apollo_vmaplist = kmalloc(sizeof(struct backlight_apollo_vmaplist), GFP_KERNEL);
		if (p_apollo_vmaplist == NULL) {
			pr_err("can not malloc p_apollo_maplist");
			return -EINVAL;
		}
		pr_err("%s success, size = %d\n", __func__, sizeof(struct backlight_apollo_vmaplist));
	}

	pr_err("%s has already alloced\n", __func__);

	return 0;
}

int oplus_display_set_apollo_backlight_maplist(void *data)
{
	struct backlight_apollo_maplist *p_apollo = data;

	pr_err("%s", __func__);
	memcpy(p_apollo_maplist, p_apollo, sizeof(struct backlight_apollo_maplist));

	return 0;
}

int oplus_display_set_apollo_backlight_vmaplist(void *data)
{
	struct backlight_apollo_vmaplist *p_apollo = data;

	pr_err("%s", __func__);
	memcpy(p_apollo_vmaplist, p_apollo, sizeof(struct backlight_apollo_maplist));

	return 0;
}

static int oplus_find_index_invmaplist(uint32_t bl_level)
{
	int index = 0;
	for (index; index < APOLLO_BACKLIGHT_LENS; index++) {
		if (p_apollo_vmaplist->p_backlight_apollo_vlist[index] == bl_level) {
			//pr_err("%s hit the level[%d] = %d\n", __func__, index, bl_level);
			return index;
		}
	}
	pr_err("%s error\n", __func__);
	return -1;
}

static int oppo_get_panel_brightness_to_alpha(void)
{
	struct dsi_display *display = get_main_display();
	int index = 0;
	uint32_t brightness_panel = 0;

	if (!display)
		return 0;
	if (oppo_panel_alpha)
		return oppo_panel_alpha;

	if (hbm_mode)
		return 0;

	if (!oppo_ffl_trigger_finish)
		return brightness_to_alpha(FFL_FP_LEVEL);

	DSI_DEBUG("%s bl_level = %d\n", __func__, display->panel->bl_config.bl_level);

	if (apollo_backlight_enable) {
		index = oplus_find_index_invmaplist(display->panel->bl_config.bl_level);
		if (index >= 0) {
			brightness_panel = p_apollo_maplist->p_backlight_apollo_panel_list[index];
			pr_err("%s index = %d, panel_level = %d, apolo_level = %d\n",__func__, index, brightness_panel,
				p_apollo_vmaplist->p_backlight_apollo_vlist[index]);

			return brightness_to_alpha(brightness_panel);
		}
	}

	return brightness_to_alpha(display->panel->bl_config.bl_level);
}

int dsi_panel_parse_oppo_fod_config(struct dsi_panel *panel)
{
	int rc = 0;
	int i;
	u32 length = 0;
	u32 count = 0;
	u32 size = 0;
	u32 *arr_32 = NULL;
	const u32 *arr;
	struct dsi_parser_utils *utils = &panel->utils;
	struct oppo_brightness_alpha *seq;

	if (panel->host_config.ext_bridge_mode)
		return 0;

	arr = utils->get_property(utils->data, "oppo,dsi-fod-brightness", &length);
	if (!arr) {
		DSI_DEBUG("[%s] oppo,dsi-fod-brightness  not found\n", panel->name);
		return -EINVAL;
	}

	if (length & 0x1) {
		DSI_ERR("[%s] oppo,dsi-fod-brightness length error\n", panel->name);
		return -EINVAL;
	}

	DSI_DEBUG("RESET SEQ LENGTH = %d\n", length);
	length = length / sizeof(u32);
	size = length * sizeof(u32);

	arr_32 = kzalloc(size, GFP_KERNEL);
	if (!arr_32) {
		rc = -ENOMEM;
		goto error;
	}

	rc = utils->read_u32_array(utils->data, "oppo,dsi-fod-brightness",
					arr_32, length);
	if (rc) {
		DSI_ERR("[%s] cannot read dsi-fod-brightness\n", panel->name);
		goto error_free_arr_32;
	}

	count = length / 2;
	size = count * sizeof(*seq);
	seq = kzalloc(size, GFP_KERNEL);
	if (!seq) {
		rc = -ENOMEM;
		goto error_free_arr_32;
	}

	panel->ba_seq = seq;
	panel->ba_count = count;

	for (i = 0; i < length; i += 2) {
		seq->brightness = arr_32[i];
		seq->alpha = arr_32[i + 1];
		seq++;
	}

error_free_arr_32:
	kfree(arr_32);
error:
	return rc;
}

static int dsi_panel_parse_oppo_backlight_remapping_config(struct dsi_panel *panel)
{
	int rc = 0;
	int i;
	u32 length = 0;
	u32 count = 0;
	u32 size = 0;
	u32 *arr_32 = NULL;
	const u32 *arr;
	struct dsi_parser_utils *utils = &panel->utils;
	struct oppo_brightness_alpha *bl_remap;

	if (panel->host_config.ext_bridge_mode)
		return 0;

	panel->oppo_priv.bl_interpolate_nosub = utils->read_bool(utils->data,
			"oppo,bl_interpolate_nosub");

	arr = utils->get_property(utils->data, "oppo,dsi-brightness-remapping", &length);
	if (!arr) {
		DSI_DEBUG("[%s] oppo,dsi-fod-brightness  not found\n", panel->name);
		return -EINVAL;
	}

	if (length & 0x1) {
		DSI_ERR("[%s] oppo,dsi-fod-brightness length error\n", panel->name);
		return -EINVAL;
	}

	DSI_DEBUG("RESET SEQ LENGTH = %d, interpolate_nosub = %d\n", length, panel->oppo_priv.bl_interpolate_nosub ? 1 : 0);
	length = length / sizeof(u32);
	size = length * sizeof(u32);

	arr_32 = kzalloc(size, GFP_KERNEL);
	if (!arr_32) {
		rc = -ENOMEM;
		goto error;
	}

	rc = utils->read_u32_array(utils->data, "oppo,dsi-brightness-remapping",
					arr_32, length);
	if (rc) {
		DSI_ERR("[%s] cannot read oppo,dsi-brightness-remapping\n", panel->name);
		goto error_free_arr_32;
	}

	count = length / 2;
	size = count * sizeof(*bl_remap);
	bl_remap = kzalloc(size, GFP_KERNEL);
	if (!bl_remap) {
		rc = -ENOMEM;
		goto error_free_arr_32;
	}

	panel->oppo_priv.bl_remap = bl_remap;
	panel->oppo_priv.bl_remap_count = count;

	for (i = 0; i < length; i += 2) {
		bl_remap->brightness = arr_32[i];
		bl_remap->alpha = arr_32[i + 1];
		bl_remap++;
	}

error_free_arr_32:
	kfree(arr_32);
error:
	return rc;
}

int dsi_panel_parse_oppo_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = &panel->utils;
	int ret = 0;

	dsi_panel_parse_oppo_fod_config(panel);
	dsi_panel_parse_oppo_backlight_remapping_config(panel);

	panel->oppo_priv.vendor_name = utils->get_property(utils->data,
				"oppo,mdss-dsi-vendor-name", NULL);
	if (!panel->oppo_priv.vendor_name) {
		pr_err("Failed to found panel name, using dumming name\n");
		panel->oppo_priv.vendor_name = DSI_PANEL_OPPO_DUMMY_VENDOR_NAME;
	}
	panel->oppo_priv.manufacture_name = utils->get_property(utils->data,
				"oppo,mdss-dsi-manufacture", NULL);
	if (!panel->oppo_priv.manufacture_name) {
		pr_err("Failed to found panel name, using dumming name\n");
		panel->oppo_priv.manufacture_name = DSI_PANEL_OPPO_DUMMY_MANUFACTURE_NAME;
	}
	/*#ifdef OPLUS_BUG_STABILITY*/
        panel->oppo_priv.oplus_fp_hbm_config_flag = utils->read_bool(utils->data,
                                  "oplus,fp-hbm-config-flag");
        /*#ifdef OPLUS_BUG_STABILITY*/
	if (!strcmp(panel->oppo_priv.vendor_name, "ANA6706")) {
		oppo_enhance_mipi_strength = true;
	} else {
		oppo_enhance_mipi_strength = false;
	}

	#ifdef OPLUS_BUG_STABILITY
	panel->oppo_priv.dfps_idle_off = utils->read_bool(utils->data,
		"oppo,dfps-idle-off");

	DSI_INFO("oppo,dfps-idle-off: %s", panel->oppo_priv.dfps_idle_off ? "true" : "false");
	oppo_dfps_idle_off = panel->oppo_priv.dfps_idle_off;
	#endif /*OPLUS_BUG_STABILITY*/

	panel->oppo_priv.is_pxlw_iris5 = utils->read_bool(utils->data,
				"oppo,is_pxlw_iris5");
	DSI_INFO("is_pxlw_iris5: %s", panel->oppo_priv.is_pxlw_iris5 ? "true" : "false");

#ifdef OPLUS_FEATURE_AOD_RAMLESS
	panel->oppo_priv.is_aod_ramless = utils->read_bool(utils->data,
			"oppo,aod_ramless");
	DSI_INFO("aod ramless mode: %s", panel->oppo_priv.is_aod_ramless ? "true" : "false");
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

	apollo_backlight_enable = utils->read_bool(utils->data,
				"oplus,apollo_backlight_enable");
	DSI_INFO("apollo_backlight_enable: %s", apollo_backlight_enable ? "true" : "false");

	panel->oppo_priv.is_osc_support = utils->read_bool(utils->data, "oplus,osc-support");
	pr_info("[%s]osc mode support: %s", __func__, panel->oppo_priv.is_osc_support ? "Yes" : "Not");

	ret = utils->read_u32(utils->data, "oplus,mdss-dsi-osc-clk-mode0-rate",
				&panel->oppo_priv.osc_clk_mode0_rate);
	if (ret) {
		pr_err("[%s]failed get panel parameter: oplus,mdss-dsi-osc-clk-mode0-rate\n", __func__);
		panel->oppo_priv.osc_clk_mode0_rate = 0;
	}
	dynamic_osc_clock = panel->oppo_priv.osc_clk_mode0_rate;

	ret = utils->read_u32(utils->data, "oplus,mdss-dsi-osc-clk-mode1-rate",
				&panel->oppo_priv.osc_clk_mode1_rate);
	if (ret) {
		pr_err("[%s]failed get panel parameter: oplus,mdss-dsi-osc-clk-mode1-rate\n", __func__);
		panel->oppo_priv.osc_clk_mode1_rate = 0;
	}

	panel->oppo_priv.is_tps65132_support = utils->read_bool(utils->data,
				"oppo,tps65132_support");
	DSI_INFO("oppo,tps65132_support;: %s", panel->oppo_priv.is_tps65132_support ? "true" : "false");

#ifdef OPLUS_FEATURE_BACKLIGHT_GAMMA
	panel->oppo_priv.is_lcd_gamma_support = utils->read_bool(utils->data,
				"oplus,lcd_gamma_support");
	DSI_INFO("oplus,lcd_gamma_support: %s", panel->oppo_priv.is_lcd_gamma_support ? "true" : "false");
#endif /* OPLUS_FEATURE_BACKLIGHT_GAMMA */

	panel->oppo_priv.lcd_cabc_support = utils->read_bool(utils->data,
				"oplus,lcd_cabc_support");
	is_lcd_cabc_support = panel->oppo_priv.lcd_cabc_support;
	DSI_INFO("oplus,lcd_cabc_support: %s", is_lcd_cabc_support ? "true" : "false");
	return 0;
}

int dsi_panel_parse_oppo_mode_config(struct dsi_display_mode *mode,
				struct dsi_parser_utils *utils)
{
	int rc;
	struct dsi_display_mode_priv_info *priv_info;
	int val = 0;

	priv_info = mode->priv_info;

	rc = utils->read_u32(utils->data, "oppo,fod-on-vblank", &val);
	if (rc) {
		DSI_ERR("oppo,fod-on-vblank is not defined, rc=%d\n", rc);
		priv_info->fod_on_vblank = 0;
	} else {
		priv_info->fod_on_vblank = val;
		DSI_INFO("oppo,fod-on-vblank is %d", val);
	}

	rc = utils->read_u32(utils->data, "oppo,fod-off-vblank", &val);
	if (rc) {
		DSI_ERR("oppo,fod-on-vblank is not defined, rc=%d\n", rc);
		priv_info->fod_off_vblank = 0;
	} else {
		priv_info->fod_off_vblank = val;
		DSI_INFO("oppo,fod-off-vblank is %d", val);
	}

	return 0;
}

bool sde_crtc_get_dimlayer_mode(struct drm_crtc_state *crtc_state)
{
	struct sde_crtc_state *cstate;

	if (!crtc_state)
		return false;

	cstate = to_sde_crtc_state(crtc_state);
	return !!cstate->fingerprint_dim_layer;
}

bool sde_crtc_get_fingerprint_mode(struct drm_crtc_state *crtc_state)
{
	struct sde_crtc_state *cstate;

	if (!crtc_state)
		return false;

	cstate = to_sde_crtc_state(crtc_state);
	return !!cstate->fingerprint_mode;
}

bool sde_crtc_get_fingerprint_pressed(struct drm_crtc_state *crtc_state)
{
	struct sde_crtc_state *cstate;

	if (!crtc_state)
		return false;

	cstate = to_sde_crtc_state(crtc_state);
	return cstate->fingerprint_pressed;
}

int sde_crtc_set_onscreenfinger_defer_sync(struct drm_crtc_state *crtc_state, bool defer_sync)
{
	struct sde_crtc_state *cstate;

	if (!crtc_state)
		return -EINVAL;

	cstate = to_sde_crtc_state(crtc_state);
	cstate->fingerprint_defer_sync = defer_sync;
	return 0;
}

int sde_crtc_config_fingerprint_dim_layer(struct drm_crtc_state *crtc_state, int stage)
{
	struct sde_crtc_state *cstate;
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	struct sde_hw_dim_layer *fingerprint_dim_layer;
	int alpha = oppo_get_panel_brightness_to_alpha();
	struct sde_kms *kms;

	kms = _sde_crtc_get_kms_(crtc_state->crtc);
	if (!kms || !kms->catalog) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	cstate = to_sde_crtc_state(crtc_state);

	if (cstate->num_dim_layers == SDE_MAX_DIM_LAYERS - 1) {
		pr_err("failed to get available dim layer for custom\n");
		return -EINVAL;
	}

	if ((stage + SDE_STAGE_0) >= kms->catalog->mixer[0].sblk->maxblendstages) {
		return -EINVAL;
	}

	fingerprint_dim_layer = &cstate->dim_layer[cstate->num_dim_layers];
	fingerprint_dim_layer->flags = SDE_DRM_DIM_LAYER_INCLUSIVE;
	fingerprint_dim_layer->stage = stage + SDE_STAGE_0;

	fingerprint_dim_layer->rect.x = 0;
	fingerprint_dim_layer->rect.y = 0;
	fingerprint_dim_layer->rect.w = mode->hdisplay;
	fingerprint_dim_layer->rect.h = mode->vdisplay;
	fingerprint_dim_layer->color_fill = (struct sde_mdss_color) {0, 0, 0, alpha};
	cstate->fingerprint_dim_layer = fingerprint_dim_layer;
	oppo_underbrightness_alpha = alpha;

	return 0;
}

bool is_skip_pcc(struct drm_crtc *crtc)
{
	if (OPPO_DISPLAY_POWER_DOZE_SUSPEND == get_oppo_display_power_status() ||
	    OPPO_DISPLAY_POWER_DOZE == get_oppo_display_power_status() ||
	    sde_crtc_get_fingerprint_mode(crtc->state))
		return true;

	return false;
}

bool sde_cp_crtc_update_pcc(struct drm_crtc *crtc)
{
	struct sde_hw_cp_cfg hw_cfg;
	struct sde_hw_dspp *hw_dspp;
	struct sde_hw_mixer *hw_lm;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);
	struct sde_mdss_cfg *catalog = NULL;
	u32 num_mixers = sde_crtc->num_mixers;
	bool pcc_skip_mode;
	int i = 0;

	if (!is_dsi_panel(&sde_crtc->base))
		return false;

	pcc_skip_mode = is_skip_pcc(crtc);
	if (oppo_skip_pcc == pcc_skip_mode)
		return false;

	oppo_skip_pcc = pcc_skip_mode;
	memset(&hw_cfg, 0, sizeof(hw_cfg));

	if (!pcc_skip_mode && oppo_pcc_enabled){
		hw_cfg.payload = &oppo_save_pcc;
		hw_cfg.len = sizeof(oppo_save_pcc);
	}

	hw_cfg.num_of_mixers = sde_crtc->num_mixers;
	hw_cfg.last_feature = 0;

	for (i = 0; i < num_mixers; i++) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_dspp || i >= DSPP_MAX)
			continue;
		hw_cfg.dspp[i] = hw_dspp;
	}

	catalog = get_kms_(&sde_crtc->base)->catalog;
	hw_cfg.broadcast_disabled = catalog->dma_cfg.broadcast_disabled;
	for (i = 0; i < num_mixers; i++) {

		hw_lm = sde_crtc->mixers[i].hw_lm;
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		if (!hw_lm)
			continue;
		if (!hw_dspp || !hw_dspp->ops.setup_pcc)
			continue;

		hw_cfg.ctl = sde_crtc->mixers[i].hw_ctl;
		hw_cfg.mixer_info = hw_lm;
		hw_cfg.displayh = num_mixers * hw_lm->cfg.out_width;
		hw_cfg.displayv = hw_lm->cfg.out_height;
		hw_dspp->ops.setup_pcc(hw_dspp, &hw_cfg);
	}
	return true;
}


bool _sde_encoder_setup_dither_for_onscreenfingerprint(struct sde_encoder_phys *phys,
						  void *dither_cfg, int len, struct sde_hw_pingpong *hw_pp)
{
	struct drm_encoder *drm_enc = phys->parent;
	struct drm_msm_dither dither;

	if (!drm_enc || !drm_enc->crtc)
		return -EFAULT;

	if (!sde_crtc_get_dimlayer_mode(drm_enc->crtc->state))
		return -EINVAL;

	if (len != sizeof(dither))
		return -EINVAL;

	if (oppo_get_panel_brightness_to_alpha() < oppo_dimlayer_dither_threshold)
		return -EINVAL;

	if(hw_pp == 0){
		return 0;
	}

	memcpy(&dither, dither_cfg, len);
	dither.c0_bitdepth = 6;
	dither.c1_bitdepth = 6;
	dither.c2_bitdepth = 6;
	dither.c3_bitdepth = 6;
	dither.temporal_en = 1;

	phys->hw_pp->ops.setup_dither(hw_pp, &dither, len);

	return 0;
}

int sde_plane_check_fingerprint_layer(const struct drm_plane_state *drm_state)
{
	struct sde_plane_state *pstate;

	if (!drm_state)
		return 0;

	pstate = to_sde_plane_state(drm_state);

	return sde_plane_get_property(pstate, PLANE_PROP_CUSTOM);
}

int oplus_display_get_dimlayer_hbm(void *data)
{
	uint32_t *dimlayer_hbm = data;

	(*dimlayer_hbm) = oppo_dimlayer_hbm;

	return 0;
}

int oplus_display_set_dimlayer_hbm(void *data)
{
	struct dsi_display *display = get_main_display();
	struct drm_connector *dsi_connector = display->drm_conn;
	uint32_t *dimlayer_hbm = data;
	int err = 0;
	int value = (*dimlayer_hbm);

	value = !!value;
	if (oppo_dimlayer_hbm == value)
		return 0;
	if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
		pr_err("[%s]: display not ready\n", __func__);
	} else {
		err = drm_crtc_vblank_get(dsi_connector->state->crtc);
		if (err) {
			pr_err("failed to get crtc vblank, error=%d\n", err);
		} else {
			/* do vblank put after 5 frames */
			oppo_dimlayer_hbm_vblank_count = 5;
			atomic_inc(&oppo_dimlayer_hbm_vblank_ref);
		}
	}
	oppo_dimlayer_hbm = value;

#ifdef VENDOR_EDIT
	pr_err("debug for oppo_display_set_dimlayer_hbm set oppo_dimlayer_hbm = %d\n", oppo_dimlayer_hbm);
#endif

	return 0;
}

