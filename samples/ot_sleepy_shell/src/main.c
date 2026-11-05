/*
 * Copyright (c) 2024 A Labs GmbH
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openthread_sleepy_shell_sample, LOG_LEVEL_INF);

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/net/openthread.h>
#include <zephyr/sys/atomic.h>

#include <openthread-system.h>
#include <openthread/link.h>
#include <openthread/thread.h>

#define OT_SLEEP_TOGGLE_BUTTON_NODE DT_ALIAS(sw0)

#if DT_HAS_CHOSEN(zephyr_shell_uart)
static const struct device *const shell_uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
#endif

#if defined(CONFIG_OPENTHREAD_MTD_SED)
/*
 * OPENTHREAD_MANUAL_START defaults to y on Silabs SoCs, so openthread_run()
 * never runs and the sleepy link mode and poll period it would derive from
 * CONFIG_OPENTHREAD_MTD_SED are never applied. The receiver would then stay on
 * when idle, holding the radio's EM1 requirement and making EM2 unreachable.
 */
static void sleepy_link_mode_init(void)
{
	otInstance *instance = openthread_get_default_instance();
	otLinkModeConfig mode;
	otError error;

	if (instance == NULL) {
		LOG_WRN("OpenThread instance is not available");
		return;
	}

	openthread_mutex_lock();

	/*
	 * Request the full Network Data as well, matching "ot mode n". Clearing
	 * only mRxOnWhenIdle would leave the stack's default minimal device mode
	 * with no bits set ("-"), which requests just the stable subset.
	 */
	mode = otThreadGetLinkMode(instance);
	mode.mRxOnWhenIdle = false;
	mode.mDeviceType = false;
	mode.mNetworkData = true;

	error = otThreadSetLinkMode(instance, mode);
	if (error != OT_ERROR_NONE) {
		LOG_WRN("Failed to set sleepy link mode: %d", error);
	} else {
		error = otLinkSetPollPeriod(instance, CONFIG_OPENTHREAD_POLL_PERIOD);
		if (error != OT_ERROR_NONE) {
			LOG_WRN("Failed to set poll period: %d", error);
		}
	}

	openthread_mutex_unlock();
}
#else
static void sleepy_link_mode_init(void)
{
}
#endif

#if defined(CONFIG_OPENTHREAD_PLATFORM_DEEP_SLEEP)
static atomic_t allow_deep_sleep;

bool platformPowerAllowDeepSleepCallback(void)
{
	return atomic_get(&allow_deep_sleep) != 0;
}

static void shell_uart_receive_set(bool enabled)
{
#if DT_HAS_CHOSEN(zephyr_shell_uart) && defined(CONFIG_UART_INTERRUPT_DRIVEN)
	if (!device_is_ready(shell_uart)) {
		LOG_WRN("Shell UART is not ready");
		return;
	}

	if (enabled) {
		uart_irq_rx_enable(shell_uart);
	} else {
		uart_irq_rx_disable(shell_uart);
	}
#else
	ARG_UNUSED(enabled);
#endif
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

	/*
	 * The shell keeps the UART receiver enabled, which prevents the USART
	 * driver from permitting EM2. Disable reception after the status message
	 * is queued; it is restored when the button selects EM1 again.
	 */
	if (atomic_get(&allow_deep_sleep)) {
		shell_uart_receive_set(false);
	}
}

static void sleep_toggle_button_pressed(const struct device *port, struct gpio_callback *cb,
					uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	atomic_set(&allow_deep_sleep, !atomic_get(&allow_deep_sleep));
	if (!atomic_get(&allow_deep_sleep)) {
		shell_uart_receive_set(true);
	}
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
	sleepy_link_mode_init();
	sleep_toggle_button_init();

	return 0;
}
