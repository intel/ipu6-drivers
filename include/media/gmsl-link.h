/*
 * Copyright (c) 2018-2020, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 * <b>GMSL API: Gigabit Multimedia Serial Link protocol</b>
 *
 * @b Description: Defines elements used to set up and use a GMSL link.
 */

#ifndef __GMSL_LINK_H__
/**
 * \defgroup GMSL Gigabit Multimedia Serial Link (GMSL)
 *
 * Defines the interface used to control the MAX9295 serializer and
 * MAX9296 deserializer modules.
 *
 * @ingroup serdes_group
 * @{
 */

#define __GMSL_LINK_H__

#define GMSL_CSI_1X4_MODE 0x1
#define GMSL_CSI_2X4_MODE 0x2
#define GMSL_CSI_2X2_MODE 0x3
#define GMSL_CSI_4X2_MODE 0x4

#define GMSL_CSI_PORT_A 0x0
#define GMSL_CSI_PORT_B 0x1
#define GMSL_CSI_PORT_C 0x2
#define GMSL_CSI_PORT_D 0x3
#define GMSL_CSI_PORT_E 0x4
#define GMSL_CSI_PORT_F 0x5

#define GMSL_SERDES_CSI_LINK_A 0x1
#define GMSL_SERDES_CSI_LINK_B 0x2

/* Didn't find kernel defintions, for now adding here */
#define GMSL_CSI_DT_RAW_12 0x2C
#define GMSL_CSI_DT_UED_U1 0x30
#define GMSL_CSI_DT_EMBED 0x12
#define GMSL_CSI_DT_YUV422_8 0x1E
#define GMSL_CSI_DT_RGB_888 0x24
#define GMSL_CSI_DT_RAW_8 0x2A

#define GMSL_ST_ID_UNUSED 0xFF

/**
 * Maximum number of data streams (\ref gmsl_stream elements) in a GMSL link
 * (\ref gmsl_link_ctx).
 */
#define GMSL_DEV_MAX_NUM_DATA_STREAMS 4

/**
 * Holds information about a data stream in a GMSL link (\ref gmsl_link_ctx).
 */
struct gmsl_stream {
	__u32 st_id_sel;
	__u32 st_data_type;
	__u32 des_pipe;
};

/**
 * Holds the configuration of the GMSL links from a sensor to its serializer to
 * its deserializer.
 */
struct gmsl_link_ctx {
	__u32 st_vc;            /**< Default sensor virtual channel. */
	__u32 dst_vc;           /**< Destination virtual channel (user-defined). */
	__u32 src_csi_port;     /**< Sensor to serializer CSI port connection. */
	__u32 dst_csi_port;     /**< Deserializer to Jetson CSI port connection. */
	__u32 serdes_csi_link;  /**< GMSL link between serializer and deserializer
                                devices. */
	__u32 num_streams;      /**< Number of active streams to be mapped
                                from sensor. */
	__u32 num_csi_lanes;    /**< Sensor's CSI lane configuration. */
	__u32 csi_mode;         /**< Deserializer CSI mode. */
	__u32 ser_reg;          /**< Serializer slave address. */
	__u32 sdev_reg;         /**< Sensor proxy slave address. */
	__u32 sdev_def;         /**< Sensor default slave address. */
	bool serdev_found;      /**< Indicates whether the serializer device for
                             the specified sensor source was found. Set by
                             the serializer driver during setup; used by
                             the deserializer driver to choose certain
                             configuration settings during setup. */
	struct gmsl_stream streams[GMSL_DEV_MAX_NUM_DATA_STREAMS];
        /*< An array of information about the data streams in the link. */
	struct device *s_dev;   /**< Sensor device handle. */
};

/** @} */

#endif  /* __GMSL_LINK_H__ */
