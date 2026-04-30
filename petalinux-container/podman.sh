#!/usr/bin/env bash

podman kill -a && podman rm -a && podman compose up -d --build

podman unshare chown -R 1000:0 petalinux* ../space_invaders
podman unshare chmod -R g+wrX petalinux* ../space_invaders
podman unshare chmod g+s petalinux* ../space_invaders

