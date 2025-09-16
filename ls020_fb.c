/*
 * LS020 Siemens S65 TFT LCD framebuffer driver for Linux
 * Based on Arduino library by Yaroslav Kashapov
 * 
 * Display specs:
 * - Resolution: 176x132 pixels
 * - Color depth: 16-bit (RGB565)
 * - Interface: SPI
 * - Pins: MOSI, SCL(CLK), CS, RST, RS
 *
 * Target: Orange Pi Zero 2W with Allwinner H618
 * Kernel: 6.12.43-current-sunxi64
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>

#define DRIVER_NAME "ls020_fb"
#define LS020_WIDTH 176
#define LS020_HEIGHT 132
#define LS020_BPP 16

/* Module parameters */
static int rotation = 0;
module_param(rotation, int, 0644);
MODULE_PARM_DESC(rotation, "Display rotation: 0=0°, 1=90°, 2=180°, 3=270° (default: 0)");

static int fps = 60;
module_param(fps, int, 0644);
MODULE_PARM_DESC(fps, "Display refresh rate in FPS (default: 60, max: 120)");

/* Display commands */
#define LS020_CMD 1
#define LS020_DATA 0

struct ls020_fb_par {
	struct spi_device *spi;
	struct gpio_desc *rst_gpio;
	struct gpio_desc *rs_gpio;
	struct fb_info *info;
	u16 *videomemory;
	u8 *spi_buffer;  /* Pre-allocated SPI buffer for fast updates */
	dma_addr_t dma_handle;  /* DMA handle for SPI buffer */
	u32 pseudo_palette[16];
	u8 orientation;
	bool invert;
	bool window_set;  /* Track if address window is already set */
	bool use_dma;     /* Whether DMA is available and working */
};

/* Display initialization sequences from Arduino code */
static const u8 init_array_0[] = {
	0xEF, 0x00, 0xEE, 0x04, 0x1B, 0x04, 0xFE, 0xFE,
	0xFE, 0xFE, 0xEF, 0x90, 0x4A, 0x04, 0x7F, 0x3F,
	0xEE, 0x04, 0x43, 0x06
};

static const u8 init_array_1[] = {
	0xEF, 0x90, 0x09, 0x83, 0x08, 0x00, 0x0B, 0xAF,
	0x0A, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00,
	0xEF, 0x00, 0xEE, 0x0C, 0xEF, 0x90, 0x00, 0x80,
	0xEF, 0xB0, 0x49, 0x02, 0xEF, 0x00, 0x7F, 0x01,
	0xE1, 0x81, 0xE2, 0x02, 0xE2, 0x76, 0xE1, 0x83,
	0x80, 0x01, 0xEF, 0x90, 0x00, 0x00
};

static int ls020_write_cmd(struct ls020_fb_par *par, u8 cmd)
{
	int ret;
	gpiod_set_value(par->rs_gpio, LS020_CMD);
	ret = spi_write(par->spi, &cmd, 1);
	/* Remove frequent debug messages for speed */
	return ret;
}

static int ls020_write_reg(struct ls020_fb_par *par, u8 reg, u8 val)
{
	int ret;
	
	gpiod_set_value(par->rs_gpio, LS020_CMD);
	ret = spi_write(par->spi, &reg, 1);
	if (ret) {
		dev_err(&par->spi->dev, "Failed to write reg 0x%02X\n", reg);
		return ret;
	}
	
	ret = spi_write(par->spi, &val, 1);
	if (ret) {
		dev_err(&par->spi->dev, "Failed to write val 0x%02X to reg 0x%02X\n", val, reg);
	} else {
		dev_dbg(&par->spi->dev, "REG: 0x%02X = 0x%02X\n", reg, val);
	}
	
	return ret;
}

static int ls020_write_data16(struct ls020_fb_par *par, u16 data)
{
	u8 buf[2];
	int ret;
	
	buf[0] = data >> 8;
	buf[1] = data & 0xFF;
	
	gpiod_set_value(par->rs_gpio, LS020_DATA);
	dev_dbg(&par->spi->dev, "RS=LOW for DATA: 0x%04X\n", data);
	ret = spi_write(par->spi, buf, 2);
	if (ret) {
		dev_err(&par->spi->dev, "Failed to write data16 0x%04X\n", data);
	}
	return ret;
}

static int ls020_reset(struct ls020_fb_par *par)
{
	dev_info(&par->spi->dev, "Resetting display...\n");
	
	/* Reset sequence from Arduino: LOW -> HIGH */
	gpiod_set_value(par->rst_gpio, 0);  /* Bring reset low */
	msleep(50);                        /* Wait 100ms */
	gpiod_set_value(par->rst_gpio, 1);  /* Bring out of reset */
	msleep(50);                        /* Wait 100ms */
	
	dev_info(&par->spi->dev, "Reset complete\n");
	return 0;
}

static int ls020_init_display(struct ls020_fb_par *par)
{
	int i, ret;
	
	dev_info(&par->spi->dev, "Initializing display...\n");

	/* Reset display */
	ret = ls020_reset(par);
	if (ret)
		return ret;
	
	/* Send initialization sequence 0 */
	dev_info(&par->spi->dev, "Sending init sequence 0\n");
	for (i = 0; i < ARRAY_SIZE(init_array_0); i++) {
		ret = ls020_write_cmd(par, init_array_0[i]);
		if (ret)
			return ret;
	}
	
	msleep(7);
	
	/* Send initialization sequence 1 */
	dev_info(&par->spi->dev, "Sending init sequence 1\n");
	for (i = 0; i < ARRAY_SIZE(init_array_1); i++) {
		ret = ls020_write_cmd(par, init_array_1[i]);
		if (ret)
			return ret;
	}
	
	dev_info(&par->spi->dev, "Display initialization complete.\n");
	return 0;
}

static int ls020_set_addr_window(struct ls020_fb_par *par, u8 x0, u8 y0, u8 x1, u8 y1)
{
	int ret;
	
	ret = ls020_write_reg(par, 0xEF, 0x90);
	if (ret)
		return ret;
	
	/* Set coordinates based on orientation */
	switch (par->orientation) {
	case 0:
		ret = ls020_write_reg(par, 0x08, y0);
		ret |= ls020_write_reg(par, 0x09, y1);
		ret |= ls020_write_reg(par, 0x0A, (LS020_WIDTH - 1) - x0);
		ret |= ls020_write_reg(par, 0x0B, (LS020_WIDTH - 1) - x1);
		ret |= ls020_write_reg(par, 0x06, y0);
		ret |= ls020_write_reg(par, 0x07, (LS020_WIDTH - 1) - x0);
		break;
	case 1:
		ret = ls020_write_reg(par, 0x08, x0);
		ret |= ls020_write_reg(par, 0x09, x1);
		ret |= ls020_write_reg(par, 0x0A, y0);
		ret |= ls020_write_reg(par, 0x0B, y1);
		ret |= ls020_write_reg(par, 0x06, x0);
		ret |= ls020_write_reg(par, 0x07, y0);
		break;
	case 2:
		ret = ls020_write_reg(par, 0x08, (LS020_HEIGHT - 1) - y0);
		ret |= ls020_write_reg(par, 0x09, (LS020_HEIGHT - 1) - y1);
		ret |= ls020_write_reg(par, 0x0A, x0);
		ret |= ls020_write_reg(par, 0x0B, x1);
		ret |= ls020_write_reg(par, 0x06, (LS020_HEIGHT - 1) - y0);
		ret |= ls020_write_reg(par, 0x07, x0);
		break;
	case 3:
		ret = ls020_write_reg(par, 0x08, (LS020_HEIGHT - 1) - x0);
		ret |= ls020_write_reg(par, 0x09, (LS020_HEIGHT - 1) - x1);
		ret |= ls020_write_reg(par, 0x0A, (LS020_WIDTH - 1) - y0);
		ret |= ls020_write_reg(par, 0x0B, (LS020_WIDTH - 1) - y1);
		ret |= ls020_write_reg(par, 0x06, (LS020_HEIGHT - 1) - x0);
		ret |= ls020_write_reg(par, 0x07, (LS020_WIDTH - 1) - y0);
		break;
	}
	
	return ret;
}

static int ls020_set_rotation(struct ls020_fb_par *par, u8 rotation)
{
	u8 val01, val05;
	int ret;
	
	par->orientation = rotation & 3;
	
	switch (par->orientation) {
	case 1:
		val01 = 0x00;
		val05 = 0x00;
		break;
	case 2:
		val01 = 0x80;
		val05 = 0x04;
		break;
	case 3:
		val01 = 0xC0;
		val05 = 0x00;
		break;
	default:
		val01 = 0x40;
		val05 = 0x04;
		break;
	}
	
	ret = ls020_write_reg(par, 0xEF, 0x90);
	ret |= ls020_write_reg(par, 0x01, val01);
	ret |= ls020_write_reg(par, 0x05, val05);
	
	return ret;
}

static void ls020_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct ls020_fb_par *par = info->par;
	u16 color = rect->color;
	u32 x, y;
	int ret;
	
	/* Reset window cache since we're changing region */
	par->window_set = false;
	
	ret = ls020_set_addr_window(par, rect->dx, rect->dy, 
				    rect->dx + rect->width - 1, 
				    rect->dy + rect->height - 1);
	if (ret)
		return;
	
	/* Optimized bulk fill for large rectangles */
	if (rect->width * rect->height > 64) {
		u8 *fill_buf;
		size_t fill_size = rect->width * rect->height * 2;
		u8 color_bytes[2] = { color >> 8, color & 0xFF };
		
		fill_buf = kmalloc(fill_size, GFP_ATOMIC);
		if (fill_buf) {
			/* Fill buffer with repeated color */
			for (u32 i = 0; i < rect->width * rect->height; i++) {
				fill_buf[i * 2] = color_bytes[0];
				fill_buf[i * 2 + 1] = color_bytes[1];
			}
			
			gpiod_set_value(par->rs_gpio, LS020_DATA);
			spi_write(par->spi, fill_buf, fill_size);
			kfree(fill_buf);
			return;
		}
	}
	
	/* Fallback to pixel-by-pixel for small rects or if buffer allocation failed */
	for (y = 0; y < rect->height; y++) {
		for (x = 0; x < rect->width; x++) {
			ls020_write_data16(par, color);
		}
	}
}

static void ls020_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	/* Software fallback */
	cfb_copyarea(info, area);
}

static void ls020_imageblit(struct fb_info *info, const struct fb_image *image)
{
	/* Software fallback */
	cfb_imageblit(info, image);
}

/* Fallback slow update function for when fast buffer allocation fails */
static int ls020_update_display_slow(struct ls020_fb_par *par)
{
	u16 *vmem = par->videomemory;
	int ret, x, y;
	
	/* Set address window for full screen */
	ret = ls020_set_addr_window(par, 0, 0, LS020_WIDTH - 1, LS020_HEIGHT - 1);
	if (ret)
		return ret;
	
	/* Send pixels one by one (slow) */
	for (y = 0; y < LS020_HEIGHT; y++) {
		for (x = 0; x < LS020_WIDTH; x++) {
			ret = ls020_write_data16(par, vmem[y * LS020_WIDTH + x]);
			if (ret)
				return ret;
		}
	}
	
	return 0;
}

/* DMA-optimized display update function */
static int ls020_update_display_dma(struct ls020_fb_par *par)
{
	u16 *vmem = par->videomemory;
	u8 *data_buf = par->spi_buffer;
	struct spi_transfer t;
	struct spi_message m;
	int ret, i;
	
	/* Set address window only if not already set (optimization) */
	if (!par->window_set) {
		/* Optimized command sequence: combine all setup commands */
		static const u8 setup_cmds[] = {
			0xEF, 0x90,    /* Enable extended commands */
			0x08, 0x00,    /* y0 = 0 */
			0x09, 0x83,    /* y1 = 131 (LS020_HEIGHT - 1) */
			0x0A, 0xAF,    /* x0 = 175 (LS020_WIDTH - 1) */
			0x0B, 0x00,    /* x1 = 0 */
			0x06, 0x00,    /* cursor y */
			0x07, 0xAF     /* cursor x = 175 */
		};
		
		gpiod_set_value(par->rs_gpio, LS020_CMD);
		ret = spi_write(par->spi, setup_cmds, sizeof(setup_cmds));
		if (ret)
			return ret;
		par->window_set = true;
	}
	
	/* Convert framebuffer to big-endian format optimized */
	for (i = 0; i < LS020_WIDTH * LS020_HEIGHT; i++) {
		u16 pixel = vmem[i];
		data_buf[i << 1] = pixel >> 8;
		data_buf[(i << 1) + 1] = pixel & 0xFF;
	}
	
	/* Prepare DMA transfer */
	memset(&t, 0, sizeof(t));
	t.tx_buf = data_buf;
	t.len = LS020_WIDTH * LS020_HEIGHT * 2;
	t.tx_dma = par->dma_handle;
	
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	
	/* Set RS to DATA mode and perform DMA transfer */
	gpiod_set_value(par->rs_gpio, LS020_DATA);
	ret = spi_sync(par->spi, &m);
	
	return ret;
}

static int ls020_update_display(struct ls020_fb_par *par)
{
	u16 *vmem = par->videomemory;
	u8 *data_buf = par->spi_buffer;
	int ret, i;
	size_t buf_size = LS020_WIDTH * LS020_HEIGHT * 2;
	
	/* Use DMA if available for better performance */
	if (par->use_dma && par->spi_buffer) {
		return ls020_update_display_dma(par);
	}
	
	/* Use pre-allocated buffer if available, otherwise allocate temporarily */
	if (!data_buf) {
		data_buf = kmalloc(buf_size, GFP_ATOMIC);
		if (!data_buf) {
			/* Fallback to slow pixel-by-pixel update */
			dev_warn(&par->spi->dev, "Buffer allocation failed, using slow mode\n");
			return ls020_update_display_slow(par);
		}
	}
	
	/* Set address window only if not already set (optimization) */
	if (!par->window_set) {
		/* Optimized command sequence: combine all setup commands */
		static const u8 setup_cmds[] = {
			0xEF, 0x90,    /* Enable extended commands */
			0x08, 0x00,    /* y0 = 0 */
			0x09, 0x83,    /* y1 = 131 (LS020_HEIGHT - 1) */
			0x0A, 0xAF,    /* x0 = 175 (LS020_WIDTH - 1) */
			0x0B, 0x00,    /* x1 = 0 */
			0x06, 0x00,    /* cursor y */
			0x07, 0xAF     /* cursor x = 175 */
		};
		
		/* Send all setup commands at once */
		gpiod_set_value(par->rs_gpio, LS020_CMD);
		ret = spi_write(par->spi, setup_cmds, sizeof(setup_cmds));
		if (ret) {
			if (!par->spi_buffer)
				kfree(data_buf);
			return ret;
		}
		par->window_set = true;
	}
	
	/* Convert framebuffer to big-endian format in one optimized pass */
	for (i = 0; i < LS020_WIDTH * LS020_HEIGHT; i++) {
		u16 pixel = vmem[i];
		data_buf[i << 1] = pixel >> 8;        /* Bit shift instead of multiply */
		data_buf[(i << 1) + 1] = pixel & 0xFF;
	}
	
	/* Set RS to DATA mode and send all pixel data at once */
	gpiod_set_value(par->rs_gpio, LS020_DATA);
	ret = spi_write(par->spi, data_buf, buf_size);
	
	/* Free temporary buffer if we allocated it */
	if (!par->spi_buffer)
		kfree(data_buf);
		
	return ret;
}

static ssize_t ls020_write(struct fb_info *info, const char __user *buf, 
			   size_t count, loff_t *ppos)
{
	struct ls020_fb_par *par = info->par;
	ssize_t res;
	
	res = fb_sys_write(info, buf, count, ppos);
	ls020_update_display(par);
	
	return res;
}

static void ls020_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
	struct ls020_fb_par *par = info->par;
	ls020_update_display(par);
}

static struct fb_deferred_io ls020_defio = {
	.delay = HZ / 40,
	.deferred_io = ls020_deferred_io,
};

static int ls020_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	return fb_deferred_io_mmap(info, vma);
}

static int ls020_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	/* Panning not supported on this small display */
	return 0;
}

static struct fb_ops ls020_fbops = {
	.owner = THIS_MODULE,
	.fb_write = ls020_write,
	.fb_fillrect = ls020_fillrect,
	.fb_copyarea = ls020_copyarea,
	.fb_imageblit = ls020_imageblit,
	.fb_mmap = ls020_fb_mmap,
	.fb_pan_display = ls020_fb_pan_display,
};

static int ls020_fb_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct fb_info *info;
	struct ls020_fb_par *par;
	int retval = 0;
	
	dev_info(dev, "LS020 framebuffer driver probing\n");
	
	info = framebuffer_alloc(sizeof(struct ls020_fb_par), dev);
	if (!info) {
		dev_err(dev, "Couldn't allocate framebuffer.\n");
		retval = -ENOMEM;
		goto fballoc_fail;
	}
	
	par = info->par;
	par->spi = spi;
	par->info = info;
	par->orientation = 0;
	par->invert = false;
	
	/* Get GPIO pins */
	par->rst_gpio = devm_gpiod_get(dev, "ls020-reset", GPIOD_OUT_LOW);
	if (IS_ERR(par->rst_gpio)) {
		dev_err(dev, "Failed to get reset GPIO\n");
		retval = PTR_ERR(par->rst_gpio);
		goto gpio_fail;
	}
	
	par->rs_gpio = devm_gpiod_get(dev, "ls020-dc", GPIOD_OUT_LOW);
	if (IS_ERR(par->rs_gpio)) {
		dev_err(dev, "Failed to get RS/DC GPIO\n");
		retval = PTR_ERR(par->rs_gpio);
		goto gpio_fail;
	}
	
	/* Allocate video memory */
	par->videomemory = vzalloc(LS020_WIDTH * LS020_HEIGHT * 2);
	if (!par->videomemory) {
		dev_err(dev, "Couldn't allocate video memory.\n");
		retval = -ENOMEM;
		goto videomem_alloc_fail;
	}
	
	/* Try to allocate DMA-coherent SPI buffer for fastest updates */
	par->spi_buffer = dma_alloc_coherent(dev, LS020_WIDTH * LS020_HEIGHT * 2,
					     &par->dma_handle, GFP_KERNEL);
	if (par->spi_buffer) {
		par->use_dma = true;
		dev_info(dev, "DMA buffer allocated for optimized performance\n");
	} else {
		/* Fallback to regular allocated buffer */
		par->spi_buffer = kmalloc(LS020_WIDTH * LS020_HEIGHT * 2, GFP_KERNEL);
		par->use_dma = false;
		if (!par->spi_buffer) {
			dev_warn(dev, "Couldn't allocate SPI buffer, will use temporary buffers\n");
		} else {
			dev_info(dev, "Regular SPI buffer allocated\n");
		}
	}
	
	/* Initialize state */
	par->window_set = false;
	
	/* Setup framebuffer */
	info->screen_base = (char __iomem *)par->videomemory;
	info->screen_size = LS020_WIDTH * LS020_HEIGHT * 2;
	info->fbops = &ls020_fbops;
	info->var.xres = LS020_WIDTH;
	info->var.yres = LS020_HEIGHT;
	info->var.xres_virtual = LS020_WIDTH;
	info->var.yres_virtual = LS020_HEIGHT;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.bits_per_pixel = LS020_BPP;
	info->var.red.offset = 11;
	info->var.red.length = 5;
	info->var.green.offset = 5;
	info->var.green.length = 6;
	info->var.blue.offset = 0;
	info->var.blue.length = 5;
	info->var.transp.offset = 0;
	info->var.transp.length = 0;
	info->fix.smem_start = (unsigned long)par->videomemory;
	info->fix.smem_len = info->screen_size;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.line_length = LS020_WIDTH * 2;
	info->fix.accel = FB_ACCEL_NONE;
	info->fix.xpanstep = 0;  /* Disable panning */
	info->fix.ypanstep = 0;  /* Disable panning */
	info->fix.ywrapstep = 0; /* Disable scrolling */
	info->pseudo_palette = par->pseudo_palette;
	info->flags = FBINFO_VIRTFB;
	
	/* Setup deferred I/O with dynamic FPS */
	if (fps < 1 || fps > 120) {
		dev_warn(dev, "Invalid FPS %d, using default 40\n", fps);
		fps = 40;
	}
	ls020_defio.delay = HZ / fps;
	info->fbdefio = &ls020_defio;
	fb_deferred_io_init(info);
	
	dev_info(dev, "Deferred I/O configured for %d FPS (delay: %ld jiffies)\n", 
		 fps, ls020_defio.delay);
	
	/* Configure SPI */
	spi->max_speed_hz = 30000000; /* 32 MHz */
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	retval = spi_setup(spi);
	if (retval < 0) {
		dev_err(dev, "SPI setup failed.\n");
		goto spi_setup_fail;
	}
	
	/* Initialize display */
	retval = ls020_init_display(par);
	if (retval < 0) {
		dev_err(dev, "Display initialization failed.\n");
		goto init_fail;
	}
	
	retval = ls020_set_rotation(par, rotation & 3);  /* Use module parameter */
	if (retval < 0) {
		dev_err(dev, "Failed to set rotation.\n");
		goto init_fail;
	}
	
	dev_info(dev, "Display rotation set to %d° (parameter: %d)\n", 
		 (rotation & 3) * 90, rotation);
	
	/* Fill framebuffer with test pattern */
	dev_info(dev, "Drawing test pattern\n");
	for (int i = 0; i < LS020_WIDTH * LS020_HEIGHT; i++) {
		if (i < (LS020_WIDTH * LS020_HEIGHT / 3))
			par->videomemory[i] = 0xF800; /* Red */
		else if (i < (2 * LS020_WIDTH * LS020_HEIGHT / 3))
			par->videomemory[i] = 0x07E0; /* Green */
		else
			par->videomemory[i] = 0x001F; /* Blue */
	}
	
	/* Update display with test pattern */
	retval = ls020_update_display(par);
	if (retval < 0) {
		dev_err(dev, "Failed to update display with test pattern.\n");
		goto init_fail;
	}
	
	/* Register framebuffer */
	retval = register_framebuffer(info);
	if (retval < 0) {
		dev_err(dev, "Framebuffer registration failed.\n");
		goto register_fail;
	}
	
	spi_set_drvdata(spi, info);
	
	dev_info(dev, "LS020 framebuffer %dx%d registered\n", 
		 LS020_WIDTH, LS020_HEIGHT);
	
	return 0;

register_fail:
init_fail:
spi_setup_fail:
	fb_deferred_io_cleanup(info);
	/* Free SPI buffer (DMA or regular) */
	if (par->spi_buffer) {
		if (par->use_dma) {
			dma_free_coherent(dev, LS020_WIDTH * LS020_HEIGHT * 2,
					  par->spi_buffer, par->dma_handle);
		} else {
			kfree(par->spi_buffer);
		}
	}
	vfree(par->videomemory);
videomem_alloc_fail:
gpio_fail:
	framebuffer_release(info);
fballoc_fail:
	return retval;
}

static void ls020_fb_remove(struct spi_device *spi)
{
	struct fb_info *info = spi_get_drvdata(spi);
	struct ls020_fb_par *par = info->par;
	
	if (info) {
		unregister_framebuffer(info);
		fb_deferred_io_cleanup(info);
		
		/* Free SPI buffer (DMA or regular) */
		if (par->spi_buffer) {
			if (par->use_dma) {
				dma_free_coherent(&spi->dev, LS020_WIDTH * LS020_HEIGHT * 2,
						  par->spi_buffer, par->dma_handle);
			} else {
				kfree(par->spi_buffer);
			}
		}
		
		vfree(par->videomemory);
		framebuffer_release(info);
	}
}

static const struct of_device_id ls020_of_match[] = {
	{ .compatible = "siemens,ls020" },
	{},
};
MODULE_DEVICE_TABLE(of, ls020_of_match);

static const struct spi_device_id ls020_ids[] = {
	{ "ls020", 0 },
	{},
};
MODULE_DEVICE_TABLE(spi, ls020_ids);

static struct spi_driver ls020_fb_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = ls020_of_match,
	},
	.id_table = ls020_ids,
	.probe = ls020_fb_probe,
	.remove = ls020_fb_remove,
};

module_spi_driver(ls020_fb_driver);

MODULE_DESCRIPTION("LS020 Siemens S65 TFT LCD framebuffer driver");
MODULE_AUTHOR("Based on Arduino library by Yaroslav Kashapov");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:ls020");