:orphan:

.. _migration_4.1:

Migration guide to Zephyr v4.1.0 (Working Draft)
################################################

This document describes the changes required when migrating your application from Zephyr v4.0.0 to
Zephyr v4.1.0.

Any other changes (not directly related to migrating applications) can be found in
the :ref:`release notes<zephyr_4.1>`.

.. contents::
    :local:
    :depth: 2

Build System
************

Kernel
******

Boards
******

Modules
*******

Mbed TLS
========

* If a platform has a CSPRNG source available (i.e. :kconfig:option:`CONFIG_CSPRNG_ENABLED`
  is set), then the Kconfig option :kconfig:option:`CONFIG_MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG`
  is the default choice for random number source instead of
  :kconfig:option:`CONFIG_MBEDTLS_PSA_CRYPTO_LEGACY_RNG`. This helps in reducing
  ROM/RAM footprint of the Mbed TLS library.

* The newly-added Kconfig option :kconfig:option:`CONFIG_MBEDTLS_PSA_KEY_SLOT_COUNT`
  allows to specify the number of key slots available in the PSA Crypto core.
  Previously this value was not explicitly set, so Mbed TLS's default value of
  32 was used. The new Kconfig option defaults to 16 instead in order to find
  a reasonable compromise between RAM consumption and most common use cases.
  It can be further trimmed down to reduce RAM consumption if the final
  application doesn't need that many key slots simultaneously.

Trusted Firmware-M
==================

LVGL
====

* The config option :kconfig:option:`CONFIG_LV_Z_FLUSH_THREAD_PRIO` is now called
  :kconfig:option:`CONFIG_LV_Z_FLUSH_THREAD_PRIORITY` and its value is now interpreted as an
  absolute priority instead of a cooperative one.

Device Drivers and Devicetree
*****************************

* Device driver APIs are placed into iterable sections (:github:`71773`) to allow for runtime
  checking. See :ref:`device_driver_api` for more details.
  The :c:macro:`DEVICE_API()` macro should be used by out-of-tree driver implementations for
  the following driver classes:

    * :c:struct:`adc_driver_api` (:github:`72292`)
    * :c:struct:`auxdisplay_driver_api` (:github:`82121`)
    * :c:struct:`bbram_driver_api` (:github:`82123`)
    * :c:struct:`bc12_driver_api` (:github:`82125`)
    * :c:struct:`bt_hci_driver_api` (:github:`82126`)
    * :c:struct:`can_driver_api` (:github:`82128`)
    * :c:struct:`can_transceiver_driver_api` (:github:`82146`)
    * :c:struct:`cellular_driver_api` (:github:`82147`)
    * :c:struct:`charger_driver_api` (:github:`82148`)
    * :c:struct:`clock_control_driver_api` (:github:`82150`)
    * :c:struct:`comparator_driver_api` (:github:`82186`)
    * :c:struct:`coredump_driver_api` (:github:`82164`)
    * :c:struct:`counter_driver_api` (:github:`82166`)
    * :c:struct:`crypto_driver_api` (:github:`82167`)
    * :c:struct:`dac_driver_api` (:github:`82168`)
    * :c:struct:`dai_driver_api` (:github:`82170`)
    * :c:struct:`display_driver_api` (:github:`82171`)
    * :c:struct:`dma_driver_api` (:github:`82292`)
    * :c:struct:`edac_driver_api` (:github:`82172`)
    * :c:struct:`eeprom_driver_api` (:github:`82173`)
    * :c:struct:`entropy_driver_api` (:github:`82174`)
    * :c:struct:`espi_driver_api` (:github:`82296`)
    * :c:struct:`espi_saf_driver_api` (:github:`82296`)
    * :c:struct:`ethphy_driver_api` (:github:`82175`)
    * :c:struct:`flash_driver_api` (:github:`82291`)
    * :c:struct:`fpga_driver_api` (:github:`82176`)
    * :c:struct:`fuel_gauge_driver_api` (:github:`82178`)
    * :c:struct:`gnss_driver_api` (:github:`82179`)
    * :c:struct:`gpio_driver_api` (:github:`82180`)
    * :c:struct:`haptics_driver_api` (:github:`82181`)
    * :c:struct:`hwspinlock_driver_api` (:github:`82182`)
    * :c:struct:`i2c_driver_api` (:github:`82207`)
    * :c:struct:`i2s_driver_api` (:github:`82233`)
    * :c:struct:`i3c_driver_api` (:github:`82207`)
    * :c:struct:`ipm_driver_api` (:github:`82234`)
    * :c:struct:`its_driver_api` (:github:`82241`)
    * :c:struct:`ivshmem_driver_api` (:github:`82242`)
    * :c:struct:`kscan_driver_api` (:github:`82246`)
    * :c:struct:`led_driver_api` (:github:`82249`)
    * :c:struct:`led_strip_driver_api` (:github:`82250`)
    * :c:struct:`lora_driver_api` (:github:`82264`)
    * :c:struct:`mbox_driver_api` (:github:`82266`)
    * :c:struct:`mdio_driver_api` (:github:`82267`)
    * :c:struct:`mipi_dbi_driver_api` (:github:`82270`)
    * :c:struct:`mipi_dsi_driver_api` (:github:`82271`)
    * :c:struct:`mspi_driver_api` (:github:`82296`)
    * :c:struct:`nrf_clock_control_driver_api` (:github:`82277`)
    * :c:struct:`pcie_ctrl_driver_api` (:github:`82278`)
    * :c:struct:`pcie_ep_driver_api` (:github:`82279`)
    * :c:struct:`peci_driver_api` (:github:`82283`)
    * :c:struct:`ps2_driver_api` (:github:`82283`)
    * :c:struct:`ptp_clock_driver_api` (:github:`82301`)
    * :c:struct:`pwm_driver_api` (:github:`82253`)
    * :c:struct:`regulator_driver_api` (:github:`82290`)
    * :c:struct:`regulator_parent_driver_api` (:github:`82290`)
    * :c:struct:`reset_driver_api` (:github:`82283`)
    * :c:struct:`retained_mem_driver_api` (:github:`82301`)
    * :c:struct:`rtc_driver_api` (:github:`82337`)
    * :c:struct:`sdhc_driver_api` (:github:`82301`)
    * :c:struct:`sensor_driver_api` (:github:`72293`)
    * :c:struct:`shared_irq_driver_api` (:github:`82283`)
    * :c:struct:`smbus_driver_api` (:github:`82283`)
    * :c:struct:`spi_driver_api` (:github:`82261`)
    * :c:struct:`stepper_driver_api` (:github:`82283`)
    * :c:struct:`svc_driver_api` (:github:`82283`)
    * :c:struct:`syscon_driver_api` (:github:`82283`)
    * :c:struct:`tcpc_driver_api` (:github:`82285`)
    * :c:struct:`tee_driver_api` (:github:`82283`)
    * :c:struct:`tgpio_driver_api` (:github:`82283`)
    * :c:struct:`uart_driver_api` (:github:`82293`)
    * :c:struct:`usbc_ppc_driver_api` (:github:`82285`)
    * :c:struct:`usbc_vbus_driver_api` (:github:`82285`)
    * :c:struct:`video_driver_api` (:github:`82301`)
    * :c:struct:`vtd_driver_api` (:github:`82283`)
    * :c:struct:`w1_driver_api` (:github:`82297`)
    * :c:struct:`wdt_driver_api` (:github:`82300`)


Controller Area Network (CAN)
=============================

Display
=======

* Displays using the MIPI DBI driver which set their MIPI DBI mode via the
  ``mipi-mode`` property in devicetree should now use a string property of
  the same name, like so:

  .. code-block:: devicetree

    /* Legacy display definition */

    st7735r: st7735r@0 {
        ...
        mipi-mode = <MIPI_DBI_MODE_SPI_4WIRE>;
        ...
    };

    /* New display definition */

    st7735r: st7735r@0 {
        ...
        mipi-mode = "MIPI_DBI_MODE_SPI_4WIRE";
        ...
    };


Enhanced Serial Peripheral Interface (eSPI)
===========================================

Entropy
=======

* BT HCI based entropy driver now directly sends the HCI command to parse random
  data instead of waiting for BT connection to be ready. This is helpful on
  platforms where the BT controller owns the HW random generator and the application
  processor needs to get random data before BT is fully enabled.
  (:github:`79931`)

GNSS
====

Input
=====

Interrupt Controller
====================

LED Strip
=========

Pin Control
===========

  * Renamed the ``compatible`` from ``nxp,kinetis-pinctrl`` to :dtcompatible:`nxp,port-pinctrl`.
  * Renamed the ``compatible`` from ``nxp,kinetis-pinmux`` to :dtcompatible:`nxp,port-pinmux`.
  * Silabs Series 2 devices now use a new pinctrl driver selected by
    :dtcompatible:`silabs,dbus-pinctrl`. This driver allows the configuration of GPIO properties
    through device tree, rather than having them hard-coded for each supported signal. It also
    supports all possible digital bus signals by including a binding header such as
    :zephyr_file:`include/zephyr/dt-bindings/pinctrl/silabs/xg24-pinctrl.h`.

    Pinctrl should now be configured like this:

    .. code-block:: devicetree

      #include <dt-bindings/pinctrl/silabs/xg24-pinctrl.h>

      &pinctrl {
        i2c0_default: i2c0_default {
          group0 {
            /* Pin selection(s) using bindings included above */
            pins = <I2C0_SDA_PD2>, <I2C0_SCL_PD3>;
            /* Shared properties for the group of pins */
            drive-open-drain;
            bias-pull-up;
          };
        };
      };


Sensors
=======

Serial
======

Stepper
=======

  * Renamed the ``compatible`` from ``zephyr,gpio-steppers`` to :dtcompatible:`zephyr,gpio-stepper`.
  * Renamed the ``stepper_set_actual_position`` function to :c:func:`stepper_set_reference_position`.

Regulator
=========

Video
=====

* The :file:`include/zephyr/drivers/video-controls.h` got updated to have video controls IDs (CIDs)
  matching the definitions in the Linux kernel file ``include/uapi/linux/v4l2-controls.h``.
  In most cases, removing the category prefix is enough: ``VIDEO_CID_CAMERA_GAIN`` becomes
  ``VIDEO_CID_GAIN``.
  The new ``video-controls.h`` source now contains description of each control ID to help
  disambiguating.

Bluetooth
*********

Bluetooth HCI
=============

Bluetooth Mesh
==============

* Following the beginnig of the deprecation process for the TinyCrypt crypto
  library, Kconfig symbol :kconfig:option:`CONFIG_BT_MESH_USES_TINYCRYPT` was
  set as deprecated. Default option for platforms that do not support TF-M
  is :kconfig:option:`CONFIG_BT_MESH_USES_MBEDTLS_PSA`.

Bluetooth Audio
===============

* The following Kconfig options are not longer automatically enabled by the LE Audio Kconfig
  options and may need to be enabled manually (:github:`81328`):

    * :kconfig:option:`CONFIG_BT_GATT_CLIENT`
    * :kconfig:option:`CONFIG_BT_GATT_AUTO_DISCOVER_CCC`
    * :kconfig:option:`CONFIG_BT_GATT_AUTO_UPDATE_MTU`
    * :kconfig:option:`CONFIG_BT_EXT_ADV`
    * :kconfig:option:`CONFIG_BT_PER_ADV_SYNC`
    * :kconfig:option:`CONFIG_BT_ISO_BROADCASTER`
    * :kconfig:option:`CONFIG_BT_ISO_SYNC_RECEIVER`
    * :kconfig:option:`CONFIG_BT_PAC_SNK`
    * :kconfig:option:`CONFIG_BT_PAC_SRC`

Bluetooth Classic
=================

Bluetooth Host
==============

* :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT` has been deprecated. The number of ACL RX buffers is
  now computed internally and is equal to :kconfig:option:`CONFIG_BT_MAX_CONN` + 1. If an application
  needs more buffers, it can use the new :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT_EXTRA` to add
  additional ones.

  e.g. if :kconfig:option:`CONFIG_BT_MAX_CONN` was ``3`` and
  :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT` was ``7`` then
  :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT_EXTRA` should be set to ``7 - (3 + 1) = 3``.

  .. warning::

   The default value of :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT` has been set to 0.

Bluetooth Crypto
================

Networking
**********

* The Prometheus metric creation has changed as user does not need to have a separate
  struct :c:struct:`prometheus_metric` any more. This means that the Prometheus macros
  :c:macro:`PROMETHEUS_COUNTER_DEFINE`, :c:macro:`PROMETHEUS_GAUGE_DEFINE`,
  :c:macro:`PROMETHEUS_HISTOGRAM_DEFINE` and :c:macro:`PROMETHEUS_SUMMARY_DEFINE`
  prototypes have changed. (:github:`81712`)

* The default subnet mask on newly added IPv4 addresses is now specified with
  :kconfig:option:`CONFIG_NET_IPV4_DEFAULT_NETMASK` option instead of being left
  empty. Applications can still specify a custom netmask for an address with
  :c:func:`net_if_ipv4_set_netmask_by_addr` function if needed.

Other Subsystems
****************

Flash map
=========

hawkBit
=======

MCUmgr
======

Modem
=====

Architectures
*************

* Common

  * ``_current`` is deprecated, used :c:func:`arch_current_thread` instead.

* native/POSIX

  * :kconfig:option:`CONFIG_NATIVE_APPLICATION` has been deprecated. Out-of-tree boards using this
    option should migrate to the native_simulator runner (:github:`81232`).
    For an example of how this was done with a board in-tree check :github:`61481`.
  * For the native_sim target :kconfig:option:`CONFIG_NATIVE_SIM_NATIVE_POSIX_COMPAT` has been
    switched to ``n`` by default, and this option has been deprecated. Ensure your code does not
    use the :kconfig:option:`CONFIG_BOARD_NATIVE_POSIX` option anymore (:github:`81232`).
