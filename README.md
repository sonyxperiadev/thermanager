## Overview ##
Thermanager is an attempt to simplify & unify thermal management for platforms requiring it.  It was initially written to enable users outside of Sony Mobile to customize and potentially improve the thermal management of Xperia devices, which previously had been managed using proprietary applications.

The core concept behind thermanager is to move most platform and device specific configuration into a simple configuration file, adding code only when it makes sense.  This way, when porting thermal configurations to a new device or platform, only the configuration file needs to be updated.  This also has the benifit of allowing users to tweak the configuration without requiring any knowledge of programming in general.

Thermanager currently uses an XML based configuration file, but can be easily modified to parse any DOM-like format.  There are three important types of sections within the configuration file, which are intended to allow a wide range of configuration possibilities:
* Resources: Lists I/O resources such as intents or sysfs files.
* Control: Lists mitigations actions which can be taken for a mitigation plan.
* Configuration: Lists thresholds at which mitigation actions should be taken.

## Resources Types ##
Resources are used to provide I/O functionality.  There are several different types of resources, which provide different types of I/O capabilities:
* "sysfs" - Usually a text file in /sys which holds a value, or can have a value written to it.  
`<resource name="gpu-fan" type="sysfs">/sys/class/fan/gpu0/rpm</resource>`

* "tz" - A thermal zone directory for reading temperatures.  
`<resource name="zone0" type="tz">/sys/class/thermal/thermal_zone0</resource>`

* "cpufreq" - A cpufreq directory for reading current frequency, and writing maximum frequency.  
`<resource name="cpu0-freq" type="cpufreq">/sys/devices/system/cpu/cpu0/cpufreq</resource>`

* "msm-adc" - Read only special sysfs file with format "Result: %ld Raw:%ld" provided by the msm-adc drivers.  
`<resource name="pmic-temp" type="msm-adc">/sys/devices/pm8xxx-adc/pmic_temp</resource>`

* "echo" - Write only, will output to console. Used purely for debugging.  
`<resource name="debug" type="echo" />`

* "halt" - Write only resource type, which on write will begin a shutdown sequence lasting a specified number of seconds.  
`<resource name="shutdown" type="halt" delay="5" />`

* "intent" - Write only resource type, which will send an intent as specified with the written data as the extra "notice".  
`<resource name="notify" type="intent">com.example.ThermalNotifer</resource>`

* "alias" - Wrapper resource alias type to reference other resources.  
`<resource name="gpu-temp" type="alias" resource="QKgpu1-tz0" />`

* "deadband" - Read-Write alias resource which will ignore all values within the specified dead-band range.  
`<resource name="debounce" type="deadband" size="25" resource="noisy-temp" />`

* "union" - Wrapper resource type used to group resources.  
`<resource name="cpuX" type="union"><resource name="cpu0" /><resource name="cpu1" /></resource>`

## Control ##
Control sections are intended to define a list of mitigation levels for a specific mitigation plan. Classic examples would be mitigating the CPU frequency, or enabling active cooling. The mitigation levels should start at 0 and increase from there.  Each mitigation can contain any number of 'values' which are written to specified resources which the mitigation level is activated.  A control will only have one mitigation level active at a time, and it will be the highest level selected by any configuration threshold.

## Configuration ##
A configuration section lists the thresholds at which mitigations should be activated.  Each threshold contains the mitigation levels which should be activated when the threshold is entered. Each threshold has a 'trigger' and 'clear' attribute, specifying within what range the threshold should activate based on the configuration's sensor.  If the sensor's value rises above 'trigger' the threshold's mitigations will be activated. If the sensor's value then falls below 'clear' the threshold's mitigations will be deactivated.  The default threshold's 'trigger' and 'clear' attributes should be unspecified.
