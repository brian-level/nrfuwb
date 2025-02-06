#include "nrfspi.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/types.h>
#include <zephyr/drivers/spi.h>

#include "autoconf.h"

#define COMPONENT_NAME nrfspi_c
#include "Logging.h"

// This code implements the controller (SPIM) side of SPI bus
//
#define SPI_DEV_NODE DT_NODELABEL(spi1)

#define SPI_MAX_DATA        (256)
#define SPI_PACKET_SIZE     (4 + SPI_MAX_DATA)

typedef struct
{
    bool                    initialized;
    int                     max_packet;
    struct k_poll_signal    spi_done_sig;
    const struct device     *dev;
    uint64_t                last_write;
    uint32_t                txRequested;
    uint32_t                rxRequested;
    struct gpio_callback    irqCallback;
    struct spi_config       spi_cfg;
    const struct gpio_dt_spec *irq_gpio;
    const struct gpio_dt_spec *sync_gpio;
    const struct gpio_dt_spec *ce_gpio;
    struct k_mutex          rxlock;
    struct k_mutex          txlock;
    uint8_t                 rxBuffer[SPI_PACKET_SIZE];
    uint8_t                 txBuffer[SPI_PACKET_SIZE];
}
nrfspi_t;

static nrfspi_t mSPI;

// rx buffer is filled in worker thread triggered by ISR and
// read by reader thread so serialize access
//
#define NRFSPI_RXRING_LOCK()      \
    k_mutex_lock( &nrfspi->rxlock, K_FOREVER )

#define NRFSPI_RXRING_UNLOCK()    \
    k_mutex_unlock( &nrfspi->rxlock )

// writes can come from multiple threads, so make sure txbuffer is
// is only used by one at a time
//
#define NRFSPI_TX_LOCK()      \
    k_mutex_lock( &nrfspi->txlock, K_FOREVER )

#define NRFSPI_TX_UNLOCK()    \
    k_mutex_unlock( &nrfspi->txlock )

#define NRFSPI_TX_ISLOCKED()  \
    (nrfspi->txlock.lock_count != 0)

static int _nrfspi_trx(
                    nrfspi_t *nrfspi,
                    const uint8_t *txdata,
                    const int txcount,
                    uint8_t *rxdata,
                    const int rxsize)
{
    int ret = 0;

    struct spi_buf rx_buf[1];
    struct spi_buf_set rx;

    struct spi_buf tx_buf[1];
    struct spi_buf_set tx;

    rx_buf[0].buf = (uint8_t*)rxdata;
    rx_buf[0].len = rxsize;

    rx.buffers = rx_buf;
    rx.count = rxdata ? 1 : 0;

    tx_buf[0].buf = txdata;
    tx_buf[0].len = txcount;

    tx.buffers = tx_buf;
    tx.count = txdata ? 1 : 0;

    ret = spi_transceive(nrfspi->dev, &nrfspi->spi_cfg, &tx, &rx);

    gpio_pin_configure_dt(nrfspi->sync_gpio, GPIO_OUTPUT_INACTIVE);

    return ret;
}

static void _host_irq_callback(const struct device *dev,
                     struct gpio_callback *cb, uint32_t pins)
{

    nrfspi_t *nrfspi = CONTAINER_OF(cb, nrfspi_t, irqCallback);

    // just count the request, well do the xfer in poll if
    // needed. the host should never interrupt unless it has
    // the whole packet to send already loaded into the sp
    // tx buffer on its side
    //
    nrfspi->rxRequested++;

    // disable host int for a bit
    gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_DISABLE);

}

static const struct spi_cs_control mspi_cs =
{
    .gpio = GPIO_DT_SPEC_GET_BY_IDX(SPI_DEV_NODE, cs_gpios, 0),
    .delay = 0
};

#define SPI1_NODE DT_NODELABEL(uci_spi)

#define IPC_GPIO_NODE DT_PATH(gpio)

static const struct gpio_dt_spec mspi_irq = GPIO_DT_SPEC_GET(DT_NODELABEL(uwb_spi_irq), gpios);
static const struct gpio_dt_spec mspi_sync = GPIO_DT_SPEC_GET(DT_NODELABEL(uwb_spi_sync), gpios);
static const struct gpio_dt_spec mspi_ce = GPIO_DT_SPEC_GET(DT_NODELABEL(uwb_spi_ce), gpios);

/*
The time required for the module to go into DPD state is < 100 탎 controlled by the firmware.
The required time for the module to enter HPD state is less then 100 탎 starting for the instance
that CE is de-asserted, in both modes VDD_1V8_DIG is turned off. The Wakeup timing from DPD
state is around 370 탎, the wakeup from HPD state is triggered once CE is asserted and takes
around 380 탎.
*/

static int _nrfspi_init(nrfspi_t *nrfspi)
{
    int ret = -1;

    memcpy(&nrfspi->spi_cfg.cs, &mspi_cs, sizeof(mspi_cs));
    nrfspi->spi_cfg.frequency = DT_PROP(SPI1_NODE, spi_max_frequency);
    nrfspi->spi_cfg.slave = DT_REG_ADDR(SPI1_NODE);
    nrfspi->irq_gpio = &mspi_irq;
    nrfspi->sync_gpio = &mspi_sync;
    nrfspi->ce_gpio = &mspi_ce;

    nrfspi->spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB; // | SPI_MODE_CPOL | SPI_MODE_CPHA;
    nrfspi->spi_cfg.operation |= SPI_OP_MODE_MASTER;

    // de-assert sync
    ret = gpio_pin_configure_dt(nrfspi->sync_gpio, GPIO_OUTPUT_INACTIVE);

    // disable chip
    ret = gpio_pin_configure_dt(nrfspi->ce_gpio, GPIO_OUTPUT_INACTIVE);

    // setup an interrupt on gpio for peripheral initiated transfers
    ret = gpio_pin_configure_dt(nrfspi->irq_gpio, GPIO_INPUT | GPIO_PULL_UP);
    require_noerr(ret, exit);

    // delay a bit to reset chip
    k_sleep(K_USEC(400));

    // enable chip
    ret = gpio_pin_configure_dt(nrfspi->ce_gpio, GPIO_OUTPUT_ACTIVE);

    // let chip boot
    k_sleep(K_USEC(400));

    // If irq pin is stuck active, disable it and assume no spi attachement
    //
    volatile int state = gpio_pin_get_dt(nrfspi->irq_gpio);
    volatile int xstate = gpio_pin_get_raw(nrfspi->irq_gpio->port, nrfspi->irq_gpio->pin);
    int attempt;

    LOG_INF("SPI irq pin %d is %d.%d on open", nrfspi->irq_gpio->pin, state, xstate);

    // note, the slave could be just coming out of reset, so wait a second to let
    // it come up before we decide its there or not, and read it to clesr its irq
    //
    for (attempt = 0; state && (attempt < 20); attempt++)
    {
        state = gpio_pin_get_dt(nrfspi->irq_gpio);
        if (state)
        {
            uint8_t junk[32];
            int c;

            nrfspi->rxRequested = 1;
            NRFSPIread(junk, sizeof(junk), true);
            k_sleep(K_MSEC(1));
        }
    }

    if (state)
    {
        // irq high for over 200ms, assume board is not populated
        //
        LOG_WRN("SPI pin %d irq stuck on, assuming no device attached", nrfspi->irq_gpio->pin);
        ret = -EIO;
        goto exit;
    }

    gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(&nrfspi->irqCallback, _host_irq_callback, BIT(nrfspi->irq_gpio->pin));
    ret = gpio_add_callback(nrfspi->irq_gpio->port, &nrfspi->irqCallback);
    require_noerr(ret, exit);

    nrfspi->txRequested = 0;
    nrfspi->rxRequested = 0;

    ret = 0;

exit:
    return ret;
}

int NRFSPIread(
                uint8_t *outRxData,
                int inRxSize,
                bool inUseSync)
{
    nrfspi_t *nrfspi = &mSPI;
    int ret = -1;
    int xret;

    require(nrfspi->initialized, exit);

    if (inUseSync)
    {
        // raise sync to allow reading
        xret = gpio_pin_configure_dt(nrfspi->sync_gpio, GPIO_OUTPUT_ACTIVE);
        require_noerr(xret, exit);
    }

    // write what we want to tx
    ret = _nrfspi_trx(nrfspi, NULL, 0, outRxData, inRxSize);

    if (inUseSync)
    {
        // raise sync to allow reading
        xret = gpio_pin_configure_dt(nrfspi->sync_gpio, GPIO_OUTPUT_INACTIVE);
        require_noerr(xret, exit);
    }

exit:
    nrfspi->rxRequested = 0;
    gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    return ret;
}

int NRFSPIwrite(
                const uint8_t *inTxData,
                const int inTxCount)
{
    nrfspi_t *nrfspi = &mSPI;
    int ret;

    require(nrfspi->initialized, exit);

    // write what we want to tx
    ret = _nrfspi_trx(nrfspi, inTxData, inTxCount, NULL, 0);
exit:
    return ret;
}

int NRFSPIpoll(bool *outReadable)
{
    int ret = -1;

    require(outReadable, exit);
    *outReadable = mSPI.rxRequested > 0;
    ret = 0;
exit:
    return ret;
}

void NRFSPIdeinit(void)
{
    nrfspi_t *nrfspi;

    nrfspi = &mSPI;

    if (nrfspi->initialized)
    {
        // abort any active spi transactions
        nrfspi->initialized = false;

        // turn off host interrupts
        gpio_pin_interrupt_configure_dt(nrfspi->irq_gpio, GPIO_INT_DISABLE);
    }
}

int NRFSPIinit(void)
{
    nrfspi_t *nrfspi;
    int ret = -1;

    nrfspi = &mSPI;

    if (!nrfspi->initialized)
    {
        k_poll_signal_init(&nrfspi->spi_done_sig);

        nrfspi->dev = DEVICE_DT_GET(SPI_DEV_NODE);
        require(nrfspi->dev, exit);

        require(device_is_ready(nrfspi->dev), exit);

        ret = k_mutex_init(&nrfspi->rxlock);
        require_noerr(ret, exit);
        ret = k_mutex_init(&nrfspi->txlock);
        require_noerr(ret, exit);

        nrfspi->initialized = true;
        nrfspi->max_packet = 255;
    }

    ret = _nrfspi_init(nrfspi);
    require_noerr(ret, exit);
exit:
    return ret;
}

