#include <zephyr/kernel.h>
#include <zephyr/drivers/led.h>
#include <openthread/platform/radio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/net/openthread.h>
#include <string.h>
#include <zephyr/drivers/hwinfo.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define STEP_MS		72
#define STEP_CNT	24
#define LED_CNT		3
#define STEP_RATIO	2
#define STEP_BLINK	4
#define BR_SOLID	25
#define BR_BLINK	50
#define BR_SCALE(v)	v / 4

enum indication_mode {
	OFF = 0,
	BREATH_SLOW,
	BREATH_FAST,
	SOLID,
};

typedef struct indication_s {
	const struct device *const leds;
	enum indication_mode mode[LED_CNT];
	size_t breath_step_slow[LED_CNT];
	size_t breath_step_fast[LED_CNT];
	size_t to_blink[LED_CNT];
	size_t step_div;
} indication_t;

static indication_t indication = {
	.leds = DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds)),
	.mode = { OFF, OFF, OFF },
	.breath_step_slow = { 0, 0, 0 },
	.breath_step_fast = { 0, 0, 0 },
	.step_div = 0,
};

static const uint8_t pwm_curve[STEP_CNT] = {
	BR_SCALE(0), BR_SCALE(17), BR_SCALE(32), BR_SCALE(45), BR_SCALE(57),  BR_SCALE(68), BR_SCALE(77), BR_SCALE(85), BR_SCALE(91), BR_SCALE(95), BR_SCALE(98), BR_SCALE(100),
	BR_SCALE(100), BR_SCALE(98), BR_SCALE(95), BR_SCALE(91), BR_SCALE(85), BR_SCALE(77), BR_SCALE(68), BR_SCALE(57), BR_SCALE(45), BR_SCALE(32), BR_SCALE(17), BR_SCALE(0),
};

otError __real_otPlatRadioEnable(otInstance *i);
otError __wrap_otPlatRadioEnable(otInstance *i)
{
	otError err = __real_otPlatRadioEnable(i);

	if (err == OT_ERROR_NONE) {
		indication.mode[0] = SOLID;
	}
	return err;
}

otError __real_otPlatRadioDisable(otInstance *i);
otError __wrap_otPlatRadioDisable(otInstance *i)
{
	indication.mode[0] = BREATH_SLOW;
	return __real_otPlatRadioDisable(i);
}

void __real_otPlatRadioTxDone(otInstance *i, otRadioFrame *f, otRadioFrame *ack, otError e);
void __wrap_otPlatRadioTxDone(otInstance *i, otRadioFrame *f, otRadioFrame *ack, otError e)
{
	indication.to_blink[1] = STEP_BLINK;
	__real_otPlatRadioTxDone(i, f, ack, e);
}

void __real_otPlatRadioReceiveDone(otInstance *i, otRadioFrame *f, otError e);
void __wrap_otPlatRadioReceiveDone(otInstance *i, otRadioFrame *f, otError e)
{
	if (e == OT_ERROR_NONE) {
		indication.to_blink[2] = STEP_BLINK;
	}
	__real_otPlatRadioReceiveDone(i, f, e);
}

void __wrap_otPlatRadioGetIeeeEui64(otInstance *instance, uint8_t *ieee_eui64)
{
	uint8_t id[OT_EXT_ADDRESS_SIZE] = {0};

	ARG_UNUSED(instance);

	/* Stable EUI-64 from the SoC unique ID; unused bytes stay 0 */
	(void)hwinfo_get_device_id(id, sizeof(id));

	/* Set the locally-administered bit, clear multicast bit */
	id[0] = (id[0] | 0x02U) & 0xFEU;

	memcpy(ieee_eui64, id, sizeof(id));
}

static const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));

int main(void)
{
	int wdt_channel_id;
	int ret;
	bool update_slow;

	if (!device_is_ready(indication.leds) || !device_is_ready(wdt)) {
		k_msleep(1000);
		if (!device_is_ready(indication.leds) || !device_is_ready(wdt)) {
			led_set_brightness(indication.leds, 0, BR_SOLID);
			led_set_brightness(indication.leds, 1, BR_SOLID);
			led_set_brightness(indication.leds, 2, BR_SOLID);
			return -ENODEV;
		}
	}

	struct wdt_timeout_cfg wdt_config = {
		.flags = WDT_FLAG_RESET_SOC,
		.window.max = 1500,
	};

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id < 0) {
		led_set_brightness(indication.leds, 0, BR_SOLID);
		led_set_brightness(indication.leds, 1, BR_SOLID);
		led_set_brightness(indication.leds, 2, BR_SOLID);
		return 0;
	}

	ret = wdt_setup(wdt, 0);
	if (ret < 0) {
		led_set_brightness(indication.leds, 1, BR_SOLID);
		led_set_brightness(indication.leds, 2, BR_SOLID);
		return 0;
	}

	indication.mode[0] = BREATH_SLOW;

	while (true) {
		indication.step_div++;
		update_slow = false;
		if (indication.step_div >= STEP_RATIO) {
			indication.step_div = 0;
			update_slow = true;
		}
		for (size_t i = 0; i < LED_CNT; i++) {
			if (indication.mode[i] == SOLID) {
				led_set_brightness(indication.leds, i, BR_SOLID);
			} else if (indication.mode[i] == BREATH_SLOW && update_slow) {
				indication.breath_step_slow[i] += 1;
				if (indication.breath_step_slow[i] >= STEP_CNT) {
					indication.breath_step_slow[i] = 0;
				}
				led_set_brightness(indication.leds, i,
					pwm_curve[indication.breath_step_slow[i]]);
			} else if (indication.mode[i] == BREATH_FAST) {
				indication.breath_step_fast[i] += 1;
				if (indication.breath_step_fast[i] >= STEP_CNT) {
					indication.breath_step_fast[i] = 0;
				}
				led_set_brightness(indication.leds, i,
					pwm_curve[indication.breath_step_fast[i]]);
			} else if (indication.to_blink[i] > 0) {
				indication.to_blink[i]--;
				if (indication.to_blink[i] == 0) {
					led_set_brightness(indication.leds, i, 0);
				} else {
					led_set_brightness(indication.leds, i, BR_BLINK);
				}
			}
		}
		wdt_feed(wdt, wdt_channel_id);
		k_msleep(STEP_MS);
	}
}
