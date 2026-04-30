#!/usr/bin/env bash

cp /mnt/sn850x/ECE554_Capstone/Vivado_projects/projbuild/*.xsa petalinux-build/projbuild/
cp /mnt/sn850x/ECE554_Capstone/Vivado_projects/projbuild/projbuild.runs/impl_1/*.bit petalinux-build/projbuild/
cp /mnt/sn850x/ECE554_Capstone/Vivado_projects/projbuild/projbuild.runs/impl_1/*.bit petalinux-build/projbuild/projbuild/images/linux/

podman kill -a && podman rm -a && podman compose up -d --build

podman unshare chown -R 1000:0 petalinux* ../space_invaders
podman unshare chmod -R g+wrX petalinux* ../space_invaders
podman unshare chmod g+s petalinux* ../space_invaders

