# DeepThaw

## Introduction
This is a small utility that can disable system reboot‑restore software (such as Deep Freeze, Reboot Restore, etc.) even when they are enabled and password‑protected.
  
Currently it only supports Deep Freeze (all versions) and Windows 10/11 64‑bit operating systems.
  
It uses a kernel‑mode driver to counter them, allowing it to bypass the reboot‑restore software regardless of its version.  
  
The method used by this utility can essentially defeat any system reboot‑restore software, regardless of how often they update, as long as they do not fundamentally change their underlying approach.  
  
## The Icon & UI
![](images/icon.png)  
![](images/ui.png)

## Build Environment
Visual Studio 2026 + Windows 11 SDK 10.0.28000.2114 + WDK 28000.1761

## TO-DO list （No idea when I'll get these TODOs done – I'm pretty lazy, to be honest :）
1. Improve support for Windows 10 32‑bit.
2. Add support for Windows 7 (and even Windows XP).
3. Support Reboot Restore RX.
4. Support Shadow Defender.
5. ~Thoroughly resolve the utility's stability in complex environments by permitting I/O operations only from the Configuration Manager.~
6. ~Harden the stability of programs in complex environments using Intel VT-x and AMD-V technologies(If it is necessary).~

## Remarks
### Some current situations
This tool employs a rather forceful approach that could, under certain circumstances, damage the Windows registry and render the system unbootable (although the risk is minimal). Prior to the release of version 1.4, I recommend exercising caution when using it.
### My Next Step
In version 1.4, I will use a completely new method (signature scanning) to disable DeepFreeze and a number of other similar applications. Although this method is version‑limited, it is absolutely stable. The current method will be made available as 'Force Mode' in later versions.