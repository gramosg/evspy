# Evspy

Evspy is a general purpose kernel-mode keylogger in (early) development stage.

The file from where you can read the registered keystrokes is /proc/driver/evspy
by default. Only root can read it. Beware users: evspy can troll you.

Don't be evil.

## Compile
    $ make

## Load
    # insmod evspy.ko

## Unload
    # rmmod evspy

## Is it already loaded?
    $ modinfo evspy

## Persistence

* With dkms:

        # make [install, uninstall]

* Manually:
    Copy it into your kernel module dir:

        # cp evspy.ko /lib/modules/$(uname -r)/kernel/drivers/input/evspy.ko

    and update module database:

        # depmod -a
    (in some distros you could also need to add it to some rc/config file)

    Once it has been installed, you can load it when you want with

        # modprobe evspy

## OTHER

A patch is supplied (evspy.patch) to be able to compile a kernel with evspy
included. If KERN is the directory where your kernel is located, just copy the
patch there (KERN/) and copy all the evspy files (*.c, *.h, maps, kmap) to
KERN/drivers/input/. Then, cd to KERN and apply the patch:

    $ patch -p1 < evspy.patch

Then you should be able to configure the kernel to include evspy just like
any other module:

    $ make menuconfig
        Device Drivers --> Input device support --> Event based keylogger
    $ ...
