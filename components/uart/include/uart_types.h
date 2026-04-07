/****************************************************************
 * Versiyon       : v1.1.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 08.04.2026
 *===============================================================
 * Aciklama      : UART sub-modulu tip tanimlari. uart_cfg_t
 *                 yapilandirma struct'i ve uart_handle_t
 *                 tanitici tipini tanimlar.
 ****************************************************************/

#pragma once

#include "driver/uart.h"

typedef struct {
    uart_port_t           port_num;      /*!< UART port numarasi             */
    int                   tx_pin;        /*!< UART TX pin numarasi           */
    int                   rx_pin;        /*!< UART RX pin numarasi           */
    int                   baud_rate;     /*!< UART baud rate                 */
    int                   rx_buf_size;   /*!< Alim tamponu boyutu (bayt)     */
    int                   tx_buf_size;   /*!< Gonderim tamponu boyutu (bayt) */
    uart_word_length_t    veri_bitleri;  /*!< Veri bit sayisi                */
    uart_parity_t         parity;        /*!< Parite ayari                   */
    uart_stop_bits_t      stop_bitleri;  /*!< Stop bit sayisi                */
    uart_hw_flowcontrol_t akis_kontrolu; /*!< Donanim akis kontrolu          */
} uart_cfg_t;

typedef uart_port_t uart_handle_t;
