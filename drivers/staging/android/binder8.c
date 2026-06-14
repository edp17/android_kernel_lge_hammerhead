/*
 * Separate Binder protocol 8 instance for Waydroid.
 *
 * The primary binder.c build remains protocol 7 for the Sailfish/Hybris
 * Android adaptation. This translation unit registers only the dedicated
 * Waydroid Binder devices.
 */

#define BINDER_FORCE_PROTOCOL_8		1
#define BINDER_DRIVER_NAME		"binder8"
#define BINDER_DEVICE_CONFIG		CONFIG_ANDROID_BINDER_V8_DEVICES
#define BINDER_NO_TRACEPOINT_DEFINITIONS	1

#include "binder.c"