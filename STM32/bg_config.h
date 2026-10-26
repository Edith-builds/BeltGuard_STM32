#ifndef BG_CONFIG_H
#define BG_CONFIG_H
/* EDIT BEFORE COMMISSIONING. Zero disables scored health / localisation. */
#define BG_NODE_ID 1
#define BG_LOCALIZATION 1
#define BG_REMOTE_GATE 0 /* node 2: acquire features; laptop applies primary-node gate */
#define BG_CALIBRATED 0
#define BG_ENABLE_MIC 1
#define BG_ENABLE_LOAD 1
#define BG_VIB_FS 26667.0f
#define BG_MIC_FS 32000.0f /* replace by measured actual WS frequency */
#define BG_MIN_SPEED 0.01f
#define BG_MIN_LOAD_KG 0.10f
#define BG_LOAD_TARE 0
#define BG_LOAD_COUNTS_PER_KG 100000.0f /* placeholder; calibrate with known mass */
#define BG_LOOP_COUNTS 26943 /* EXAMPLE: 1.7634m belt, 50mm roller, 600PPR x4 */
#define BG_WINDOW_COUNTS 450 /* EXAMPLE +/-29mm; measure RF read zone */
#define BG_METRES_PER_COUNT (0.1570796327f/2400.0f)
#define BG_ENCODER_SIGN 1 /* +1 or -1; set with encoder A/B orientation */
#define BG_MISSING_STREAK 3 /* provisional maintenance warning, not damage proof */
/* Edit physical UID and measured positive encoder offsets, not equal spacing by assumption. */
#define BG_TAG_CONFIG { \
 {{0xDE,0xAD,0x00,0x01},4,0}, \
 {{0xDE,0xAD,0x00,0x02},4,6736}, \
 {{0xDE,0xAD,0x00,0x03},4,13472}, \
 {{0xDE,0xAD,0x00,0x04},4,20208} }
#endif
