#include<linux/module.h>
#include<linux/spi/spi.h>
#include<linux/gpio/consumer.h>
#include<linux/of.h>
#include<linux/delay.h>
#include<linux/miscdevice.h>
#include<linux/fs.h>
#include<linux/uaccess.h>

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_BUF_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)


#define SSD1306_CMD_DISPLAY_OFF       0xAE
#define SSD1306_CMD_DISPLAY_ON        0xAF
#define SSD1306_CMD_SET_CURSOR        0x21 // Column address
#define SSD1306_CMD_SET_PAGE          0x22 // Page address

struct ssd1306 {
    struct spi_device *spi;
    struct gpio_desc *dc;
    struct gpio_desc *reset;
    struct miscdevice miscdev;
};

static int ssd1306_send_cmd(struct ssd1306 *oled, uint8_t cmd);
static int ssd1306_send_data(struct ssd1306 *oled, const uint8_t *data, size_t len);

static const uint8_t font5x7[][5] = {
    // ASCII 32 to 127
    {0x00,0x00,0x00,0x00,0x00}, // 32  Space
    {0x00,0x00,0x5F,0x00,0x00}, // 33  !
    {0x00,0x07,0x00,0x07,0x00}, // 34  "
    {0x14,0x7F,0x14,0x7F,0x14}, // 35  #
    {0x24,0x2A,0x7F,0x2A,0x12}, // 36  $
    {0x23,0x13,0x08,0x64,0x62}, // 37  %
    {0x36,0x49,0x55,0x22,0x50}, // 38  &
    {0x00,0x05,0x03,0x00,0x00}, // 39  '
    {0x00,0x1C,0x22,0x41,0x00}, // 40  (
    {0x00,0x41,0x22,0x1C,0x00}, // 41  )
    {0x14,0x08,0x3E,0x08,0x14}, // 42  *
    {0x08,0x08,0x3E,0x08,0x08}, // 43  +
    {0x00,0x50,0x30,0x00,0x00}, // 44  ,
    {0x08,0x08,0x08,0x08,0x08}, // 45  -
    {0x00,0x60,0x60,0x00,0x00}, // 46  .
    {0x20,0x10,0x08,0x04,0x02}, // 47  /
    {0x3E,0x51,0x49,0x45,0x3E}, // 48  0
    {0x00,0x42,0x7F,0x40,0x00}, // 49  1
    {0x42,0x61,0x51,0x49,0x46}, // 50  2
    {0x21,0x41,0x45,0x4B,0x31}, // 51  3
    {0x18,0x14,0x12,0x7F,0x10}, // 52  4
    {0x27,0x45,0x45,0x45,0x39}, // 53  5
    {0x3C,0x4A,0x49,0x49,0x30}, // 54  6
    {0x01,0x71,0x09,0x05,0x03}, // 55  7
    {0x36,0x49,0x49,0x49,0x36}, // 56  8
    {0x06,0x49,0x49,0x29,0x1E}, // 57  9
    {0x00,0x36,0x36,0x00,0x00}, // 58  :
    {0x00,0x56,0x36,0x00,0x00}, // 59  ;
    {0x08,0x14,0x22,0x41,0x00}, // 60  <
    {0x14,0x14,0x14,0x14,0x14}, // 61  =
    {0x00,0x41,0x22,0x14,0x08}, // 62  >
    {0x02,0x01,0x51,0x09,0x06}, // 63  ?
    {0x32,0x49,0x79,0x41,0x3E}, // 64  @
    {0x7E,0x11,0x11,0x11,0x7E}, // 65  A
    {0x7F,0x49,0x49,0x49,0x36}, // 66  B
    {0x3E,0x41,0x41,0x41,0x22}, // 67  C
    {0x7F,0x41,0x41,0x22,0x1C}, // 68  D
    {0x7F,0x49,0x49,0x49,0x41}, // 69  E
    {0x7F,0x09,0x09,0x09,0x01}, // 70  F
    {0x3E,0x41,0x49,0x49,0x7A}, // 71  G
    {0x7F,0x08,0x08,0x08,0x7F}, // 72  H
    {0x00,0x41,0x7F,0x41,0x00}, // 73  I
    {0x20,0x40,0x41,0x3F,0x01}, // 74  J
    {0x7F,0x08,0x14,0x22,0x41}, // 75  K
    {0x7F,0x40,0x40,0x40,0x40}, // 76  L
    {0x7F,0x02,0x0C,0x02,0x7F}, // 77  M
    {0x7F,0x04,0x08,0x10,0x7F}, // 78  N
    {0x3E,0x41,0x41,0x41,0x3E}, // 79  O
    {0x7F,0x09,0x09,0x09,0x06}, // 80  P
    {0x3E,0x41,0x51,0x21,0x5E}, // 81  Q
    {0x7F,0x09,0x19,0x29,0x46}, // 82  R
    {0x46,0x49,0x49,0x49,0x31}, // 83  S
    {0x01,0x01,0x7F,0x01,0x01}, // 84  T
    {0x3F,0x40,0x40,0x40,0x3F}, // 85  U
    {0x1F,0x20,0x40,0x20,0x1F}, // 86  V
    {0x3F,0x40,0x38,0x40,0x3F}, // 87  W
    {0x63,0x14,0x08,0x14,0x63}, // 88  X
    {0x07,0x08,0x70,0x08,0x07}, // 89  Y
    {0x61,0x51,0x49,0x45,0x43}, // 90  Z
    {0x00,0x7F,0x41,0x41,0x00}, // 91  [
    {0x02,0x04,0x08,0x10,0x20}, // 92  backslash
    {0x00,0x41,0x41,0x7F,0x00}, // 93  ]
    {0x04,0x02,0x01,0x02,0x04}, // 94  ^
    {0x40,0x40,0x40,0x40,0x40}, // 95  _
    {0x00,0x01,0x02,0x04,0x00}, // 96  `
    {0x20,0x54,0x54,0x54,0x78}, // 97  a
    {0x7F,0x48,0x44,0x44,0x38}, // 98  b
    {0x38,0x44,0x44,0x44,0x20}, // 99  c
    {0x38,0x44,0x44,0x48,0x7F}, // 100 d
    {0x38,0x54,0x54,0x54,0x18}, // 101 e
    {0x08,0x7E,0x09,0x01,0x02}, // 102 f
    {0x0C,0x52,0x52,0x52,0x3E}, // 103 g
    {0x7F,0x08,0x04,0x04,0x78}, // 104 h
    {0x00,0x44,0x7D,0x40,0x00}, // 105 i
    {0x20,0x40,0x44,0x3D,0x00}, // 106 j
    {0x7F,0x10,0x28,0x44,0x00}, // 107 k
    {0x00,0x41,0x7F,0x40,0x00}, // 108 l
    {0x7C,0x04,0x18,0x04,0x78}, // 109 m
    {0x7C,0x08,0x04,0x04,0x78}, // 110 n
    {0x38,0x44,0x44,0x44,0x38}, // 111 o
    {0x7C,0x14,0x14,0x14,0x08}, // 112 p
    {0x08,0x14,0x14,0x18,0x7C}, // 113 q
    {0x7C,0x08,0x04,0x04,0x08}, // 114 r
    {0x48,0x54,0x54,0x54,0x20}, // 115 s
    {0x04,0x3F,0x44,0x40,0x20}, // 116 t
    {0x3C,0x40,0x40,0x20,0x7C}, // 117 u
    {0x1C,0x20,0x40,0x20,0x1C}, // 118 v
    {0x3C,0x40,0x30,0x40,0x3C}, // 119 w
    {0x44,0x28,0x10,0x28,0x44}, // 120 x
    {0x0C,0x50,0x50,0x50,0x3C}, // 121 y
    {0x44,0x64,0x54,0x4C,0x44}, // 122 z
    {0x00,0x08,0x36,0x41,0x00}, // 123 {
    {0x00,0x00,0x7F,0x00,0x00}, // 124 |
    {0x00,0x41,0x36,0x08,0x00}, // 125 }
    {0x10,0x08,0x08,0x10,0x08}  // 126 ~
};


static void ssd1306_clear(struct ssd1306 *oled)
{
    uint8_t buffer[SSD1306_BUF_SIZE];
    memset(buffer, 0x00, sizeof(buffer)); // Fill with black

    ssd1306_send_cmd(oled, SSD1306_CMD_SET_CURSOR); // Set column address
    ssd1306_send_cmd(oled, 0);                      // Start column
    ssd1306_send_cmd(oled, SSD1306_WIDTH - 1);      // End column

    ssd1306_send_cmd(oled, SSD1306_CMD_SET_PAGE);   // Set page address
    ssd1306_send_cmd(oled, 0);                      // Start page
    ssd1306_send_cmd(oled, (SSD1306_HEIGHT / 8) - 1); // End page

    ssd1306_send_data(oled, buffer, sizeof(buffer));    // Write blank buffer
}

static void ssd1306_set_cursor(struct ssd1306 *oled, uint8_t x, uint8_t y)
{
    ssd1306_send_cmd(oled, SSD1306_CMD_SET_CURSOR);
    ssd1306_send_cmd(oled, x);
    ssd1306_send_cmd(oled, SSD1306_WIDTH - 1);

    ssd1306_send_cmd(oled, SSD1306_CMD_SET_PAGE);
    ssd1306_send_cmd(oled, y);
    ssd1306_send_cmd(oled, (SSD1306_HEIGHT / 8) - 1);
}

static void ssd1306_display_text(struct ssd1306 *oled, const char *text)
{
    uint8_t data[6]; // 5 pixels + 1 space
    int i;

    while (*text) {
        char c = *text++;
        if (c < 32 || c > 126)
            c = ' ';
    
        const uint8_t *char_bitmap = font5x7[c - 32];

        for (i = 0; i < 5; i++)
            data[i] = char_bitmap[i];

        data[5] = 0x00; // Space between characters

        ssd1306_send_data(oled, data, sizeof(data));
    }
}

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

static ssize_t ssd1306_write(struct file *fp, const char __user *buf, size_t count, loff_t *len){
    
    struct ssd1306 *oled  = container_of(fp->private_data,struct ssd1306,miscdev);
    char *kbuf;
    int ret=0;
    if(count <= 0 || count > 128){
        return -EINVAL;
    }

    kbuf = kzalloc(count+1,GFP_KERNEL);
    if(!kbuf){
        return -ENOMEM;
    }

    if(copy_from_user(kbuf,buf,count)){
        ret = -EFAULT;
        goto out;
    }

    kbuf[count]='\0';
    dev_info(&oled->spi->dev,"Received data from user = %s \n",kbuf);

    ssd1306_clear(oled); // Clears Display
    ssd1306_set_cursor(oled,0,0); // Set cursor to top-left
    ssd1306_display_text(oled,kbuf); // Send text to display

    ret = count;
    

    out:
        kfree(kbuf);
        return ret; 
}

static struct file_operations ssd_fops = {
    .owner = THIS_MODULE,
    .write = ssd1306_write
};

static int ssd1306_probe(struct spi_device *spi)
{
    struct ssd1306 *oled;
    int ret;

    oled = devm_kzalloc(&spi->dev, sizeof(*oled), GFP_KERNEL);
    if (!oled)
        return -ENOMEM;

    oled->spi = spi;
    oled->miscdev.minor = MISC_DYNAMIC_MINOR;
    oled->miscdev.name = "ssd1306";
    oled->miscdev.fops = &ssd_fops;

    ret = misc_register(&oled->miscdev);
    if (ret){
        dev_info(&spi->dev,"Failed to registed misc device \n");
        return ret;
    }

    dev_info(&spi->dev,"Miscdev :%s registed with minor:%d\n",oled->miscdev.name,oled->miscdev.minor);

    
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
    misc_deregister(&oled->miscdev);
    dev_info(&spi->dev,"SSD1306 Remove Complete\n") ;
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