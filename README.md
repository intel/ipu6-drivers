# Kernel driver build guide

## build kernel
1. cp -rf include/ drivers/ vied-viedlin-intel-next/
2. cd vied-viedlin-intel-next
3. echo "" | make oldconfig
4. make -j8
5. make tarbz2-pkg

## use kernel
1. cp linux-xxx.tar.gz to ADL board
2. tar -xvf linux-xxxtar.bz2
3. cp -rf boot/vmlinuxz-xxx /boot/bzImage-kernel
4. cp -rf lib/modules/5.15.10-intel-ese-standard-lts-bullpen+/ /lib/modules/
5. sync & depmod -a & reboot
