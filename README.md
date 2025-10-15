# HL-KMD: PoC for CVE-2025-61476

This repository contains a chunk of source code (Reversed) for wamsdk.sys, a kernel-mode driver to demonstrate the privilege escalation vulnerability identified in Watchdog Anti-Malware v4.8.2.0.
The vulnerability exists in the driver's I/O dispatch routine for IRP_MJ_DEVICE_CONTROL, which fails to properly validate user-supplied input, allowing an attacker with local administrative privileges to execute code with kernel-level (SYSTEM) privileges.

## Installation (Loading the Driver)
Use the sc (Service Control) utility to create and start the service.
- `sc create vldrv type= kernel binPath= C:\Temp\wamsdk.sys`
- `sc start vldrv`
