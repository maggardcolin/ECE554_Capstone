# TODO 

## How to use
1. Start by downloading the petalinux 2024.1 image at https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html. We cannot provide this image to you due to ToS and laws and such.
1.1. Place the download inside this directory

2. Run `make`. This will create the podman container. If you have docker only, good luck (try sudo).

3. Place your xsa, bit, and bsp inside the `petalinux-build` directory. This is mounted inside the container.
3.1. TODO: The bsp in question will be provided in a future commit

4. Exec into the container with `podman exec -it petalinux bash`

5. Run the installer with `./petalinx*.run`
5.1. Follow the instructions. Its more or less 'Enter -> Q -> y -> Enter -> Q -> y -> Enter'
5.2. When prompted for a directory, please use `petalinux` rather than the default. This is to make your life easier.

6. Run the following commands in order:
```
source /petalinux/settings.sh

cd /petalinux-build

petalinux-create project -t zynqMP -s *.bsp

cd petalinux-base-8GB

petalinux-config --get-hw-description=../

# In the menu, go to *Image Packaging Configuration* and disable *Copy final images to tftpboot*
# Then save and exit. This will take a minute or two to close

petalinux-build

# This will take maybe an hour depending on machine

cp /petalinux-build/*.bit images/linux/

petalinux-package boot --u-boot --fpga --force

petalinux-package wic --i ./images/linux --bootfiles "BOOT.BIN image.ub system.dtb boot.scr" --rootfs-file rootfs.tar.gz
```
