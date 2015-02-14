/*!
 * FHT heating valve comms example with RFM22/23 for AVR
 *
 * Copyright (C) 2013 Mike Stirling
 *
 * The OpenTRV project licenses this file to you
 * under the Apache Licence, Version 2.0 (the "Licence");
 * you may not use this file except in compliance
 * with the Licence. You may obtain a copy of the Licence at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the Licence is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the Licence for the
 * specific language governing permissions and limitations
 * under the Licence.
 *
 * \file si443x_min.h
 * \brief Minimal polled driver for Si443x transceiver
 *
 * This is a cut down version of a larger driver.
 * It only supports raw FIFO mode and does not handle
 * packets larger than the radio's FIFO (64 bytes).
 *
 */

#ifndef SI443X_H_
#define SI443X_H_

#ifndef MHZ
#define MHZ					1000000
#endif
#ifndef KHZ
#define KHZ					1000
#endif


int si443x_status(void);


/*!
 * Initialise the radio and place it into its default configuration (and in
 * standby mode).  Must be called at power up and after the radio has been
 * in shutdown mode.
 *
 * \return				0 on success or -1 if radio not found
 */
int si443x_init(void);

/*! Receive a packet, blocking until one is available.  Packets must start with
 * preamble and the currently selected sync word.
 * \param	data			Pointer to buffer large enough for received packet
 * \param	data_length		Size of data buffer (max 64 bytes)
 * \param	timeout			Timeout (in system ticks)
 * \param	rssi			Pointer to variable to be populated with RSSI (signal strength)
 * \return					0 on success, -1 on error (timeout)
 */
int si443x_receive(uint8_t *data, uint8_t data_length, int timeout, int *rssi);

/*! Transmit a packet, blocking until complete
 * \param	data			Pointer to data buffer
 * \param	data_length		Size of data buffer (max 64 bytes)
 * \return					0
 */
int si443x_transmit(uint8_t *data, uint8_t data_length);

/*! Dump registers */
void si443x_dump(void);

/*! Current temperature from on-chip sensor */
int16_t si443x_get_temperature(void);

/*! Print above temperature */
int16_t si443x_temp_print(void);



#endif /* SI443X_H_ */
