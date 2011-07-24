/********************************************************************
 * system.h: Declarations and prototypes for system.c
 * This file contains all functions that are specific to dealing with
 * either /sys/class/hwmon or the /proc/acpi/ibm interface. They are
 * referenced in the main program via function pointers.
 *
 * This file is part of thinkfan.

 * thinkfan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * thinkfan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with thinkfan.  If not, see <http://www.gnu.org/licenses/>.
 ********************************************************************/

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
