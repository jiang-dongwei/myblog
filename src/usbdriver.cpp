/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 Open Stick Community (gp2040-ce.info)
 */

#ifndef _USBDRIVER_CPP_
#define _USBDRIVER_CPP_

#include "tusb.h"
#include "BoardConfig.h"
#include "drivermanager.h"

#include <cstring>

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_ENABLED
#define FIGHTPAD12SLIM_ESP32_PROXY_ENABLED 0
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED
#define FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED FIGHTPAD12SLIM_ESP32_PROXY_ENABLED
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_NOTIF
#define FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_NOTIF 0x87
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_OUT
#define FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_OUT 0x08
#endif

#ifndef FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_IN
#define FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_IN 0x88
#endif

static bool usb_mounted;
static bool usb_suspended;

#if FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED
static uint8_t fightpadCompositeDeviceDescriptor[18];
static uint8_t fightpadCompositeConfigDescriptor[512];

static const uint8_t *fightpad_append_cdc_descriptor(const uint8_t *baseDescriptor) {
	if (baseDescriptor == nullptr || DriverManager::getInstance().isConfigMode()) {
		return baseDescriptor;
	}

	uint16_t baseLength = baseDescriptor[2] | (baseDescriptor[3] << 8);
	uint8_t baseInterfaces = baseDescriptor[4];
	uint16_t totalLength = baseLength + TUD_CDC_DESC_LEN;

	if (totalLength > sizeof(fightpadCompositeConfigDescriptor)) {
		return baseDescriptor;
	}

	memcpy(fightpadCompositeConfigDescriptor, baseDescriptor, baseLength);

	const uint8_t cdcDescriptor[TUD_CDC_DESC_LEN] = {
		TUD_CDC_DESCRIPTOR(
			baseInterfaces,
			0,
			FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_NOTIF,
			8,
			FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_OUT,
			FIGHTPAD12SLIM_ESP32_PROXY_CDC_EP_IN,
			64
		)
	};
	memcpy(&fightpadCompositeConfigDescriptor[baseLength], cdcDescriptor, sizeof(cdcDescriptor));

	fightpadCompositeConfigDescriptor[2] = totalLength & 0xFF;
	fightpadCompositeConfigDescriptor[3] = (totalLength >> 8) & 0xFF;
	fightpadCompositeConfigDescriptor[4] = baseInterfaces + 2;

	return fightpadCompositeConfigDescriptor;
}

static const uint8_t *fightpad_composite_device_descriptor(const uint8_t *baseDescriptor) {
	if (baseDescriptor == nullptr || DriverManager::getInstance().isConfigMode()) {
		return baseDescriptor;
	}

	memcpy(fightpadCompositeDeviceDescriptor, baseDescriptor, sizeof(fightpadCompositeDeviceDescriptor));
	fightpadCompositeDeviceDescriptor[4] = TUSB_CLASS_MISC;
	fightpadCompositeDeviceDescriptor[5] = 0x02; // Common subclass
	fightpadCompositeDeviceDescriptor[6] = 0x01; // IAD protocol
	return fightpadCompositeDeviceDescriptor;
}
#endif

bool get_usb_mounted(void) {
	return usb_mounted;
}

bool get_usb_suspended(void) {
	return usb_suspended;
}

const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *driver_count) {
	*driver_count = 1;
	return DriverManager::getInstance().getDriver()->get_class_driver();
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
	return DriverManager::getInstance().getDriver()->get_report(report_id, report_type, buffer, reqlen);
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
	DriverManager::getInstance().getDriver()->set_report(report_id, report_type, buffer, bufsize);
}

// Invoked when device is mounted
void tud_mount_cb(void)
{
	usb_mounted = true;
	usb_suspended = false;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
	usb_mounted = false;
	usb_suspended = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
	(void)remote_wakeup_en;
	usb_suspended = true;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
	usb_suspended = false;
}

// Vendor Controlled XFER occured
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request) {
	return DriverManager::getInstance().getDriver()->vendor_control_xfer_cb(rhport, stage, request);
}


// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
	return DriverManager::getInstance().getDriver()->get_descriptor_string_cb(index, langid);
}

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb() {
	const uint8_t *descriptor = DriverManager::getInstance().getDriver()->get_descriptor_device_cb();
#if FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED
	return fightpad_composite_device_descriptor(descriptor);
#else
	return descriptor;
#endif
}

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf) {
	return DriverManager::getInstance().getDriver()->get_hid_descriptor_report_cb(itf);
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
	const uint8_t *descriptor = DriverManager::getInstance().getDriver()->get_descriptor_configuration_cb(index);
#if FIGHTPAD12SLIM_ESP32_PROXY_CDC_DESC_ENABLED
	return fightpad_append_cdc_descriptor(descriptor);
#else
	return descriptor;
#endif
}

uint8_t const* tud_descriptor_device_qualifier_cb() {
	return DriverManager::getInstance().getDriver()->get_descriptor_device_qualifier_cb();
}

#endif
