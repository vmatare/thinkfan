/********************************************************************
 * system.h: Declarations and prototypes for system.c
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file contains all functions that are specific to dealing with
 * either /sys/class/hwmon or the /proc/acpi/ibm interface. They are
 * referenced in the main program via function pointers.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

#ifndef SYSTEM_H
#define SYSTEM_H

int get_temp_ibm();
int get_temp_sysfs();
int depulse_and_get_temp_ibm();
int depulse_and_get_temp_sysfs();
void setfan_ibm();
void setfan_sysfs();
void setfan_sysfs_safe();
int init_fan_ibm();
int preinit_fan_sysfs();
int init_fan_sysfs_once();
int init_fan_sysfs();
void uninit_fan_ibm();
void uninit_fan_sysfs();

#endif
