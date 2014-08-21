/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file      Ethernet_Interface_Extern.h
 * \brief     Exports API functions for Ethernet interface
 * \author    Ashwin R Uchil
 * \copyright Copyright (c) 2014, Robert Bosch Engineering and Business Solutions. All rights reserved.
 *
 * Exports API functions for Vector XL CAN Hardware interface
 */

#pragma once

#if defined USAGEMODE
#undef USAGEMODE
#endif

#if defined USAGE_EXPORT
#define USAGEMODE   __declspec(dllexport)
#else
#define USAGEMODE   __declspec(dllimport)
#endif


#ifdef __cplusplus
extern "C" {  // only need to export C interface if used by C++ source code
#endif

    /*  Exported function list */
	//Interface to interpret received messages. Used in Ethernet Interface
    USAGEMODE HRESULT GetCustomAppLayerProtocol(void** ppvInterface);

	//Interface to add custom headers to form Application layer protocol. Used in Ethernet Tx window
	USAGEMODE HRESULT GetCustomTxAppLayerProtocol(void** ppvInterface);

#ifdef __cplusplus
}
#endif