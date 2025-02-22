/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h> // memcpy
#include <stdio.h>
#include <nds/system.h>
#include <nds/debug.h>

//#include "my_fat.h"
#include "debug_file.h"
#include "nds_header.h"
#include "cardengine_header_arm7.h"
#include "cheat_engine.h"
#include "value_bits.h"
#include "common.h"
#include "patch.h"
#include "find.h"
#include "hook.h"

#define b_gameOnFlashcard BIT(0)
#define b_saveOnFlashcard BIT(1)
#define b_extendedMemory BIT(2)
#define b_ROMinRAM BIT(3)
#define b_dsiMode BIT(4)
#define b_dsiSD BIT(5)
#define b_preciseVolumeControl BIT(6)
#define b_powerCodeOnVBlank BIT(7)
#define b_runCardEngineCheck BIT(8)
#define b_ipcEveryFrame BIT(9)

extern u32 newArm7binarySize;

static const int MAX_HANDLER_LEN = 50;

static const u32 handlerStartSig[3] = {
	0xe92d4000, 	// push {lr}
	0xe3a0c301, 	// mov  ip, #0x4000000
	0xe28cce21		// add  ip, ip, #0x210
};

static const u32 handlerEndSig[1] = {
	0xe12fff10		// bx   r0
};

static const u32 irqListEndTwlSig[1] = {0x12345678};

static u32* findIrqHandlerOffset(const u32* start, size_t size) {
	// Find the start of the handler
	u32* addr = findOffset(
		start, size,
		handlerStartSig, 3
	);
	if (!addr) {
		return NULL;
	}

	return addr;
}

static u32* findIrqHandlerWordsOffset(u32* handlerOffset, const u32* start, size_t size) {
	// Find the end of the handler
	u32* addr = findOffset(
		handlerOffset, MAX_HANDLER_LEN*sizeof(u32),
		handlerEndSig, 1
	);
	if (!addr) {
		return NULL;
	}

	return addr+1;
}

static u32* findIrqListOffset(const u32* start, size_t size) {
	// Find the start of the handler
	u32* addr = findOffset(
		start, size,
		irqListEndTwlSig, 1
	);
	if (!addr) {
		return NULL;
	}

	addr -= 1;
	while (*addr == 0 || *addr == 0xFFFFFFFF) {
		addr -= 1;
	}
	addr -= 0x7C/sizeof(u32);

	return addr;
}

int hookNdsRetailArm7(
	cardengineArm7* ce7,
	const tNDSHeader* ndsHeader,
	const module_params_t* moduleParams,
	u32 fileCluster,
	u32 srParamsFileCluster,
	u32 ramDumpCluster,
	u32 wideCheatFileCluster,
	u32 wideCheatSize,
	u32 cheatFileCluster,
	u32 cheatSize,
	u32 apPatchFileCluster,
	u32 apPatchSize,
    u8 gameOnFlashcard,
    u8 saveOnFlashcard,
	u8 language,
	u8 dsiMode, // SDK 5
	u8 dsiSD,
	u8 extendedMemory,
	u8 ROMinRAM,
	u8 consoleModel,
	u8 romRead_LED,
	u8 dmaRomRead_LED
) {
	dbg_printf("hookNdsRetailArm7\n");

	bool isDSiWare = (ce7 == NULL);

	if (isDSiWare) newArm7binarySize = ndsHeader->arm7binarySize;

	if (newArm7binarySize < 0x1000) {
		return ERR_NONE;
	}

	u32* handlerLocation = patchOffsetCache.a7IrqHandlerOffset;
	if (!handlerLocation && !isDSiWare) {
		handlerLocation = findIrqHandlerOffset((u32*)ndsHeader->arm7destination, newArm7binarySize);
		if (handlerLocation) {
			patchOffsetCache.a7IrqHandlerOffset = handlerLocation;
		}
	}

	const char* romTid = getRomTid(ndsHeader);

	if (!handlerLocation && !isDSiWare) {
	/*	if (strncmp(romTid, "YGX", 3) == 0) {
			ce7->valueBits |= b_powerCodeOnVBlank;
		} else {
			// Patch
			memcpy(handlerLocation, ce7->patches->j_irqHandler, 0xC);
		}
	} else {*/
		dbg_printf("ERR_HOOK\n");
		return ERR_HOOK;
	}

	u32* wordsLocation = patchOffsetCache.a7IrqHandlerWordsOffset;
	if (!wordsLocation && !isDSiWare) {
		wordsLocation = findIrqHandlerWordsOffset(handlerLocation, (u32*)ndsHeader->arm7destination, newArm7binarySize);
		if (wordsLocation) {
			patchOffsetCache.a7IrqHandlerWordsOffset = wordsLocation;
		}
	}

	u32* hookLocation = patchOffsetCache.a7IrqHookOffset;
	if (!hookLocation) {
		// Now find the IRQ vector table
		if (ndsHeader->unitCode > 0) {
			switch (newArm7binarySize) {	// SDK 5
				case 0x0001D5A8:
					hookLocation = (u32*)0x239D280;		// DS WiFi Settings
					break;

				case 0x00022B40:
					hookLocation = (u32*)0x238DED8;
					break;

				case 0x00022BCC:
					hookLocation = (u32*)0x238DF60;
					break;

				case 0x00025664:
					hookLocation = (u32*)0x23A5330;		// DSi-Exclusive cart games
					if (*(u32*)0x23A5660 != 0xFFFFFFFF) {
						hookLocation = (u32*)0x23A5340;		// DSiWare alternative
					}
					break;

				case 0x000257DC:
					hookLocation = (u32*)0x23A54B8;		// DSi-Exclusive cart games
					break;

				case 0x00025860:
					hookLocation = (u32*)0x23A5538;		// DSi-Exclusive cart games
					break;

				case 0x00026DF4:
					hookLocation = (u32*)0x23A6AD4;		// DSi-Exclusive cart games
					break;

				case 0x00028F84:
					hookLocation = (u32*)0x2391918;
					break;

				case 0x0002909C:
					hookLocation = (u32*)0x2391A30;
					break;

				case 0x0002914C:
				case 0x00029164:
					hookLocation = (u32*)0x2391ADC;
					break;

				case 0x00029EE8:
					hookLocation = (u32*)0x2391F70;
					break;

				case 0x0002A2EC:
					hookLocation = (u32*)0x23921BC;
					break;

				case 0x0002A318:
					hookLocation = (u32*)0x23921D8;
					break;

				case 0x0002AF18:
					hookLocation = (u32*)0x239227C;
					break;

				case 0x0002B184:
					hookLocation = (u32*)0x23924CC;
					break;

				case 0x0002B24C:
					hookLocation = (u32*)0x2392578;
					break;

				case 0x0002C5B4:
					hookLocation = (u32*)0x2392E74;
					break;
			}
			if (!hookLocation) {
				hookLocation = findIrqListOffset((u32*)ndsHeader->arm7destination, newArm7binarySize);
			}
			if (!hookLocation && ndsHeader->unitCode == 3) {
				switch (*(u32*)0x02FFE1DC) {
					case 0x6AFD4:
						hookLocation = (u32*)0x2EE7360;
						break;
					case 0x7250C:
						hookLocation = (u32*)0x2EE5E10;
						break;
				}
			}
			if (!hookLocation && ndsHeader->unitCode == 3) {
				dbg_printf("ERR_HOOK\n");
				return ERR_HOOK;
			}
		} else if (wordsLocation[1] >= 0x037F0000 && wordsLocation[1] < 0x03800000) {
			// Use relative and absolute addresses to find the location of the table in RAM
			u32 tableAddr = wordsLocation[0];
			u32 returnAddr = wordsLocation[1];
			u32* actualReturnAddr = wordsLocation + 2;
			hookLocation = actualReturnAddr + (tableAddr - returnAddr)/sizeof(u32);
		}
	}

	/* *(ce7->irqTable_offset) = wordsLocation[0];
	if (wordsLocation[1] >= 0x037F0000 && wordsLocation[1] < 0x03800000) {
		*(ce7->irqTable_offset + 1) = wordsLocation[1];
	} else {
		*(ce7->irqTable_offset + 1) = wordsLocation[3];
	} */

   	dbg_printf("hookLocation arm7: ");
	dbg_hexa((u32)hookLocation);
	dbg_printf("\n\n");
	patchOffsetCache.a7IrqHookOffset = hookLocation;

	u32* vblankHandler = hookLocation;
	u32* ipcSyncHandler = hookLocation + 16;
	//u32* ndma0Handler = hookLocation + 28;

	if (isDSiWare) {
		u32 intr_vblank_orig_return = *(u32*)0x2FFC004;
		intr_vblank_orig_return += 0x2FFC008;

		*(u32*)intr_vblank_orig_return = *vblankHandler;
		*vblankHandler = 0x2FFC008;
	} else {
		ce7->intr_vblank_orig_return  = *vblankHandler;
		ce7->intr_fifo_orig_return    = *ipcSyncHandler;
		//ce7->intr_ndma0_orig_return   = *ndma0Handler;
		ce7->moduleParams             = moduleParams;
		ce7->fileCluster              = fileCluster;
		ce7->srParamsCluster          = srParamsFileCluster;
		ce7->ramDumpCluster           = ramDumpCluster;
		if (gameOnFlashcard) {
			ce7->valueBits |= b_gameOnFlashcard;
		}
		if (saveOnFlashcard) {
			ce7->valueBits |= b_saveOnFlashcard;
		}
		if (extendedMemory) {
			ce7->valueBits |= b_extendedMemory;
		}
		if (ROMinRAM) {
			ce7->valueBits |= b_ROMinRAM;
		}
		if (dsiMode) {
			ce7->valueBits |= b_dsiMode; // SDK 5
		}
		if (dsiSD) {
			ce7->valueBits |= b_dsiSD;
		}
		if (consoleModel < 2 && preciseVolumeControl && romRead_LED == 0 && dmaRomRead_LED == 0) {
			ce7->valueBits |= b_preciseVolumeControl;
		}
		ce7->language                 = language;
		if (strcmp(romTid, "AKYP") == 0) { // Etrian Odyssey (EUR)
			ce7->languageAddr = (u32*)0x020DC5DC;
		} else if (strcmp(romTid, "AWIP") == 0) { // Hotel Dusk (EUR)
			ce7->languageAddr = (u32*)0x02100BBC;
		}
		ce7->consoleModel             = consoleModel;
		ce7->romRead_LED              = romRead_LED;
		ce7->dmaRomRead_LED           = dmaRomRead_LED;

		*vblankHandler = ce7->patches->vblankHandler;
		if ((strncmp(romTid, "UOR", 3) == 0)
		|| (strncmp(romTid, "UXB", 3) == 0)
		|| (strncmp(romTid, "USK", 3) == 0)
		|| (!gameOnFlashcard || ROMinRAM)) {
			*ipcSyncHandler = ce7->patches->fifoHandler;
			//*ndma0Handler = ce7->patches->ndma0Handler;
			if (!ROMinRAM) {
				ce7->valueBits |= b_runCardEngineCheck;
			}
		}

		/*extern bool setDmaPatched;

		if (!setDmaPatched
		// && strncmp(romTid, "ALK", 3) != 0
		 && strncmp(romTid, "VDE", 3) != 0) {
			ce7->valueBits |= b_ipcEveryFrame;
		}*/
	}

	aFile wideCheatFile = getFileFromCluster(wideCheatFileCluster);
	aFile cheatFile = getFileFromCluster(cheatFileCluster);
	aFile apPatchFile = getFileFromCluster(apPatchFileCluster);
	if (wideCheatSize+cheatSize+(apPatchIsCheat ? apPatchSize : 0) <= (isDSiWare ? 0x1C00 : 0x8000)) {
		u32 cheatEngineOffset = (u32)ce7-0x8400;
		char* cheatDataOffset = (char*)cheatEngineOffset+0x3E8;
		if (isDSiWare) {
			cheatEngineOffset = 0x2FFC000;
			cheatDataOffset = (char*)cheatEngineOffset+0x3E8;
		}
		if (ndsHeader->unitCode < 3 && apPatchFile.firstCluster != CLUSTER_FREE && apPatchIsCheat) {
			fileRead(cheatDataOffset, apPatchFile, 0, apPatchSize, 0);
			cheatDataOffset += apPatchSize;
			*(cheatDataOffset + 3) = 0xCF;
			dbg_printf("AP-fix found and applied\n");
		}
		if (wideCheatFile.firstCluster != CLUSTER_FREE) {
			fileRead(cheatDataOffset, wideCheatFile, 0, wideCheatSize, 0);
			cheatDataOffset += wideCheatSize;
			*(cheatDataOffset + 3) = 0xCF;
		}
		if (cheatFile.firstCluster != CLUSTER_FREE) {
			fileRead(cheatDataOffset, cheatFile, 0, cheatSize, 0);
		}
	}

	dbg_printf("ERR_NONE\n");
	return ERR_NONE;
}
