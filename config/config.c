/*
 * Copyright © 2006-2007 Daniel Stone
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Daniel Stone <daniel@fooishbar.org>
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "os.h"
#include "inputstr.h"
#include "hotplug.h"
#include "config-backends.h"

void
config_pre_init(void)
{
#ifdef CONFIG_UDEV
    if (!config_udev_pre_init())
        ErrorF("[config] failed to pre-init udev\n");
#endif
}

void
config_init(void)
{
#ifdef CONFIG_UDEV
    if (!config_udev_init())
        ErrorF("[config] failed to initialise udev\n");
#elif defined(CONFIG_NEED_DBUS)
    if (config_dbus_core_init()) {
#ifdef CONFIG_DBUS_API
        if (!config_dbus_init())
            ErrorF("[config] failed to initialise D-Bus API\n");
#endif
#ifdef CONFIG_HAL
        if (!config_hal_init())
            ErrorF("[config] failed to initialise HAL\n");
#endif
    }
    else {
        ErrorF("[config] failed to initialise D-Bus core\n");
    }
#elif defined(CONFIG_WSCONS)
    if (!config_wscons_init())
        ErrorF("[config] failed to initialise wscons\n");
#endif
}

void
config_fini(void)
{
#if defined(CONFIG_UDEV)
    config_udev_fini();
#elif defined(CONFIG_NEED_DBUS)
#ifdef CONFIG_HAL
    config_hal_fini();
#endif
#ifdef CONFIG_DBUS_API
    config_dbus_fini();
#endif
    config_dbus_core_fini();
#elif defined(CONFIG_WSCONS)
    config_wscons_fini();
#endif
}

void
config_odev_probe(config_odev_probe_proc_ptr probe_callback)
{
#if defined(CONFIG_UDEV_KMS)
    config_udev_odev_probe(probe_callback);
#endif
}

static void
remove_device(const char *backend, DeviceIntPtr dev)
{
    /* this only gets called for devices that have already been added */
    LogMessage(X_INFO, "config/%s: removing device %s\n", backend, dev->name);

    /* Call PIE here so we don't try to dereference a device that's
     * already been removed. */
    OsBlockSignals();
    ProcessInputEvents();
    DeleteInputDeviceRequest(dev);
    OsReleaseSignals();
}

void
remove_devices(const char *backend, const char *config_info)
{
    DeviceIntPtr dev, next;

    for (dev = inputInfo.devices; dev; dev = next) {
        next = dev->next;
        if (dev->config_info && strcmp(dev->config_info, config_info) == 0)
            remove_device(backend, dev);
    }
    for (dev = inputInfo.off_devices; dev; dev = next) {
        next = dev->next;
        if (dev->config_info && strcmp(dev->config_info, config_info) == 0)
            remove_device(backend, dev);
    }
}

BOOL
device_is_duplicate(const char *config_info)
{
    DeviceIntPtr dev;

    for (dev = inputInfo.devices; dev; dev = dev->next) {
        if (dev->config_info && (strcmp(dev->config_info, config_info) == 0))
            return TRUE;
    }

    for (dev = inputInfo.off_devices; dev; dev = dev->next) {
        if (dev->config_info && (strcmp(dev->config_info, config_info) == 0))
            return TRUE;
    }

    return FALSE;
}

struct OdevAttributes *
config_odev_allocate_attribute_list(void)
{
    struct OdevAttributes *attriblist;

    attriblist = malloc(sizeof(struct OdevAttributes));
    if (!attriblist)
        return NULL;

    xorg_list_init(&attriblist->list);
    return attriblist;
}

void
config_odev_free_attribute_list(struct OdevAttributes *attribs)
{
    config_odev_free_attributes(attribs);
    free(attribs);
}

Bool
config_odev_add_attribute(struct OdevAttributes *attribs, int attrib,
                          const char *attrib_name)
{
    struct OdevAttribute *oa;

    oa = malloc(sizeof(struct OdevAttribute));
    if (!oa)
        return FALSE;

    oa->attrib_id = attrib;
    oa->attrib_name = strdup(attrib_name);
    xorg_list_append(&oa->member, &attribs->list);
    return TRUE;
}

void
config_odev_free_attributes(struct OdevAttributes *attribs)
{
    struct OdevAttribute *iter, *safe;

    xorg_list_for_each_entry_safe(iter, safe, &attribs->list, member) {
        xorg_list_del(&iter->member);
        free(iter->attrib_name);
        free(iter);
    }
}
