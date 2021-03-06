//#include <AP_HAL.h>

//#if CONFIG_HAL_BOARD == HAL_BOARD_STM32F4

#include "EEPROM.h"
#include <stdio.h>

/**
 * @brief  Erase page with increment erase counter (page + 2)
 * @param  page base address
 * @retval Success or error
 *			FLASH_COMPLETE: success erase
 *			- Flash error code: on write Flash error
 */
FLASH_Status EEPROMClass::EE_EraseSector(uint32_t pageBase)
{
		FLASH_Status FlashStatus;
		uint32_t sector_erase;
		uint16_t data; 

		if(pageBase < EEPROM_START_ADDRESS && pageBase >= EEPROM_SWAP_BASE)
				sector_erase = EEPROM_SWAP_SECTOR;
		else
				sector_erase = EEPROM_MAIN_SECTOR;

		data = (*(__IO uint16_t*)(sector_erase));

		if ((data == EEPROM_ERASED) || (data == EEPROM_VALID_PAGE) || (data == EEPROM_RECEIVE_DATA) || (data == EEPROM_IGNORED_PAGE))
				data = (*(__IO uint16_t*)(pageBase + 2)) + 1;
		else
				data = 0;

		FlashStatus = FLASH_EraseSector(sector_erase, VoltageRange_3);
		if (FlashStatus == FLASH_COMPLETE)
				FlashStatus = FLASH_ProgramHalfWord(pageBase + 2, data);

		return FlashStatus;
}


/**
 * @brief  Find valid Page for write or read operation
 * @param	Page0: Page0 base address
 *			Page1: Page1 base address
 * @retval Valid page address (PAGE0 or PAGE1) or NULL in case of no valid page was found
 */
uint32_t EEPROMClass::EE_FindValidPage(uint32_t sector)
{
		uint32_t pageBase;
		uint32_t sector_end = sector + EEPROM_SECTOR_SIZE;
		for (pageBase = sector; pageBase < sector_end; pageBase += PageSize)
		{
				if (EEPROM_VALID_PAGE == *(__IO uint16_t *)pageBase)
						return pageBase;
		}
		return 0;
}

/**
 * @brief  Calculate unique variables in EEPROM
 * @param  start: address of first slot to check (page + 4)
 * @param	end: page end address
 * @param	address: 16 bit virtual address of the variable to excluse (or 0XFFFF)
 * @retval count of variables
 */
uint16_t EEPROMClass::EE_GetVariablesCount(uint16_t skipAddress)
{
		uint16_t varAddress, nextAddress;
		uint32_t idx;
		uint32_t pageBase = CurPageBase;
		uint32_t pageEnd = CurPageBase + (uint32_t)PageSize;
		uint16_t mycount = 0;

		for (pageBase += 6; pageBase < pageEnd; pageBase += 4)
		{
				varAddress = (*(__IO uint16_t*)pageBase);
				if (varAddress == 0xFFFF || varAddress == skipAddress)
						continue;

				mycount++;
				for(idx = pageBase + 4; idx < pageEnd; idx += 4)
				{
						nextAddress = (*(__IO uint16_t*)idx);
						if (nextAddress == varAddress)
						{
								mycount--;
								break;
						}
				}
		}
		return mycount;
}

/**
 * @brief  here we assume that newPage is valid, this function doesn't manage page status
 * @param  newPage: new page base address
 * @param	oldPage: old page base address
 *	@param	SkipAddress: 16 bit virtual address of the variable (or 0xFFFF)
 * @retval Success or error status:
 *           - FLASH_COMPLETE: on success
 *           - EEPROM_OUT_SIZE: if valid new page is full
 *           - Flash error code: on write Flash error
 */
uint16_t EEPROMClass::EE_PageTransfer(uint32_t newPage, uint32_t oldPage, uint16_t SkipAddress)
{
		uint32_t oldEnd, newEnd;
		uint32_t oldIdx, newIdx, idx;
		uint16_t address, data, found;
		FLASH_Status FlashStatus;

		// Transfer process: transfer variables from old to the new active page
		newEnd = newPage + ((uint32_t)PageSize);
		// Find first free element in new page
		for (newIdx = newPage + 4; newIdx < newEnd; newIdx += 4)
				if ((*(__IO uint32_t*)newIdx) == 0xFFFFFFFF)	// Verify if element
						break;									//  contents are 0xFFFFFFFF
		if (newIdx >= newEnd)
				return EEPROM_OUT_SIZE;

		oldEnd = oldPage + 4;
		oldIdx = oldPage + (uint32_t)(PageSize - 2);

		for (; oldIdx > oldEnd; oldIdx -= 4)
		{
				address = *(__IO uint16_t*)oldIdx;
				if (address == 0xFFFF || address == SkipAddress)
						continue;						// it's means that power off after write data

				found = 0;
				//check exiting element, this must be newer than the one in the old page
				for (idx = newPage + 6; idx < newIdx; idx += 4)
						if ((*(__IO uint16_t*)(idx)) == address)
						{
								found = 1;
								break;
						}

				if (found)
						continue;

				if (newIdx < newEnd)
				{
						data = (*(__IO uint16_t*)(oldIdx - 2));

						FlashStatus = FLASH_ProgramHalfWord(newIdx, data);
						if (FlashStatus != FLASH_COMPLETE)
								return FlashStatus;

						FlashStatus = FLASH_ProgramHalfWord(newIdx + 2, address);
						if (FlashStatus != FLASH_COMPLETE)
								return FlashStatus;

						newIdx += 4;
				}
				else
						return EEPROM_OUT_SIZE;
		}
		return EEPROM_OK;
}

/**
 * @brief  Verify if active page is full and Writes variable in EEPROM.
 * @param  Address: 16 bit virtual address of the variable
 * @param  Data: 16 bit data to be written as variable value
 * @retval Success or error status:
 *           - FLASH_COMPLETE: on success
 *           - EEPROM_PAGE_FULL: if valid page is full (need page transfer)
 *           - EEPROM_NO_VALID_PAGE: if no valid page was found
 *           - EEPROM_OUT_SIZE: if EEPROM size exceeded
 *           - Flash error code: on write Flash error
 */
uint16_t EEPROMClass::EE_VerifyPageFullWriteVariable(uint16_t Address, uint16_t Data)
{
		FLASH_Status FlashStatus;
		uint32_t idx, curPageEnd, newPage;
		uint16_t mycount;

		// Get the valid Page end Address
		curPageEnd = CurPageBase + PageSize;			// Set end of page

		for (idx = curPageEnd - 2; idx > CurPageBase; idx -= 4)
		{
				if ((*(__IO uint16_t*)idx) == Address)		// Find last value for address
				{
						mycount = (*(__IO uint16_t*)(idx - 2));	// Read last data
						if (mycount == Data)
								return EEPROM_OK;
						if (mycount == 0xFFFF)
						{
								FlashStatus = FLASH_ProgramHalfWord(idx - 2, Data);	// Set variable data
								if (FlashStatus == FLASH_COMPLETE)
										return EEPROM_OK;
						}
						break;
				}
		}

		// Check each active page address starting from begining
		for (idx = CurPageBase + 4; idx < curPageEnd; idx += 4)
				if ((*(__IO uint32_t*)idx) == 0xFFFFFFFF)				// Verify if element 
				{													//  contents are 0xFFFFFFFF
						FlashStatus = FLASH_ProgramHalfWord(idx, Data);	// Set variable data
						if (FlashStatus != FLASH_COMPLETE)
								return FlashStatus;
						FlashStatus = FLASH_ProgramHalfWord(idx + 2, Address);	// Set variable virtual address
						if (FlashStatus != FLASH_COMPLETE)
								return FlashStatus;
						return EEPROM_OK;
				}

		// Empty slot not found, need page transfer
		// Calculate unique variables in page
		mycount = EE_GetVariablesCount(Address) + 1;
		if (mycount >= (PageSize / 4 - 1))
				return EEPROM_OUT_SIZE;

		//printf("copy to next empty page.\n");
		if(CurPageBase < EEPROM_PAGEx_BASE) //empty page available
		{
				newPage = CurPageBase + PageSize;
				FlashStatus = FLASH_ProgramHalfWord(newPage, EEPROM_RECEIVE_DATA);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				// Write the variable passed as parameter in the new active page
				FlashStatus = FLASH_ProgramHalfWord(newPage + 4, Data);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(newPage + 6, Address);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = EE_PageTransfer(newPage, CurPageBase, Address);
				if (FlashStatus != EEPROM_OK)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(CurPageBase, EEPROM_IGNORED_PAGE);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(newPage, EEPROM_VALID_PAGE);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				CurPageBase = newPage;
		}
		else
		{
				//printf("change swap page.\n");
				if(CurSwapPageBase == EEPROM_SWAP_BASE) //should erase swap sector
				{
						if(EEPROM_ERASED != *(__IO uint16_t *)CurSwapPageBase)
						{
								FlashStatus = EE_EraseSector(EEPROM_SWAP_BASE);
								if (FlashStatus != FLASH_COMPLETE)
										return FlashStatus;
						}
				}

				newPage = CurSwapPageBase;
				FlashStatus = FLASH_ProgramHalfWord(newPage, EEPROM_RECEIVE_DATA);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				// Write the variable passed as parameter in the new active page
				FlashStatus = FLASH_ProgramHalfWord(newPage + 4, Data);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(newPage + 6, Address);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = EE_PageTransfer(newPage, CurPageBase, Address);
				if (FlashStatus != EEPROM_OK)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(CurPageBase, EEPROM_IGNORED_PAGE);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(CurSwapPageBase, EEPROM_VALID_PAGE);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				//erase main sector
				FlashStatus = EE_EraseSector(EEPROM_START_ADDRESS);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				newPage = EEPROM_PAGE0_BASE;

				FlashStatus = FLASH_ProgramHalfWord(newPage, EEPROM_RECEIVE_DATA);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = EE_PageTransfer(newPage, CurSwapPageBase, 0xffff);
				if (FlashStatus != EEPROM_OK)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(CurSwapPageBase, EEPROM_IGNORED_PAGE);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				FlashStatus = FLASH_ProgramHalfWord(newPage, EEPROM_VALID_PAGE);
				if (FlashStatus != FLASH_COMPLETE)
						return FlashStatus;

				CurPageBase = newPage;
				CurSwapPageBase += PageSize;
				if (CurSwapPageBase >= EEPROM_SWAP_BASE + EEPROM_SECTOR_SIZE)
						CurSwapPageBase = EEPROM_SWAP_BASE;
		}
		return EEPROM_OK;
}

EEPROMClass::EEPROMClass(void)
{
		//printf("call EEPROM constructor\n");
		//initalized as 0 means invalid value
		CurPageBase = 0;
		CurSwapPageBase = 0;
		PageSize = EEPROM_PAGE_SIZE;
		Status = EEPROM_NOT_INIT;
}

uint16_t EEPROMClass::init(void)
{
		uint32_t pageBase;
		FLASH_Status FlashStatus;

		//printf("EEPROM init...\n");
		FLASH_Unlock();

		CurSwapPageBase = 0;
		Status = EEPROM_NO_VALID_PAGE;

		CurPageBase = EE_FindValidPage(EEPROM_START_ADDRESS);

		if(CurPageBase != 0) //really good
		{
				//printf("found exist main page.\n");
				for (pageBase = EEPROM_SWAP_BASE; pageBase < EEPROM_SWAP_BASE+EEPROM_SECTOR_SIZE; pageBase += PageSize)
				{
						if ( *(__IO uint16_t *)pageBase == EEPROM_ERASED) //perfect
						{
								//printf("found exist swap page.\n");
								CurSwapPageBase = pageBase;
								break;
						}
				}
				if (CurSwapPageBase == 0)
				{
						FlashStatus = EE_EraseSector(EEPROM_SWAP_BASE);
						if (FlashStatus != FLASH_COMPLETE)
							return FlashStatus;
						CurSwapPageBase = EEPROM_SWAP_BASE;
				}
				Status = EEPROM_OK;
		}
		else if(0) //handle error
		{
		}
		else //uninitialized
		{
				//printf("no partions found, now formatting\n");
				format();
				CurPageBase = EEPROM_PAGE0_BASE;
				CurSwapPageBase = EEPROM_SWAP_BASE;
				Status = EEPROM_OK;
		}

		//printf("init done.\nCurrent page base is %x\nCurrent swap page base is %x\n",CurPageBase, CurSwapPageBase);
		return Status;
}

/**
 * @brief  Erases PAGE0 and PAGE1 and writes EEPROM_VALID_PAGE / 0 header to PAGE0
 * @param  PAGE0 and PAGE1 base addresses
 * @retval Status of the last operation (Flash write or erase) done during EEPROM formating
 */
uint16_t EEPROMClass::format(void)
{
		FLASH_Status FlashStatus;

		FLASH_Unlock();
		FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

		//printf("unlock OK.\n");

		FlashStatus = EE_EraseSector(EEPROM_PAGE0_BASE);
		if (FlashStatus != FLASH_COMPLETE)
				return FlashStatus;

		//printf("main sector erased.\n");

		FlashStatus = EE_EraseSector(EEPROM_SWAP_BASE);
		if (FlashStatus != FLASH_COMPLETE)
				return FlashStatus;

		//printf("swap sector erased.\n");

		FlashStatus = FLASH_ProgramHalfWord(EEPROM_START_ADDRESS, EEPROM_VALID_PAGE);
		if (FlashStatus != FLASH_COMPLETE)
				return FlashStatus;

		//printf("set current page valid.\n");

		return EEPROM_OK;
}

/**
 * @brief  Returns the erase counter for current page
 * @param  Data: Global variable contains the read variable value
 * @retval Success or error status:
 *			- EEPROM_OK: if erases counter return.
 *			- EEPROM_NO_VALID_PAGE: if no valid page was found.
 */
uint16_t EEPROMClass::erases(uint16_t *Erases)
{
		if (Status != EEPROM_OK)
				if (init() != EEPROM_OK)
						return Status;

		*Erases = (*(__IO uint16_t*)EEPROM_START_ADDRESS+2);
		return EEPROM_OK;
}

/**
 * @brief	Returns the last stored variable data, if found,
 *			which correspond to the passed virtual address
 * @param  Address: Variable virtual address
 * @retval Data for variable or EEPROM_DEFAULT_DATA, if any errors
 */
uint16_t EEPROMClass::read (uint16_t Address)
{
		uint16_t data;
		read(Address, &data);
		return data;
}

/**
 * @brief	Returns the last stored variable data, if found,
 *			which correspond to the passed virtual address
 * @param  Address: Variable virtual address
 * @param  Data: Pointer to data variable
 * @retval Success or error status:
 *           - EEPROM_OK: if variable was found
 *           - EEPROM_BAD_ADDRESS: if the variable was not found
 *           - EEPROM_NO_VALID_PAGE: if no valid page was found.
 */
uint16_t EEPROMClass::read(uint16_t Address, uint16_t *Data)
{
		uint32_t pageBase, pageEnd;

		// Set default data (empty EEPROM)
		*Data = EEPROM_DEFAULT_DATA;

		if (Status == EEPROM_NOT_INIT)
				if (init() != EEPROM_OK)
						return Status;

		pageBase = CurPageBase;
		// Get the valid Page end Address
		pageEnd = pageBase + ((uint32_t)(PageSize - 2));

		// Check each active page address starting from end
		for (pageBase += 6; pageEnd >= pageBase; pageEnd -= 4)
				if ((*(__IO uint16_t*)pageEnd) == Address)		// Compare the read address with the virtual address
				{
						*Data = (*(__IO uint16_t*)(pageEnd - 2));		// Get content of Address-2 which is variable value
						return EEPROM_OK;
				}

		// Return ReadStatus value: (0: variable exist, 1: variable doesn't exist)
		return EEPROM_BAD_ADDRESS;
}

/**
 * @brief  Writes/upadtes variable data in EEPROM.
 * @param  VirtAddress: Variable virtual address
 * @param  Data: 16 bit data to be written
 * @retval Success or error status:
 *			- FLASH_COMPLETE: on success
 *			- EEPROM_BAD_ADDRESS: if address = 0xFFFF
 *			- EEPROM_PAGE_FULL: if valid page is full
 *			- EEPROM_NO_VALID_PAGE: if no valid page was found
 *			- EEPROM_OUT_SIZE: if no empty EEPROM variables
 *			- Flash error code: on write Flash error
 */
uint16_t EEPROMClass::write(uint16_t Address, uint16_t Data)
{
		if (Status == EEPROM_NOT_INIT)
				if (init() != EEPROM_OK)
						return Status;

		if (Address == 0xFFFF)
				return EEPROM_BAD_ADDRESS;

		// Write the variable virtual address and value in the EEPROM
		uint16_t status = EE_VerifyPageFullWriteVariable(Address, Data);
		return status;
}

/**
 * @brief  Return number of variable
 * @retval Number of variables
 */
uint16_t EEPROMClass::count(uint16_t *Count)
{
		if (Status == EEPROM_NOT_INIT)
				if (init() != EEPROM_OK)
						return Status;

		*Count = EE_GetVariablesCount(0xFFFF);
		return EEPROM_OK;
}

uint16_t EEPROMClass::maxcount(void)
{
		return ((PageSize / 4)-1);
}

//#endif
