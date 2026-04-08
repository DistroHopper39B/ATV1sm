# ATV1sm
Bypass for the original Apple TV's EFI verification, allowing it to boot any EFI-capable OS

## Known Working OSes and Loaders
* Windows 8, 8.1, 10 (virtually unusable on 256MB RAM, especially later versions and without an SSD)
* NetBSD 10.1 (nouveau does not work, 32-bit EFI loader must be extracted from 64-bit NetBSD image for initial
  installation)
* GRUB2
* rEFInd
* 9front
* FreeLoader (MSVC)

## Known Broken OSes and Loaders
* Haiku (hangs somewhere during EFI initialization on the Apple TV and early Intel Macs)
* Windows Longhorn, Vista, and 7 EFI (requires CSM, could possibly work with UefiSeven+hacks)
* FreeLoader (MinGW GCC) (there's something about how it's linked that my hacked together PE loader does not like)

## Usage
### Getting Apple Service Diagnostics
This process involves using the internal Apple Service Diagnostics that Apple stores and AASPs used back in the
day to diagnose problems with the original Apple TV. I *heard* you could acquire a copy from here:
https://www.mediafire.com/file/zb8gsnh2ef820dk/693-6420-A.9999.dmg

Once you find this file, you must extract it. To do so using 7-Zip:
```shell
7z e 693-6420-A.9999.dmg "Apple TV Diag Installer App/Apple TV Diagnostics Installer.app/Contents/Resources/AppleTVDiags01.dmg"
7z x AppleTVDiags01.dmg "FieldDiags/System"
```

### Adding ATV1sm and GopShim to ASD
* Unzip `ATV1sm_0.1.1.zip` from the Releases page
* Copy `ASD` to `/System/Library/CoreServices/ASD.acm/Contents/MacOS/ASD`, replacing the existing file
* Copy `GopShimDxe.efi` to `/System/Library/CoreServices/Runtime_Files/EFI/Drivers/GopShimDxe.efi`

### Creating a USB flash drive with rEFInd (can be used to boot into other OSes)
*Note: these steps are for Linux because it's the easiest to do on Linux.*

1. Format a USB flash drive as GPT. In GParted:
   - Select the drive
   - Go to `Device -> Create Partition Table...`
   - Select the `gpt` partition type
2. Create a FAT32 partition
   - Partition -> New
   - File system: `fat32`
3. Change partition flags
   - Partition -> Manage Flags
   - Check `atvrecv`
4. Copy the `System` folder we extracted earlier to the root of this drive
5. Copy the `EFI` folder from `ATV1sm_0.1.1` to the root of this drive (this contains stripped-down rEFInd)

In the end, your USB drive's file layout should look like this:
```
EFI -> boot -> bootia32.efi (and other rEFInd files)
    -> tools -> shellia32.efi
       
System -> Library -> CoreServices -> ASD.acm -> Contents -> MacOS -> ASD (replaced)
                                  -> Runtime_Files -> EFI -> Drivers -> GopShimDxe.efi (added)
```

Now, plug it into your Apple TV and enjoy!

### Using without rEFInd on a fully-installed OS
If an OS is installed onto the hard drive, simply mount the ESP and copy the `System` folder to it.

*Note: Some Linux distros, notably Debian, might not install GRUB to `/EFI/BOOT/BOOTIA32.EFI` by default. If you get
`Failed to start \EFI\BOOT\BOOTIA32.EFI (Not Found)`, boot back into the system with rEFInd (which should be able to see
GRUB) and run `sudo grub-install --removable /dev/sdX`.*

## How this works
The original Apple TV has EFI firmware, just like every Intel Mac ever sold. However, unlike all non-T2 models, the
Apple TV's EFI firmware is locked to only Apple-signed executables. Previously, the only known useful executable to be
signed was the `boot.efi` executable that came with the Apple TV's version of 10.4.7. By replacing the `mach_kernel`
file that `boot.efi` loads with our own, we can get code execution after `ExitBootServices()` is called. This is what
every contemporary Apple TV loader (NTATV FreeLoader, atv-bootloader) does, but it has a few drawbacks:

* All hardware detection and access (e.g. reading from a hard disk, accessing a USB keyboard) must be done manually,
like in an OS, meaning that any bootloader must be either very limited or very complicated
* Memory allocation must be done manually, like in an OS

For this reason, conventional bootloaders such as GRUB, Windows Boot Manager, etc. never worked on the orignal Apple TV.

However, one of those signed executables is Apple's internal diagnostics suite, known as Apple Software Diagnostics
(ASD) for the Apple TV. While in EFI boot services mode, ASD loads and executes a file named `ASD` with no signature
checks, which allows us to get code execution before `ExitBootServices()` is called.

However, `ASD` is not a standard EFI executable. Instead, it is a Mach-O formatted file that is executed by signed
ASD executables (specifically `TestSupport.efi`). It is still useful though, because we can:
* Calculate the offset to the `ImageHandle` and `SystemTable` from the address of a static variable inside
`TestSupport.efi` (thank you [Rairii/Wack0](https://github.com/wack0) for figuring this out)
* Use these values to call `LoadImage` and `StartImage` to chainload a second EFI executable

However, there is still a problem: `LoadImage` and `StartImage` still only work with signed EFI executables. However,
since we now have full control over the system during the boot services, phase, we can just swap them out for ones with
no signature checks! This has been done before in [shim](https://github.com/rhboot/shim) for GPLv3 code to be
executed on UEFI Secure Boot (which works in a similar way to the Apple TV's EFI verification), and in
[OpenCore](github.com/acidanthera/OpenCorePkg) for loading Apple EFI executables on non-Apple hardware. My 
implementation of this, located in this repository, is based on the implementation of `LoadImage` and `StartImage` (as
well as `UnloadImage` and `Exit`, which have to match in order to correctly work) in OpenCore. And it's a hacked
together mess in many ways, but it works! Adding on [GopShim](https://distrohopper39b.com/gopshim) allows even more OSes
to load with working graphics.

## Known issues
* This is **NOT** to be taken as a reference implementation of, well, anything lol. The code for `Load`/`Start`/
`UnloadImage`/`Exit` is haphazardly copy-pasted from OpenCore, resulting in some bootloaders (notable MinGW-compiled
UEFI FreeLoader) not working.
* Fat binaries (like those used in Mac OS X) do not work and will fall back to the locked original bootloader for
now.

## Libraries used
* Portions of OpenCore's [OcBootManagementLib](https://github.com/acidanthera/OpenCorePkg/tree/master/Library/OcBootManagementLib)
* [GNU-EFI](https://github.com/ncroxon/gnu-efi) (hard fork for Clang/Mach-O support, the build process is also
completely different)
* [Baselibc](https://github.com/PetteriAimonen/Baselibc) string/memory functions

## Compiling (macOS)
* Install the Xcode Command Line Tools
* Install nasm
* Clone this repo and `cd` into it
* Type `make`

## Compiling (Linux)
* Install Clang, autoconf, automake, libtool, git, nasm
* Compile version 986 of [cctools-port](https://github.com/tpoechtrager/cctools-port) (newer versions are more
complicated to set up)
```shell
git clone https://github.com/tpoechtrager/cctools-port.git -b 986-ld64-711
cd cctools-port/cctools
./configure --prefix=/opt/cross --target=i386-apple-darwin8
make -j$(nproc)
sudo make install
```
* Clone this repo and `cd` into it
* Type `make`

## Compiling (Windows)
Use WSL or a Linux VM or something.

## Special thanks
* [Rairii/Wack0](https://github.com/wack0) for determining the offsets

## Name
ATV1sm is a combination of the words Apple TV 1 (ATV1) and autism (since **A**pple
**S**oftware **D**iagnostic and **A**utism **S**pectrum **D**isorder have the same acronym, and also because it took an
immense amount of autism to put this thing together). Let me know if you can think of anything better.