#include<linux/module.h>
#include<linux/spi/spi.h>
#include<linux/gpio/consumer.h>
#include<linux/of.h>
#include<linux/delay.h>


struct ssd1306 {
    struct spi_device *spi;
    struct gpio_desc *dc;
    struct gpio_desc *reset;
};


static int spi_write_checked(struct spi_device *spi, const void *buf, size_t len)
{
    int ret;
    
    ret = spi_write(spi, buf, len);
    if (ret < 0)
        dev_err(&spi->dev, "spi_write failed: %d\n", ret);
    return ret;
}


static int ssd1306_send_cmd(struct ssd1306 *oled, uint8_t cmd)
{
    int ret;
    
    gpiod_set_value(oled->dc, 0); //Command Mode : DC -> 0
    ret = spi_write_checked(oled->spi, &cmd, 1);
    return ret;
}

static int ssd1306_send_data(struct ssd1306 *oled, const uint8_t *data, size_t len)
{
    int ret;
    gpiod_set_value(oled->dc, 1); //Data Mode : DC -> 1
    ret = spi_write_checked(oled->spi, data, len);
    return ret;
}


static void ssd1306_hw_reset(struct ssd1306 *oled)
{
    dev_info(&oled->spi->dev, "hw reset: assert\n");
    gpiod_set_value(oled->reset, 1);
    msleep(50);
    gpiod_set_value(oled->reset, 0);
    msleep(10);
    dev_info(&oled->spi->dev, "hw reset: deassert\n");
}

static const char *spi_get_mode_str(u8 mode)
{
    if ((mode & (SPI_CPOL | SPI_CPHA)) == (SPI_CPOL | SPI_CPHA)) {
        return "Mode3";  // CPOL = 1, CPHA = 1
    } else if (mode & SPI_CPOL) {
        return "Mode2";  // CPOL = 1, CPHA = 0
    } else if (mode & SPI_CPHA) {
        return "Mode1";  // CPOL = 0, CPHA = 1
    } else {
        return "Mode0";  // CPOL = 0, CPHA = 0
    }
}

static void ssd1306_init(struct ssd1306 *oled){
    ssd1306_send_cmd(oled, 0xAE); // Display OFF
    ssd1306_send_cmd(oled, 0xD5); // Set display clock divide ratio/oscillator
    ssd1306_send_cmd(oled, 0x80);
    ssd1306_send_cmd(oled, 0xA8); // Set multiplex ratio
    ssd1306_send_cmd(oled, 0x3F);
    ssd1306_send_cmd(oled, 0xD3); // Set display offset
    ssd1306_send_cmd(oled, 0x00);
    ssd1306_send_cmd(oled, 0x40); // Set display start line
    ssd1306_send_cmd(oled, 0x8D); // Charge pump
    ssd1306_send_cmd(oled, 0x14);
    ssd1306_send_cmd(oled, 0x20); // Memory addressing mode
    ssd1306_send_cmd(oled, 0x00); // Horizontal addressing mode
    ssd1306_send_cmd(oled, 0xA1); // Segment remap
    ssd1306_send_cmd(oled, 0xC8); // COM scan direction
    ssd1306_send_cmd(oled, 0xDA); // COM pins hardware config
    ssd1306_send_cmd(oled, 0x12);
    ssd1306_send_cmd(oled, 0x81); // Contrast
    ssd1306_send_cmd(oled, 0x7F);
    ssd1306_send_cmd(oled, 0xA4); // Entire display ON (resume RAM content display)
    ssd1306_send_cmd(oled, 0xA6); // Normal display
    ssd1306_send_cmd(oled, 0x2E); // Deactivate scroll
    ssd1306_send_cmd(oled, 0xAF); // Display ON
}

static void ssd1306_fill(struct ssd1306 *oled, uint8_t data){
    int page, col;
    for (page = 0; page < 8; page++) {
        ssd1306_send_cmd(oled, 0xB0 + page); // Set page
        ssd1306_send_cmd(oled, 0x00);        // Lower column
        ssd1306_send_cmd(oled, 0x10);        // Higher column
        gpiod_set_value(oled->dc, 1);        // Data mode
        for (col = 0; col < 128; col++) {
            spi_write(oled->spi, &data, 1);
        }
    }
}


static int ssd1306_probe(struct spi_device *spi)
{
    struct ssd1306 *oled;
    int ret;

    oled = devm_kzalloc(&spi->dev, sizeof(*oled), GFP_KERNEL);
    if (!oled)
        return -ENOMEM;

    oled->spi = spi;

    
    dev_info(&spi->dev,
         "spi_setup OK: mode=%s, bits=%u, hz=%u\n",
         spi_get_mode_str(spi->mode),
         spi->bits_per_word,
         spi->max_speed_hz);

    oled->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_LOW);
    if (IS_ERR(oled->dc)) {
        dev_err(&spi->dev, "failed to get dc gpio: %ld\n", PTR_ERR(oled->dc));
        return PTR_ERR(oled->dc);
    }
    oled->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(oled->reset)) {
        dev_err(&spi->dev, "failed to get reset gpio: %ld\n", PTR_ERR(oled->reset));
        return PTR_ERR(oled->reset);
    }

   
    ssd1306_hw_reset(oled);
    ssd1306_init(oled);
    
    ssd1306_fill(oled, 0xFF); // Fill Full Screen 
    
    spi_set_drvdata(spi, oled);
    dev_info(&spi->dev, "SSD1306 probe complete\n");
    return 0;
}



static void ssd1306_remove(struct spi_device *spi){
    struct ssd1306 *oled = spi_get_drvdata(spi);
    ssd1306_send_cmd(oled, 0xAE); // Display OFF
    dev_info(&spi->dev,"SSD1306 Remove Complete")
    
    
}
const struct of_device_id ssd1306_dt_ids [] = {
    {.compatible = "bharath,ssd1306-spi"},
    { }
};

MODULE_DEVICE_TABLE(of,ssd1306_dt_ids);



static const struct spi_device_id ssd1306_spi_ids[] = {
    { "ssd1306-spi", 0 }, 
    { }
};
MODULE_DEVICE_TABLE(spi, ssd1306_spi_ids);

struct spi_driver ssd1306_spi = {
    .driver = {
        .name = "bharath-ssd1306",
        .of_match_table = ssd1306_dt_ids
    },
    .probe = ssd1306_probe,
    .remove = ssd1306_remove,
    .id_table = ssd1306_spi_ids
    
};

module_spi_driver(ssd1306_spi);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bharath Reddappa");
MODULE_VERSION("v1.0");
MODULE_DESCRIPTION("SPI Driver for SSD1306 OLED 128x64 Display");