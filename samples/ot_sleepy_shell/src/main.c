/*
 * Copyright (c) 2024 A Labs GmbH
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openthread_sleepy_shell_sample, LOG_LEVEL_INF);

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <openthread-system.h>

#define OT_SLEEP_TOGGLE_BUTTON_NODE DT_ALIAS(sw0)

#if defined(CONFIG_OPENTHREAD_PLATFORM_DEEP_SLEEP)
static atomic_t allow_deep_sleep;

bool platformPowerAllowDeepSleepCallback(void)
{
	return atomic_get(&allow_deep_sleep) != 0;
}
#endif

#if defined(CONFIG_OPENTHREAD_PLATFORM_DEEP_SLEEP) && defined(CONFIG_GPIO) && \
	DT_NODE_EXISTS(OT_SLEEP_TOGGLE_BUTTON_NODE) && \
	DT_NODE_HAS_STATUS(OT_SLEEP_TOGGLE_BUTTON_NODE, okay)

static const struct gpio_dt_spec sleep_toggle_button =
	GPIO_DT_SPEC_GET(OT_SLEEP_TOGGLE_BUTTON_NODE, gpios);
static struct gpio_callback sleep_toggle_button_cb_data;
static struct k_work sleep_toggle_button_work;

static void sleep_toggle_button_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_INF("OpenThread sleepy shell switching to %s mode",
		atomic_get(&allow_deep_sleep) ? "EM2 (sleep)" : "EM1 (idle)");
}

static void sleep_toggle_button_pressed(const struct device *port, struct gpio_callback *cb,
					uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	atomic_set(&allow_deep_sleep, !atomic_get(&allow_deep_sleep));
	k_work_submit(&sleep_toggle_button_work);
	otSysEventSignalPending();
}

static void sleep_toggle_button_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&sleep_toggle_button)) {
		LOG_WRN("Sleep toggle button is not ready");
		return;
	}

	ret = gpio_pin_configure_dt(&sleep_toggle_button, GPIO_INPUT);
	if (ret != 0) {
		LOG_WRN("Failed to configure sleep toggle button: %d", ret);
		return;
	}

	gpio_init_callback(&sleep_toggle_button_cb_data, sleep_toggle_button_pressed,
			   BIT(sleep_toggle_button.pin));
	ret = gpio_add_callback(sleep_toggle_button.port, &sleep_toggle_button_cb_data);
	if (ret != 0) {
		LOG_WRN("Failed to add sleep toggle callback: %d", ret);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&sleep_toggle_button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_WRN("Failed to enable sleep toggle interrupt: %d", ret);
		(void)gpio_remove_callback(sleep_toggle_button.port, &sleep_toggle_button_cb_data);
		return;
	}

	LOG_INF("OpenThread sleepy shell starting in EM1 (idle) mode");
	LOG_INF("Press Button 0 to toggle between EM2 (sleep) and EM1 (idle) modes");
}
#elif defined(CONFIG_OPENTHREAD_PLATFORM_DEEP_SLEEP)
static void sleep_toggle_button_init(void)
{
	LOG_WRN("This sample expects a sw0 button alias for EM1/EM2 toggling");
}
#else
static void sleep_toggle_button_init(void)
{
}
#endif

int main(void)
{
	/* Nothing to do here. The shell is automatically started in the background. */
#if defined(CONFIG_OPENTHREAD_PLATFORM_DEEP_SLEEP) && defined(CONFIG_GPIO) && \
	DT_NODE_EXISTS(OT_SLEEP_TOGGLE_BUTTON_NODE) && \
	DT_NODE_HAS_STATUS(OT_SLEEP_TOGGLE_BUTTON_NODE, okay)
	k_work_init(&sleep_toggle_button_work, sleep_toggle_button_work_handler);
#endif
	sleep_toggle_button_init();

	return 0;
}
