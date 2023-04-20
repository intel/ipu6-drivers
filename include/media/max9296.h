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
 * <b>MAX9296 API: For Maxim Integrated MAX9296 deserializer</b>
 *
 * @b Description: Defines elements used to set up and use a
 *  Maxim Integrated MAX9296 deserializer.
 */

#ifndef __MAX9296_H__
#define __MAX9296_H__

#include <linux/types.h>
#include <media/gmsl-link.h>
/**
 * \defgroup max9296 MAX9296 deserializer driver
 *
 * Controls the MAX9296 deserializer module.
 *
 * @ingroup serdes_group
 * @{
 */

int max9296_get_available_pipe_id(struct device *dev, int vc_id);
int max9296_set_pipe(struct device *dev, int pipe_id, u8 data_type1,
		     u8 data_type2, u32 vc_id);
int max9296_release_pipe(struct device *dev, int pipe_id);
void max9296_reset_oneshot(struct device *dev);

/**
 * Puts a deserializer device in single exclusive link mode, so link-specific
 * I2C overrides can be performed for sensor and serializer devices.
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  s_dev           The sensor device handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_setup_link(struct device *dev, struct device *s_dev);

/**
 * @brief  Sets up a deserializer link's control pipeline.
 *
 * Puts the deserializer in dual splitter mode. You must call this function
 * during device boot, after max9296_setup_link().
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  s_dev           The sensor device handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_setup_control(struct device *dev, struct device *s_dev);

/**
 * @brief  Resets a deserializer device's link control pipeline.
 *
 * The deserializer driver internally decrements the reference count and
 * resets the deserializer device if all the source sensor devices are
 * powered off, resetting all control and streaming configuration.
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  s_dev           The sensor device handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_reset_control(struct device *dev, struct device *s_dev);

/**
 * @brief  Registers a source sensor device with a deserializer device.
 *
 * The deserializer driver internally checks all perquisites and compatibility
 * factors. If it finds that the registration request is valid,
 * it stores the source's @ref gmsl_link_ctx context handle in the source list
 * maintained by the deserializer driver instance.
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  g_ctx           A @c gmsl_link_ctx structure handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_sdev_register(struct device *dev, struct gmsl_link_ctx *g_ctx);

/**
 * Unregisters a source sensor device from its deserializer device.
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  s_dev           The sensor device handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_sdev_unregister(struct device *dev, struct device *s_dev);

/**
 * Performs internal pipeline configuration for a link in context to set up
 * streaming, and puts the deserializer link in ready-to-stream state.
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  s_dev           The sensor device handle.
 *
 * @return  0 or success, or -1 otherwise.
 */
int max9296_setup_streaming(struct device *dev, struct device *s_dev);

/**
 * @brief Enables streaming.
 *
 * This function is to be called by the sensor client driver.
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  s_dev           The sensor device handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_start_streaming(struct device *dev, struct device *s_dev);

/**
 * @brief Disables streaming.
 *
 * This function is to be called by the sensor client driver.
 *
 * @note  Both @c max9296_start_streaming and @c max9296_stop_streaming
 * are mainly added to enable and disable sensor streaming on the fly
 * while other sensors are active.
 *
 * @param [in]  dev             The deserializer device handle.
 * @param [in]  s_dev           The sensor device handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_stop_streaming(struct device *dev, struct device *s_dev);

/**
 * @brief  Powers on the max9296 deserializer module.
 *
 * Asserts shared reset GPIO and powers on the regulator;
 * maintains the reference count internally for source devices.
 *
 * @param [in]  dev             The deserializer device handle.
 *
 * @return  0 for success, or -1 otherwise.
 */
int max9296_power_on(struct device *dev);

/**
 * @brief  Powers off the max9296 deserializer module.
 *
 * Deasserts the shared reset GPIO and powers off the regulator based on
 * the reference count.
 *
 * @param [in]  dev             The deserializer device handle.
 */
void max9296_power_off(struct device *dev);

int max9296_init_settings(struct device *dev);
/** @} */

#endif  /* __MAX9296_H__ */
