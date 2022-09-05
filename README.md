# Kernel driver build guide

## build kernel

### internal build with kernel source tree
1. cp -rf ipu6-drivers/* vied-viedlin-intel-next/
2. cd vied-viedlin-intel-next
3. echo "" | make oldconfig
4. make -j8
5. make tarbz2-pkg

### build external with Makefile
1. cp -rf vied-viedlin-intel-next /usr/src
2. cd ipu6-drivers
3. make clean && make -j8
4. make install
5. sync & depmod -a & reboot

### build external with dkms (need install dkms package)
1. cp -rf vied-viedlin-intel-next /usr/src
2. cp -rf ipu6-drivers /usr/src/ipu6-drivers-0.1
3. dkms --kernelsourcedir=/usr/src/vied-viedlin-intel-next add -m ipu6-drivers -v 0.1
4. dkms build -m ipu6-drivers -v 0.1
5. dkms install -m ipu6-drivers -v 0.1 
6. sync & depmod -a & reboot

## use kernel 

### internal build with kernel source tree
1. cp linux-xxx.tar.gz to ADL board
2. tar -xvf linux-xxxtar.bz2
3. cp -rf boot/vmlinuxz-xxx /boot/bzImage-kernel
4. cp -rf lib/modules/5.15.10-intel-ese-standard-lts-bullpen+/ /lib/modules/
5. sync & depmod -a & reboot
