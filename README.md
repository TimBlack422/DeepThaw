# DeepThaw

## Introduction
This is a small utility that can disable system reboot‑restore software (such as Deep Freeze, Reboot Restore, etc.) even when they are enabled and password‑protected.
  
Currently it only supports Deep Freeze (all versions) and Windows 10/11 64‑bit operating systems.
  
It uses a kernel‑mode driver to counter them, allowing it to bypass the reboot‑restore software regardless of its version.  
  
The method used by this tool can essentially defeat any system reboot‑restore software, regardless of how often they update, as long as they do not fundamentally change their underlying approach.  
  
## The Icon & UI
![](images/icon.png)  
![](images/ui.png)


## TO-DO list 
1. Improve support for Windows 10 32‑bit.
2. Add support for Windows 7 (and even Windows XP).
3. Support Reboot Restore RX.
4. Support Shadow Defender.