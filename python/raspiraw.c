#define _GNU_SOURCE
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#define I2C_SLAVE_FORCE 0x0706
#include "interface/vcos/vcos.h"
#include "bcm_host.h"

#include "scope.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_connection.h"

#include <sys/ioctl.h>

#include "raw_header.h"

#define DEFAULT_I2C_DEVICE 0

#define I2C_DEVICE_NAME_LEN 13	// "/dev/i2c-XXX"+NULL
static char i2c_device_name[I2C_DEVICE_NAME_LEN];

enum bayer_order {
	//Carefully ordered so that an hflip is ^1,
	//and a vflip is ^2.
	BAYER_ORDER_BGGR,
	BAYER_ORDER_GBRG,
	BAYER_ORDER_GRBG,
	BAYER_ORDER_RGGB
};

struct sensor_regs {
	uint16_t reg;
	uint16_t data;
};

struct mode_def
{
	struct sensor_regs *regs;
	int num_regs;
	int width;
	int height;
	MMAL_FOURCC_T encoding;
	enum bayer_order order;
	int native_bit_depth;
	uint8_t image_id;
	uint8_t data_lanes;
	int min_vts;
	int line_time_ns;
	uint32_t timing[5];
	uint32_t term[2];
	int black_level;
};

struct sensor_def
{
	char *name;
	struct mode_def *modes;
	int num_modes;
	struct sensor_regs *stop;
	int num_stop_regs;

	uint8_t i2c_addr;		// Device I2C slave address
	int i2c_addressing;		// Length of register address values
	int i2c_data_size;		// Length of register data to write

	//  Detecting the device
	int i2c_ident_length;		// Length of I2C ID register
	uint16_t i2c_ident_reg;		// ID register address
	uint16_t i2c_ident_value;	// ID register value

	// Flip configuration
	uint16_t vflip_reg;		// Register for VFlip
	int vflip_reg_bit;		// Bit in that register for VFlip
	uint16_t hflip_reg;		// Register for HFlip
	int hflip_reg_bit;		// Bit in that register for HFlip
	int flips_dont_change_bayer_order;	// Some sensors do not change the
						// Bayer order by adjusting X/Y starts
						// to compensate.

	uint16_t exposure_reg;
	int exposure_reg_num_bits;

	uint16_t vts_reg;
	int vts_reg_num_bits;

	uint16_t gain_reg;
	int gain_reg_num_bits;
};


#define NUM_ELEMENTS(a)  (sizeof(a) / sizeof(a[0]))

#include "ov5647_modes.h"

const struct sensor_def *sensors[] = {
	&ov5647,
	NULL
};

typedef struct pts_node {
	int	idx;
	int64_t  pts;
	struct pts_node *nxt;
} *PTS_NODE_T;

typedef struct {
	int mode;
	int hflip;
	int vflip;
	int exposure;
	int gain;
	char *output;
	int capture;
	int write_header;
	int timeout;
	int saverate;
	int bit_depth;
	int camera_num;
	int exposure_us;
	int i2c_bus;
	double awb_gains_r;
	double awb_gains_b;
	char *regs;
	int hinc;
	int vinc;
	double fps;
	int width;
	int height;
	int left;
	int top;
	char *write_header0;
	char *write_headerg;
	char *write_timestamps;
	int write_empty;
        PTS_NODE_T ptsa;
        PTS_NODE_T ptso;
        int decodemetadata;
} RASPIRAW_PARAMS_T;

void update_regs(const struct sensor_def *sensor, struct mode_def *mode, int hflip, int vflip, int exposure, int gain);

static int i2c_rd(int fd, uint8_t i2c_addr, uint16_t reg, uint8_t *values, uint32_t n, const struct sensor_def *sensor)
{
	int err;
	uint8_t buf[2] = { reg >> 8, reg & 0xff };
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg msgs[2] = {
		{
			 .addr = i2c_addr,
			 .flags = 0,
			 .len = 2,
			 .buf = buf,
		},
		{
			.addr = i2c_addr,
			.flags = I2C_M_RD,
			.len = n,
			.buf = values,
		},
	};

	if (sensor->i2c_addressing == 1)
	{
		msgs[0].len = 1;
	}
	msgset.msgs = msgs;
	msgset.nmsgs = 2;

	err = ioctl(fd, I2C_RDWR, &msgset);
	//vcos_log_error("Read i2c addr %02X, reg %04X (len %d), value %02X, err %d", i2c_addr, msgs[0].buf[0], msgs[0].len, values[0], err);
	if (err != (int)msgset.nmsgs)
		return -1;

	return 0;
}

const struct sensor_def * probe_sensor(void)
{
	int fd;
	const struct sensor_def **sensor_list = &sensors[0];
	const struct sensor_def *sensor = NULL;

	fd = open(i2c_device_name, O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return NULL;
	}

	while(*sensor_list != NULL)
	{
		uint16_t reg = 0;
		sensor = *sensor_list;
		vcos_log_error("Probing sensor %s on addr %02X", sensor->name, sensor->i2c_addr);
		if (sensor->i2c_ident_length <= 2)
		{
			if (!i2c_rd(fd, sensor->i2c_addr, sensor->i2c_ident_reg, (uint8_t*)&reg, sensor->i2c_ident_length, sensor))
			{
				if (reg == sensor->i2c_ident_value)
				{
					vcos_log_error("Found sensor %s at address %02X", sensor->name, sensor->i2c_addr);
					break;
				}
			}
		}
		sensor_list++;
		sensor = NULL;
	}
	return sensor;
}

void send_regs(int fd, const struct sensor_def *sensor, const struct sensor_regs *regs, int num_regs)
{
	int i;
	for (i=0; i<num_regs; i++)
	{
		if (regs[i].reg == 0xFFFF)
		{
			if (ioctl(fd, I2C_SLAVE_FORCE, regs[i].data) < 0)
			{
				vcos_log_error("Failed to set I2C address to %02X", regs[i].data);
			}
		}
		else if (regs[i].reg == 0xFFFE)
		{
			vcos_sleep(regs[i].data);
		}
		else
		{
			if (sensor->i2c_addressing == 1)
			{
				unsigned char msg[3] = {regs[i].reg, regs[i].data & 0xFF };
				int len = 2;

				if (sensor->i2c_data_size == 2)
				{
					msg[1] = (regs[i].data>>8) & 0xFF;
					msg[2] = regs[i].data & 0xFF;
					len = 3;
				}
					printf("writing register index %d (%02X val %02X)", i, regs[i].reg, regs[i].data);
				if (write(fd, msg, len) != len)
				{
					vcos_log_error("Failed to write register index %d (%02X val %02X)", i, regs[i].reg, regs[i].data);
				}
			}
			else
			{
				unsigned char msg[4] = {regs[i].reg>>8, regs[i].reg, regs[i].data};
				int len = 3;

				if (sensor->i2c_data_size == 2)
				{
					msg[2] = regs[i].data >> 8;
					msg[3] = regs[i].data;
					len = 4;
				}
				if (write(fd, msg, len) != len)
				{
					vcos_log_error("Failed to write register index %d", i);
				}
			}
		}
	}
}

void start_camera_streaming(const struct sensor_def *sensor, struct mode_def *mode)
{
	int fd;
	fd = open(i2c_device_name, O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return;
	}
	if (ioctl(fd, I2C_SLAVE_FORCE, sensor->i2c_addr) < 0)
	{
		vcos_log_error("Failed to set I2C address");
		return;
	}
	send_regs(fd, sensor, mode->regs, mode->num_regs);
	close(fd);
	vcos_log_error("Now streaming...");
}

void stop_camera_streaming(const struct sensor_def *sensor)
{
	int fd;
	fd = open(i2c_device_name, O_RDWR);
	if (!fd) {
		vcos_log_error("Couldn't open I2C device");
		return;
	}
	if (ioctl(fd, I2C_SLAVE_FORCE, sensor->i2c_addr) < 0) {
		vcos_log_error("Failed to set I2C address");
		return;
	}
	send_regs(fd, sensor, sensor->stop, sensor->num_stop_regs);
	close(fd);
}

int encoding_to_bpp(uint32_t encoding)
{
       switch(encoding)
       {
       case    MMAL_ENCODING_BAYER_SBGGR10P:
       case    MMAL_ENCODING_BAYER_SGBRG10P:
       case    MMAL_ENCODING_BAYER_SGRBG10P:
       case    MMAL_ENCODING_BAYER_SRGGB10P:
               return 10;
       case    MMAL_ENCODING_BAYER_SBGGR12P:
       case    MMAL_ENCODING_BAYER_SGBRG12P:
       case    MMAL_ENCODING_BAYER_SGRBG12P:
       case    MMAL_ENCODING_BAYER_SRGGB12P:
               return 12;
       default:
               return 8;
       };

}

MMAL_BUFFER_HEADER_T shared_buf;
bool_t got_frame = 0, aborted = 0;
VCOS_MUTEX_T mutex;

void signal_abort(int i) {
	aborted = 1;
	write(2, "aborting\n", 9);
}

int running = 0;
static void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	static int count = 0;
	//	if (++count < 20)
	//vcos_log_error("Buffer %p returned, filled %d, timestamp %llu, flags %04X", buffer, buffer->length, buffer->pts, buffer->flags);
	if (running)
	{
		//RASPIRAW_PARAMS_T *cfg = (RASPIRAW_PARAMS_T *)port->userdata;

		if (!(buffer->flags&MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO))
		{
			//printf("cb_thread: %p -> %p %d\n", buffer, buffer->data, buffer->length);
			while (vcos_mutex_lock(&mutex) != VCOS_SUCCESS);
			shared_buf = *buffer;
			//printf ("shared_buf: %p -> %p %d\n", &shared_buf, shared_buf.data, shared_buf.length);
			got_frame = 1;
			vcos_mutex_unlock(&mutex);
			//glutMainLoopEvent();
			//fwrite(buffer->data, buffer->length, 1, file);
		}

		buffer->length = 0;
		mmal_port_send_buffer(port, buffer);
	}
	else
		mmal_buffer_header_release(buffer);
}

uint32_t order_and_bit_depth_to_encoding(enum bayer_order order, int bit_depth)
{
	//BAYER_ORDER_BGGR,
	//BAYER_ORDER_GBRG,
	//BAYER_ORDER_GRBG,
	//BAYER_ORDER_RGGB
	const uint32_t depth8[] = {
		MMAL_ENCODING_BAYER_SBGGR8,
		MMAL_ENCODING_BAYER_SGBRG8,
		MMAL_ENCODING_BAYER_SGRBG8,
		MMAL_ENCODING_BAYER_SRGGB8
	};
	const uint32_t depth10[] = {
		MMAL_ENCODING_BAYER_SBGGR10P,
		MMAL_ENCODING_BAYER_SGBRG10P,
		MMAL_ENCODING_BAYER_SGRBG10P,
		MMAL_ENCODING_BAYER_SRGGB10P
	};
	const uint32_t depth12[] = {
		MMAL_ENCODING_BAYER_SBGGR12P,
		MMAL_ENCODING_BAYER_SGBRG12P,
		MMAL_ENCODING_BAYER_SGRBG12P,
		MMAL_ENCODING_BAYER_SRGGB12P,
	};
	const uint32_t depth16[] = {
		MMAL_ENCODING_BAYER_SBGGR16,
		MMAL_ENCODING_BAYER_SGBRG16,
		MMAL_ENCODING_BAYER_SGRBG16,
		MMAL_ENCODING_BAYER_SRGGB16,
	};
	if (order < 0 || order > 3)
	{
		vcos_log_error("order out of range - %d", order);
		return 0;
	}

	switch(bit_depth)
	{
		case 8:
			return depth8[order];
		case 10:
			return depth10[order];
		case 12:
			return depth12[order];
		case 16:
			return depth16[order];
	}
	vcos_log_error("%d not one of the handled bit depths", bit_depth);
	return 0;
}

//The process first loads the cleaned up dump of the registers
//than updates the known registers to the proper values
//based on: http://www.seeedstudio.com/wiki/images/3/3c/Ov5647_full.pdf
enum operation {
       EQUAL,  //Set bit to value
       SET,    //Set bit
       CLEAR,  //Clear bit
       XOR     //Xor bit
};

enum teardown { PORT=0, POOL, C1, C2 };

void modReg(struct mode_def *mode, uint16_t reg, int startBit, int endBit, int value, enum operation op);

MMAL_COMPONENT_T *rawcam=NULL, *isp=NULL, *render=NULL;
MMAL_STATUS_T status;
MMAL_PORT_T *output = NULL;
MMAL_POOL_T *pool = NULL;
MMAL_CONNECTION_T *rawcam_isp = NULL;
MMAL_CONNECTION_T *isp_render = NULL;
MMAL_PARAMETER_CAMERA_RX_CONFIG_T rx_cfg;
MMAL_PARAMETER_CAMERA_RX_TIMING_T rx_timing;
RASPIRAW_PARAMS_T cfg = { 0 };
const struct sensor_def *sensor;

static void graph_teardown(int what) {
	switch (what) {
	case PORT:
	if (cfg.capture) {
		status = mmal_port_disable(output);
		if (status != MMAL_SUCCESS) {
			vcos_log_error("Failed to disable port");
		}
	}
	/* fall-through */
	case POOL:
	if (pool)
		mmal_port_pool_destroy(output, pool);
	if (isp_render)	{
		mmal_connection_disable(isp_render);
		mmal_connection_destroy(isp_render);
	}
	if (rawcam_isp)	{
		mmal_connection_disable(rawcam_isp);
		mmal_connection_destroy(rawcam_isp);
	}
	/* fall-through */
	case C1:
	status = mmal_component_disable(render);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to disable render");
	}
	status = mmal_component_disable(isp);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to disable isp");
	}
	status = mmal_component_disable(rawcam);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to disable rawcam");
	}
	/* fall-through */
	case C2:
	if (rawcam)
		mmal_component_destroy(rawcam);
	if (isp)
		mmal_component_destroy(isp);
	if (render)
		mmal_component_destroy(render);
	}
}

int graph_start(void) {
	uint32_t encoding;
	struct mode_def *sensor_mode = NULL;

	//Initialise any non-zero config values.
	cfg.exposure = -1;
	cfg.gain = -1;
	cfg.timeout = 5000;
	cfg.saverate = 20;
	cfg.bit_depth = -1;
	cfg.camera_num = -1;
	cfg.exposure_us = -1;
	cfg.i2c_bus = DEFAULT_I2C_DEVICE;
	cfg.hinc = -1;
	cfg.vinc = -1;
	cfg.fps = -1;
	cfg.width = -1;
	cfg.height = -1;
	cfg.left = -1;
	cfg.top = -1;

	cfg.mode = 0;
	cfg.width = 2048;
	cfg.height = 256;
	cfg.fps = 60;

	/* cfg.mode = 6; */
	/* cfg.width = 320; */
	/* cfg.height = 160; */
	/* cfg.fps = 60; */
	cfg.capture = 1;
	
	initgraph();
	bcm_host_init();
	vcos_log_register("RaspiRaw", VCOS_LOG_CATEGORY);

	snprintf(i2c_device_name, sizeof(i2c_device_name), "/dev/i2c-%d", cfg.i2c_bus);
	printf("Using i2C device %s\n", i2c_device_name);

	sensor = &ov5647;//probe_sensor();
	if (!sensor) {
		vcos_log_error("No sensor found. Aborting");
		return -1;
	}

	if (cfg.mode >= 0 && cfg.mode < sensor->num_modes) {
		sensor_mode = &sensor->modes[cfg.mode];
	}

	if (!sensor_mode) {
		vcos_log_error("Invalid mode %d - aborting", cfg.mode);
		return -2;
	}


	if (cfg.regs) {
		int r,b;
		char *p,*q;

		p=strtok(cfg.regs, ";");
		while (p) {
			vcos_assert(strlen(p)>6);
			vcos_assert(p[4]==',');
			vcos_assert(strlen(p)%2);
			p[4]='\0'; q=p+5;
			sscanf(p,"%4x",&r);
			while(*q) {
				vcos_assert(isxdigit(q[0]));
				vcos_assert(isxdigit(q[1]));

				sscanf(q,"%2x",&b);
				vcos_log_error("%04x: %02x",r,b);

				modReg(sensor_mode, r, 0, 7, b, EQUAL);

				++r;
				q+=2;
			}
			p=strtok(NULL,";");
		}
	}

	if (cfg.hinc >= 0)
	{
		// TODO: handle modes different to ov5647 as well
		modReg(sensor_mode, 0x3814, 0, 7, cfg.hinc, EQUAL);
	}

	if (cfg.vinc >= 0)
	{
		// TODO: handle modes different to ov5647 as well
		modReg(sensor_mode, 0x3815, 0, 7, cfg.vinc, EQUAL);
	}

	if (cfg.fps > 0)
	{
		int n = 1000000000 / (sensor_mode->line_time_ns * cfg.fps);
		modReg(sensor_mode, sensor->vts_reg+0, 0, 7, n>>8, EQUAL);
		modReg(sensor_mode, sensor->vts_reg+1, 0, 7, n&0xFF, EQUAL);
	}

	if (cfg.width > 0)
	{
		sensor_mode->width = cfg.width;
		// TODO: handle modes different to ov5647 as well
		modReg(sensor_mode, 0x3808, 0, 3, cfg.width >>8, EQUAL);
		modReg(sensor_mode, 0x3809, 0, 7, cfg.width &0xFF, EQUAL);
	}

	if (cfg.height > 0)
	{
		sensor_mode->height = cfg.height;
		// TODO: handle modes different to ov5647 as well
		modReg(sensor_mode, 0x380A, 0, 3, cfg.height >>8, EQUAL);
		modReg(sensor_mode, 0x380B, 0, 7, cfg.height &0xFF, EQUAL);
	}

	if (cfg.left > 0)
	{
		// TODO: handle modes different to ov5647 as well
		int val = cfg.left * (cfg.mode < 2 ? 1 : 1 << (cfg.mode / 2 - 1));
		modReg(sensor_mode, 0x3800, 0, 3, val >>8, EQUAL);
		modReg(sensor_mode, 0x3801, 0, 7, val &0xFF, EQUAL);
	}

	if (cfg.top > 0) {
		// TODO: handle modes different to ov5647 as well
		int val = cfg.top * (cfg.mode < 2 ? 1 : 1 << (cfg.mode / 2 - 1));
		modReg(sensor_mode, 0x3802, 0, 3, val >>8, EQUAL);
		modReg(sensor_mode, 0x3803, 0, 7, val &0xFF, EQUAL);
	}

	if (cfg.bit_depth == -1) {
		cfg.bit_depth = sensor_mode->native_bit_depth;
	}

	if (sensor_mode->encoding == 0)
		encoding = order_and_bit_depth_to_encoding(sensor_mode->order, cfg.bit_depth);
	else
		encoding = sensor_mode->encoding;
	if (!encoding) {
		vcos_log_error("Failed to map bitdepth %d and order %d into encoding\n", cfg.bit_depth, sensor_mode->order);
		return -3;
	}
	vcos_log_error("Encoding %08X", encoding);

	unsigned int i;

	bcm_host_init();
	vcos_log_register("RaspiRaw", VCOS_LOG_CATEGORY);

	status = mmal_component_create("vc.ril.rawcam", &rawcam);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to create rawcam");
		return -1;
	}

	status = mmal_component_create("vc.ril.isp", &isp);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to create isp");
		graph_teardown(C2);
		return -1;
	}

	status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &render);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to create render");
		graph_teardown(C2);
		return -1;
	}

	output = rawcam->output[0];

	rx_cfg.hdr.id = MMAL_PARAMETER_CAMERA_RX_CONFIG;
	rx_cfg.hdr.size = sizeof(rx_cfg);
	status = mmal_port_parameter_get(output, &rx_cfg.hdr);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to get cfg");
		graph_teardown(C2);
		return -1;
	}
	if (sensor_mode->encoding || cfg.bit_depth == sensor_mode->native_bit_depth) {
		rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
		rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_NONE;
	} else {
		switch(sensor_mode->native_bit_depth) {
			case 8:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_8;
				break;
			case 10:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_10;
				break;
			case 12:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_12;
				break;
			case 14:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_16;
				break;
			case 16:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_16;
				break;
			default:
				vcos_log_error("Unknown native bit depth %d", sensor_mode->native_bit_depth);
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
				break;
		}
		switch(cfg.bit_depth)
		{
			case 8:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_8;
				break;
			case 10:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_RAW10;
				break;
			case 12:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_RAW12;
				break;
			case 14:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_14;
				break;
			case 16:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_16;
				break;
			default:
				vcos_log_error("Unknown output bit depth %d", cfg.bit_depth);
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
				break;
		}
	}
	vcos_log_error("Set pack to %d, unpack to %d", rx_cfg.unpack, rx_cfg.pack);
	if (sensor_mode->data_lanes)
		rx_cfg.data_lanes = sensor_mode->data_lanes;
	if (sensor_mode->image_id)
		rx_cfg.image_id = sensor_mode->image_id;
	status = mmal_port_parameter_set(output, &rx_cfg.hdr);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to set cfg");
		graph_teardown(C2);
		return -1;
	}

	rx_timing.hdr.id = MMAL_PARAMETER_CAMERA_RX_TIMING;
	rx_timing.hdr.size = sizeof(rx_timing);
	status = mmal_port_parameter_get(output, &rx_timing.hdr);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to get timing");
		graph_teardown(C2);
		return -1;
	}
	if (sensor_mode->timing[0])
		rx_timing.timing1 = sensor_mode->timing[0];
	if (sensor_mode->timing[1])
		rx_timing.timing2 = sensor_mode->timing[1];
	if (sensor_mode->timing[2])
		rx_timing.timing3 = sensor_mode->timing[2];
	if (sensor_mode->timing[3])
		rx_timing.timing4 = sensor_mode->timing[3];
	if (sensor_mode->timing[4])
		rx_timing.timing5 = sensor_mode->timing[4];
	if (sensor_mode->term[0])
		rx_timing.term1 = sensor_mode->term[0];
	if (sensor_mode->term[1])
		rx_timing.term2 = sensor_mode->term[1];
	vcos_log_error("Timing %u/%u, %u/%u/%u, %u/%u",
		rx_timing.timing1, rx_timing.timing2,
		rx_timing.timing3, rx_timing.timing4, rx_timing.timing5,
		rx_timing.term1,  rx_timing.term2);
	status = mmal_port_parameter_set(output, &rx_timing.hdr);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to set timing");
		graph_teardown(C2);
		return -1;
	}

	if (cfg.camera_num != -1) {
		vcos_log_error("Set camera_num to %d", cfg.camera_num);
		status = mmal_port_parameter_set_int32(output, MMAL_PARAMETER_CAMERA_NUM, cfg.camera_num);
		if (status != MMAL_SUCCESS) {
			vcos_log_error("Failed to set camera_num");
			graph_teardown(C2);
			return -1;
		}
	}

	status = mmal_component_enable(rawcam);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to enable rawcam");
		graph_teardown(C2);
		return -1;
	}
	status = mmal_component_enable(isp);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to enable isp");
		graph_teardown(C2);
		return -1;
	}
	status = mmal_component_enable(render);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to enable render");
		graph_teardown(C2);
		return -1;
	}

	output->format->es->video.crop.width = sensor_mode->width;
	output->format->es->video.crop.height = sensor_mode->height;
	output->format->es->video.width = VCOS_ALIGN_UP(sensor_mode->width, 16);
	output->format->es->video.height = VCOS_ALIGN_UP(sensor_mode->height, 16);
	output->format->encoding = encoding;

	status = mmal_port_format_commit(output);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed port_format_commit");
		graph_teardown(C1);
		return -1;
	}

	output->buffer_size = output->buffer_size_recommended;
	output->buffer_num = output->buffer_num_recommended;

	status = mmal_port_parameter_set_boolean(output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
	if (status != MMAL_SUCCESS) {
		vcos_log_error("Failed to set zero copy");
		graph_teardown(C1);
		return -1;
	}

	vcos_log_error("Create pool of %d buffers of size %d", output->buffer_num, output->buffer_size);
	pool = mmal_port_pool_create(output, output->buffer_num, output->buffer_size);
	if (!pool)
	{
		vcos_log_error("Failed to create pool");
		graph_teardown(C1);
		return -1;
	}

	output->userdata = (struct MMAL_PORT_USERDATA_T *)&cfg;
	status = mmal_port_enable(output, callback);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable port");
		graph_teardown(POOL);
		return -1;
	}
	running = 1;
	for(i = 0; i<output->buffer_num; i++) {
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);

		if (!buffer) {
			vcos_log_error("Where'd my buffer go?!");
			graph_teardown(PORT);
			return -1;
		}
		status = mmal_port_send_buffer(output, buffer);
		if (status != MMAL_SUCCESS) {
			vcos_log_error("mmal_port_send_buffer failed on buffer %p, status %d", buffer, status);
			graph_teardown(PORT);
			return -1;
		}
		vcos_log_error("Sent buffer %p", buffer);
	}

	signal(SIGINT, signal_abort);
	signal(SIGQUIT, signal_abort);
	//signal(SIGSEGV, signal_abort);

	assert(vcos_mutex_create(&mutex, "?") == VCOS_SUCCESS);

	start_camera_streaming(sensor, sensor_mode);

	return 0;
}

int graph_poll(void) {
	if (aborted) {
		return 0;
	}

	while (vcos_mutex_lock(&mutex) != VCOS_SUCCESS);
	if (got_frame) {
		graph_set_buffer(&shared_buf);
		got_frame = 0;
		vcos_mutex_unlock(&mutex);
		//printf("got frame\n");
		call_callback();
		//graph_display();
	} else {
		vcos_mutex_unlock(&mutex);
	}
	//usleep (1000000.0/cfg.fps);
	//usleep (1000);
	return 1;
}

void graph_stop (void) {
	running = 0;
	stop_camera_streaming(sensor);
	graph_teardown(PORT);
}

void modRegBit(struct mode_def *mode, uint16_t reg, int bit, int value, enum operation op)
{
	int i = 0;
	uint16_t val;
	while(i < mode->num_regs && mode->regs[i].reg != reg) i++;
	if (i == mode->num_regs) {
		vcos_log_error("qeg: %04X not found!\n", reg);
		//return;
	}
	val = mode->regs[i].data;

	switch(op)
	{
		case EQUAL:
			val = (val | (1 << bit)) & (~( (1 << bit) ^ (value << bit) ));
			break;
		case SET:
			val = val | (1 << bit);
			break;
		case CLEAR:
			val = val & ~(1 << bit);
			break;
		case XOR:
			val = val ^ (value << bit);
			break;
	}
	mode->regs[i].data = val;
}

void modReg(struct mode_def *mode, uint16_t reg, int startBit, int endBit, int value, enum operation op)
{
	int i;
	for(i = startBit; i <= endBit; i++) {
		modRegBit(mode, reg, i, value >> i & 1, op);
	}
}
