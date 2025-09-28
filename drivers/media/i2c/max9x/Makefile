ifneq ($(filter y m, $(CONFIG_VIDEO_INTEL_IPU7)),)
ccflags-y += -I$(src)/../../pci/intel/ipu7/
endif

ifneq ($(filter y m, $(CONFIG_VIDEO_INTEL_IPU6)),)
ccflags-y += -I$(src)/../../pci/intel/ipu6/
endif

obj-m 		+= max9x.o
max9x-y += serdes.o max9296.o max96724.o max9295.o max96717.o