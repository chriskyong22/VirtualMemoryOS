#include "my_vm.h"
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>

typedef struct allocationNode {
	void* physicalAddress;
	void* virtualPageEntry;
	struct allocationNode* next;
} allocationNode;

typedef struct allocationLinkedList {
	allocationNode* head;
} allocationLinkedList;

typedef struct tlbNode {
	void* physicalPageAddress;
	unsigned long virtualPageNumber;
	char metadata; // first bit will be valid bit
} tlbNode;

#define VIRTUAL_BITMAP_SIZE ((unsigned long) (ceil((MAX_MEMSIZE)/ ((PGSIZE) * 8.0))))
#define PHYSICAL_BITMAP_SIZE ((unsigned long) (ceil((MEMSIZE) / ((PGSIZE) * 8.0))))
#define ADDRESS_SPACE_BITS (sizeof(void*) * 8)
#define OFFSET_BITS ((unsigned long)log2(PGSIZE))
#define OFFSET_MASK ((1ULL << OFFSET_BITS) - 1)
#define ENTRY_SIZE (sizeof(void*))
#define NUMBER_PAGE_TABLE_ENTRIES ((PGSIZE) / ENTRY_SIZE)
#define VIRTUAL_PAGE_NUMBER_MASK (MAX_VIRTUAL_PAGE_INDEX << OFFSET_BITS)
#define VIRTUAL_PAGE_NUMBER_BITS (ADDRESS_SPACE_BITS - OFFSET_BITS)
#define MAX_VIRTUAL_PAGE_INDEX ((1ULL << VIRTUAL_PAGE_NUMBER_BITS) - 1)
#define MAX_VIRTUAL_ADDRESS (ULONG_MAX)
#define TLB_VALID_BIT_MASK (1ULL << 0)
#define TLB_REFERENCE_BIT_MASK (1ULL << 1)


static int checkValidVirtualPageNumber(unsigned long virtualPageNumber);
static int checkValidVirtualAddress(void* virtualPageAddress);
static void toggleBitVirtualBitmapVA(void* virtualPageAddress);
static void toggleBitVirtualBitmapPN(unsigned long pageNumber);
static void toggleBitPhysicalBitmapPN (unsigned long pageNumber);
static void toggleBitPhysicalBitmapPA (void* physicalPageAddress);
static void* getVirtualPageAddress(unsigned long pageNumber);
static unsigned long getVirtualPageNumber(void* virtualPageAddress);
static void* getPhysicalPageAddress(unsigned long pageNumber);
static unsigned long getPhysicalPageNumber(void* physicalPageAddress);
static void zeroOutPhysicalPage(void* physicalPageAddress);
void *get_next_avail(int num_pages);
void* get_next_physicalavail();
static void insert(allocationLinkedList* list, void* pageEntry, void* physicalAddress);
static void freeAllocationLinkedList(allocationLinkedList* list);
static void toggleAllocationLinkedList(allocationLinkedList* list);
static int checkIfFirstVirtualPageIsAllocated();
static int checkAllocatedPhysicalBitmapPA(void* physicalPageAddress);
static int checkAllocatedPhysicalBitmapPN(unsigned long pageNumber); 
static int checkAllocatedVirtualBitmapVA(void* virtualPageAddress);
static int checkAllocatedVirtualBitmapPN(unsigned long pageNumber);
static void createTLB();

pthread_mutex_t mallocLock = PTHREAD_MUTEX_INITIALIZER;

char* physicalMemoryBase = NULL;
char* physicalBitmap = NULL;
char* virtualBitmap = NULL;
unsigned long* pageDirectoryBase = NULL;
unsigned long tlbMiss = 0;
unsigned long tlbHit = 0;
tlbNode* tlbBaseAddress = NULL;
/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {
	
    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    physicalMemoryBase = calloc(MEMSIZE, sizeof(char));
    if (physicalMemoryBase == NULL) {
    	exit(-1);
    }
	//HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
	
	// Character is 1 byte and 1 byte = 8 bits 
	// therefore we can store 8 pages per character.
	// # of Pages (Bytes) / Page Size (Bytes) = Number of Pages Required in Bytes 
	// # of Pages in Bytes / 8 = Number of Pages Required in Bits
	// SOLVED: pages sizes that are not multiple of 8 round to nearest byte via ceil
	
	// If there is a fractional part and had to be rounded to allocate the
	// last pages, then we must set the pages in the last char that should not exist
	// to 1 to indicate it's already in use and it cannot be used for allocation
	physicalBitmap = calloc(PHYSICAL_BITMAP_SIZE, sizeof(char));
	if (physicalBitmap == NULL) {
		exit(-1);
	}
	
	int bitMask = (1 << 8) - 1;
	
	// Means the number of pages in the physical space is not a multiple of 8 
	// and thus we will have extra bits in the last char that should not be used.
	if (((MEMSIZE) / (PGSIZE)) % 8 != 0) {
		// (MEMSIZE) / PGSIZE will give us the number of pages that 
		// we need to account for. We % 8 to see the pages that require an extra
		// char but we should not use the full char (basically the number of bits
		// that should be set to 0 in the last char)
		int validBits = ((MEMSIZE/PGSIZE) % 8);
		int validBitsMask = (1 << validBits) - 1;
		char setMask = bitMask ^ validBitsMask;
		*(physicalBitmap + (PHYSICAL_BITMAP_SIZE - 1)) = setMask;
	} 
	
	//printf("Number of physical pages is %lu\n", PHYSICAL_BITMAP_SIZE * 8);
	virtualBitmap = calloc(VIRTUAL_BITMAP_SIZE, sizeof(char));
	if (virtualBitmap == NULL) {
		exit(-1);
	}
	
	if (((MAX_MEMSIZE) / (PGSIZE)) % 8 != 0) {
		// (MAX_MEMSIZE) / (PGSIZE) will give us the number of pages that 
		// we need to account for. We % 8 to see the pages that require an extra
		// char but we should not use the full char (basically the number of bits
		// that should be set to 0 in the last char)
		int validBits = (((MAX_MEMSIZE) / (PGSIZE)) % 8);
		int validBitsMask = (1 << validBits) - 1;
		char setMask = bitMask ^ validBitsMask;
		*(virtualBitmap + (VIRTUAL_BITMAP_SIZE - 1)) = setMask;
	}
	
	// The first page will never be used (because then the user cannot tell if 
	// a_malloc failed since 0x0 address would represent the first page OR NULL.
	(*virtualBitmap) |= 1;
}

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
	
	unsigned int virtualPageBits = VIRTUAL_PAGE_NUMBER_BITS;
	
	// Page Table Entries = Page Table Size / Entry Size 
	// Page Table Bits = log2(Page Table Entries)
	unsigned int pageTableBits = (int)log2((PGSIZE / ENTRY_SIZE));
	unsigned int pageTableLevels = (ADDRESS_SPACE_BITS) / 16.0;
	
	// Check if we have to split the address and have internal fragmentation
	// so we can guarantee there will be pageTableLevels always.
	if (pageTableLevels * pageTableBits > virtualPageBits) {
		if (virtualPageBits <= pageTableBits || 
				pageTableBits * (pageTableLevels - 1) >= virtualPageBits) {
			pageTableBits = ceil(virtualPageBits / ((double)pageTableLevels));
		}
	}
	
	// Find the page directory bits (since page directory bits are not always 
	// equal to other page table bits)
	while (pageTableLevels > 1) {
		virtualPageBits -= pageTableBits;
		pageTableLevels--;
	}
	
	//printf("The number of bits used for page directory is %u. The number of bits used for all the other pages is %u\n", virtualPageBits, pageTableBits);

	// To create Page Table Mask:
	// Step 1) Prune off all bits that are not the desired page table bits 
	// Step 2) Shift the number of bits pruned off to get the value with only 
	//		   the page table bits set.
	unsigned int usedTopBits = virtualPageBits;
	unsigned long pageTableMask = (MAX_VIRTUAL_ADDRESS >> (ADDRESS_SPACE_BITS - usedTopBits)) << (ADDRESS_SPACE_BITS - usedTopBits);
	pageTableLevels = (ADDRESS_SPACE_BITS) / 16.0;
	unsigned long* nextAddress = pgdir;
	while (pageTableLevels != 0) {
		// Mask with the Virtual Address (Assuming VA is unsigned long*) 
		unsigned long pageTableIndex = ((unsigned long)va) & pageTableMask;
		pageTableIndex >>= (ADDRESS_SPACE_BITS - usedTopBits);
		// Access the Index in the Page Table via: 
		// Page Table Base Address + (sizeof(page table entry) * pageTableIndex)
		// Assuming since nextAddress is unsigned long*, the sizeof unsigned long
		// will be 4 bytes or 8 bytes depending on 32/64 bit, safer approach is
		// ((char*)nextAddress + (ENTRY_SIZE * pageTableIndex))
		nextAddress = (unsigned long*) *(nextAddress + pageTableIndex);
		if (nextAddress == NULL) {
			write(2, "INVALID TRANSLATE ACCESS\n", sizeof("INVALID TRANSLATE ACCESS\n"));
			return NULL;
		}
		pageTableLevels--;
		// Creating the new Page Table Mask:
		// Step 1) Store an inverted version of the old Page Table Mask to be used to zero out the Top Bits 
		// Step 2) Create a Mask that includes the old Page Table Bits and the newly desired Page Table Bits (zeroing out the non-page table bits)
		// Step 3) AND the new Mask with the inverted mask to zero out the top bits that were already used
		unsigned long inverseMask = ~pageTableMask;
		usedTopBits += pageTableBits;
		pageTableMask = (MAX_VIRTUAL_ADDRESS >> (ADDRESS_SPACE_BITS - usedTopBits)) << (ADDRESS_SPACE_BITS - usedTopBits);
		pageTableMask &= inverseMask;
	}
	// Have to add the offset to the get the desired physical address
	unsigned long offset = ((unsigned long)va) & (OFFSET_MASK);
	return (void*) (((char*) nextAddress) + offset);
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t *pgdir, void *va, void *pa) {

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
	printf("Mapping %lu VA to %lu PA\n", ((unsigned long)va), ((unsigned long)pa));
	
	unsigned int virtualPageBits = VIRTUAL_PAGE_NUMBER_BITS;
	
	// Page Table Entries = Page Table Size / Entry Size 
	// Page Table Bits = log2(Page Table Entries)
	unsigned int pageTableBits = (int)log2((PGSIZE / sizeof(void*)));
	unsigned int pageTableLevels = (ADDRESS_SPACE_BITS) / 16.0;
	
	// Check if we have to split the address and have internal fragmentation
	// so we can guarantee there will be pageTableLevels always.
	if (pageTableLevels * pageTableBits > virtualPageBits) {
		if (virtualPageBits <= pageTableBits || 
				pageTableBits * (pageTableLevels - 1) >= virtualPageBits) {
			pageTableBits = ceil(virtualPageBits / ((double)pageTableLevels));
		} 
	} 
	
	// Find the page directory bits (since page directory bits are not always 
	// equal to other page table bits)
	while (pageTableLevels > 1) {
		virtualPageBits -= pageTableBits;
		pageTableLevels--;
	}
	
	//printf("The number of bits used for page directory is %u. The number of bits used for all the other pages is %u\n", virtualPageBits, pageTableBits);

	// To create Page Table Mask:
	// Step 1) Prune off all bits that are not the desired page directory bits 
	// Step 2) Shift the number of bits pruned off to get the value with only 
	//		   the page directory bits set.
	unsigned int usedTopBits = virtualPageBits;
	unsigned long pageTableMask = (MAX_VIRTUAL_ADDRESS >> (ADDRESS_SPACE_BITS - usedTopBits)) << (ADDRESS_SPACE_BITS - usedTopBits);
	
	allocationLinkedList* allocation = NULL;
	pageTableLevels = (ADDRESS_SPACE_BITS) / 16.0;
	unsigned long* nextAddress = pgdir;
	while (pageTableLevels != 0) {
		pageTableLevels--;
		// Mask with the Virtual Address (Assuming VA is unsigned long*) 
		unsigned long pageTableIndex = ((unsigned long)va) & pageTableMask;
		pageTableIndex >>= (ADDRESS_SPACE_BITS - usedTopBits);
		
		// Access the Index in the Page Table via: 
		// Page Table Base Address + (sizeof(page table entry) * pageTableIndex)
		// Assuming since nextAddress is unsigned long*, the sizeof unsigned long
		// will be 4 bytes or 8 bytes depending on 32/64 bit, safer approach is
		// ((char*)nextAddress + (ENTRY_SIZE * pageTableIndex))
		printf("The index in the %u page table is %lu\n", pageTableLevels + 1, pageTableIndex);
		
		void* holdNextAddress = (nextAddress + pageTableIndex);
		nextAddress = (unsigned long*) *(nextAddress + pageTableIndex); 
		// printf("The address stored in the entry is %lu\n", (unsigned long) nextAddress);
		
		// Checking if it is the last-level page table or if we have to 
		// allocate a new page table for the mapping to valid.
		if (nextAddress == NULL || pageTableLevels == 0) {
			if (pageTableLevels == 0) {
				// Currently assuming the passed-in PA was checked to be a free page
				// already and we just overwrite the stored PA (if there is one already stored)
				// in the last-level page table.
				printf("Setting the address of the physical in the last-level page table entry %lu\n", (unsigned long)pa);
				*((unsigned long*)holdNextAddress) = (unsigned long)pa;
				freeAllocationLinkedList(allocation);
				return 1;
			} else {
				printf("Allocating a new %d-level page table\n", pageTableLevels + 1);
				void* physicalPageAddress = get_next_physicalavail();
				if (physicalPageAddress == NULL) {
					// Need to free page tables that were allocated for this mapping 
					// because we ran out of physical pages to store the new 
					// page tables needed for the mapping.
					// Therefore we free all the previous page tables we stored 
					// for this translation and reset the entries in the physical bitmap
					write(2, "[D]: Ran out of physical pages in mapping\n", sizeof("[D]: Ran out of physical pages in mapping\n"));
					toggleAllocationLinkedList(allocation);
					freeAllocationLinkedList(allocation);
					return -1;
				}
				*((unsigned long*)holdNextAddress) = (unsigned long) physicalPageAddress;
				nextAddress = physicalPageAddress;
				printf("Storing a new page table address in current page table: %lu\n", (unsigned long) physicalPageAddress);
				// Using lazy malloc therefore we only zero out the pages when it
				// is allocated, aka there was old data and we just leave it till 
				// the physical page is in use.
				zeroOutPhysicalPage(physicalPageAddress);
				toggleBitPhysicalBitmapPA(physicalPageAddress);
				if (allocation == NULL) {
					allocation = malloc(sizeof(allocationLinkedList));
					allocation->head = NULL;
				}
				insert(allocation, (void*) holdNextAddress, physicalPageAddress);
			}
		}
		
		// Creating the new Page Table Mask:
		// Step 1) Store an inverted version of the old Page Table Mask to be used to zero out the Top Bits 
		// Step 2) Create a Mask that includes the old Page Table Bits and the newly desired Page Table Bits (zeroing out the non-page table bits)
		// Step 3) AND the new Mask with the inverted mask to zero out the top bits that were already used
		unsigned long inverseMask = ~pageTableMask;
		usedTopBits += pageTableBits;
		pageTableMask = (MAX_VIRTUAL_ADDRESS >> (ADDRESS_SPACE_BITS - usedTopBits)) << (ADDRESS_SPACE_BITS - usedTopBits);
		pageTableMask &= inverseMask;
	}
	freeAllocationLinkedList(allocation);
    return -1;
}

/*Function that gets the next available page
 Shouldn't this be unsigned long to account for unsigned long?
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
    
    //Changing to find the next free page(s) instead of singular page
    
    // In our Bitmap, each char is 8 pages therefore to check if there is a page
	// free in a char, we mask it with 0b11111111 or 255. 
	
	//printf("Looking for %d pages\n", num_pages);
	const int CHAR_IN_BITS = sizeof(char) * 8;
	int foundStartContinous = 0;
	unsigned long startOfContinousPages = -1;
	unsigned long foundContinousPages = 0;
	const int PAGE_MASK = (1 << CHAR_IN_BITS) - 1; 
    for (unsigned long pages = 0; pages < VIRTUAL_BITMAP_SIZE; pages++) {
    	char* pagesLocation = virtualBitmap + pages;
    	if (((*pagesLocation) & PAGE_MASK) != PAGE_MASK) {
    		for(int bitIndex = 0; bitIndex < CHAR_IN_BITS; bitIndex++) {
    			int bitMask = 1 << bitIndex;
    			if (((*pagesLocation) & bitMask) == 0) {
    				if (foundStartContinous == 0) {
    					foundStartContinous = 1;
    					startOfContinousPages = (pages * 8) + bitIndex;
    					//printf("Starting Continous Page Number %lu\n", startOfContinousPages);
    				}
    				foundContinousPages++;
    				if (foundContinousPages == num_pages) {
    					return getVirtualPageAddress(startOfContinousPages);
    				}
    			} else {
    				foundStartContinous = 0;
    				foundContinousPages = 0;
    			}
    		}
    	}
    }
    
    // Could not find a continous section in virtual bitmap that could
    // hold num_pages continous pages.
    return NULL;
}

void* get_next_physicalavail() {
	// In our Bitmap, each char is 8 pages therefore to check if there is a page
	// free in a char, we mask it with 0b11111111 or 255. 
	const int PAGE_MASK = (1 << (sizeof(char) * 8)) - 1; 
	const int CHAR_IN_BITS = sizeof(char) * 8;
	for(unsigned long pages = 0; pages < PHYSICAL_BITMAP_SIZE; pages++) {
		char* pagesLocation = (physicalBitmap + pages);
		// For each char, mask it to see if there is a free page within the char
		// if there is a free page within a char, the char will not equal 255. 
		if (((*pagesLocation) & PAGE_MASK) != PAGE_MASK) {
			for(int bitIndex = 0; bitIndex < CHAR_IN_BITS; bitIndex++) {
			/*
				bitMask values ~ 0b1 = 1, 0b10 = 2, 0b100 = 4, 0b1000 = 8
				0b10000 = 16, 0b100000 = 32, 0b1000000 = 64, 0b10000000 = 128
			*/
				int bitMask = 1 << bitIndex;
				if(((*pagesLocation) & bitMask) == 0) {
					// The Page Number is (pages * 8) + bitIndex.
					// Since each pages hold 8 pages and within 8 pages, the 
					// bitIndex indicates a page within a char.
					unsigned long pageNumber = (pages * 8) + bitIndex;
					return (void*) (physicalMemoryBase + (pageNumber * PGSIZE));
				}
			}
		}
	}
	
    // Could not find a free page in physical bitmap
	return NULL;
}


//I Moved these 3 TLB METHODS To here, not sure why it was above part 1 stuff

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
void add_TLB(void *va, void *pa) {

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
	unsigned long virtualPageNumber = getVirtualPageNumber(va);
	unsigned long tlbIndex = virtualPageNumber % TLB_ENTRIES;
	tlbNode* currentTLBEntry = (tlbBaseAddress + tlbIndex);
	currentTLBEntry->virtualPageNumber = virtualPageNumber;
	
	// Remove offset from physical address
	unsigned long physicalPage = getPhysicalPageNumber(pa);
	pa = getPhysicalPageAddress(physicalPage);
	
	currentTLBEntry->physicalPageAddress = pa;
	currentTLBEntry->metadata |= TLB_VALID_BIT_MASK;
	tlbMiss++;
	//printf("Added TLB ENTRY: %llu bit set, virtualPageNumber %lu\n", (currentTLBEntry->metadata & TLB_VALID_BIT_MASK), currentTLBEntry->virtualPageNumber);
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
void* check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    unsigned long virtualPageNumber = getVirtualPageNumber(va);
	unsigned long tlbIndex = virtualPageNumber % TLB_ENTRIES;
	tlbNode* currentTLBEntry = (tlbBaseAddress + tlbIndex);
	
	//printf("Current TLB ENTRY: %llu bit set, virtualPageNumber %lu, %lu\n", (currentTLBEntry->metadata & TLB_VALID_BIT_MASK), currentTLBEntry->virtualPageNumber, virtualPageNumber);
	if ((currentTLBEntry->metadata & TLB_VALID_BIT_MASK) == 1 && currentTLBEntry->virtualPageNumber == virtualPageNumber) {
		tlbHit++;
		unsigned long offset = ((unsigned long)va) & OFFSET_MASK;
		return ((char*)currentTLBEntry->physicalPageAddress) + offset;
	}
	return NULL;
}

/*
	Custom removal of TLB entry
*/
void remove_TLB(void *va) {
	unsigned long virtualPageNumber = getVirtualPageNumber(va);
	unsigned long tlbIndex = virtualPageNumber % TLB_ENTRIES;
	tlbNode* currentTLBEntry = (tlbBaseAddress + tlbIndex);
	
	if ((currentTLBEntry->metadata & TLB_VALID_BIT_MASK) == 1 && currentTLBEntry->virtualPageNumber == virtualPageNumber) {
		currentTLBEntry->metadata &= (((1ULL << 8) - 1) ^ TLB_VALID_BIT_MASK);
	}
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate() {
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    fprintf(stderr, "Misses %lu, Hits %lu\n", tlbMiss, tlbHit);
    if (tlbHit != 0 || tlbMiss != 0) { 
    	miss_rate = (((double)tlbMiss) / (tlbHit + tlbMiss));
    }
    fprintf(stderr, "TLB miss rate %.17g \n", (miss_rate * 100.0));
}

/* Function responsible for allocating pages
and used by the benchmark
*/
void *a_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */
     
	/* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */
    
	pthread_mutex_lock(&mallocLock);
	if (physicalMemoryBase == NULL) {
		set_physical_mem();
		createTLB();
	}
	
	if (pageDirectoryBase == NULL) {
		
		unsigned int virtualPageBits = VIRTUAL_PAGE_NUMBER_BITS;
		// Page Table Entries = Page Table Size / Entry Size 
		// Page Table Bits = log2(Page Table Entries)
		unsigned int pageTableBits = (int)log2((PGSIZE / sizeof(void*)));
		unsigned int pageTableLevels = (ADDRESS_SPACE_BITS) / 16.0;
		if (pageTableLevels * pageTableBits > virtualPageBits) {
			if (virtualPageBits <= pageTableBits || 
					pageTableBits * (pageTableLevels - 1) >= virtualPageBits) {
				pageTableBits = ceil(virtualPageBits / ((double)pageTableLevels));
			}
		}
		 
		while (pageTableLevels > 1) {
			virtualPageBits -= pageTableBits;
			pageTableLevels--;
		}
		printf("The number of bits used for page directory is %u. The number of bits used for all the other pages is %u\n", virtualPageBits, pageTableBits);
		
		// Number of entries in page directory = 2^(page directory bits)
		// Number of bytes required for page directory = Number of entries in page directory * entry size
		// Number of Physical Pages required for page directory = ceil(Number of bytes required for page directory / PGSIZE)
		unsigned long numberOfContinousPhysicalPages = (unsigned long) ceil(((1 << virtualPageBits) * ENTRY_SIZE) / ((double)(PGSIZE)));
		if (virtualPageBits == 0) {
			numberOfContinousPhysicalPages = 0;
		}
		
		printf("The number of pages to allocate continously is %lu for page directory\n", numberOfContinousPhysicalPages);
		while (numberOfContinousPhysicalPages != 0) { 
			void* physicalAddress = get_next_physicalavail();
			if (pageDirectoryBase == NULL) {
				pageDirectoryBase = physicalAddress;
			}
			
			if (physicalAddress == NULL) { 
				write(2, "Could not allocate page directory!\n", sizeof("Could not allocate page directory!\n"));
				exit(-1);
			}
			toggleBitPhysicalBitmapPA(physicalAddress);
			numberOfContinousPhysicalPages--;
		}

	}
	
	unsigned long numberOfPagesToAllocate = (unsigned long) ceil((double)num_bytes / PGSIZE);
	printf("%u bytes require %lu pages\n", num_bytes, numberOfPagesToAllocate);
	
	// Find a spot in the virtual bitmap where we can allocate numberOfPagesToAllocate
	// continous pages. 
	void* startingVirtualPageAddress = get_next_avail(numberOfPagesToAllocate);
	
	// Note since virtual address cannot start with 0x0 or NULL, then if 
	// startingVirtualPageAddress returns NULL, we know it could not find a spot
	// to allocate the continous number of pages.
	if (startingVirtualPageAddress == NULL) {
		write(2, "[D] Could not allocate the continous virtual pages!\n", sizeof("[D] Could not allocate the continous virtual pages!\n"));
		pthread_mutex_unlock(&mallocLock);
		return NULL;
	}
	

	void** physicalAddresses = malloc(sizeof(void*) * numberOfPagesToAllocate);
	if (physicalAddresses == NULL) {
		write(2, "[D] Could not allocate the physical addresses metadata!\n", sizeof("[D] Could not allocate the physical addresses metadata!\n"));
		pthread_mutex_unlock(&mallocLock);
		return NULL;
	} 
	
	// Check to see if there is enough physical pages to allocate for the num_bytes
	// If not enough pages, return NULL and before returning, ensure the physical
	// pages allocated is set back to free in the physical bitmap. 
	// Note: Physical Pages do not need to be continous
	for (unsigned long pageIndex = 0; pageIndex < numberOfPagesToAllocate; pageIndex++) {
		void* physicalAddress = get_next_physicalavail();
		if (physicalAddress == NULL) { 
			for (unsigned long index = 0; index < pageIndex; index++) {
				toggleBitPhysicalBitmapPA(physicalAddresses[pageIndex]);
			}
			pthread_mutex_unlock(&mallocLock);
			free(physicalAddresses);
			write(2, "[D] Failed to allocate physical pages\n", sizeof("[D] Failed to allocate physical pages\n"));
			return NULL;
		} else {
			physicalAddresses[pageIndex] = physicalAddress;
			toggleBitPhysicalBitmapPA(physicalAddresses[pageIndex]);
		}
	}
	
	unsigned long virtualPageNumber = getVirtualPageNumber(startingVirtualPageAddress);
	printf("Starting Virtual Page Number %ld\n", virtualPageNumber);
	// At this point, we found enough physical and virtual pages to allocate 
	// We must now map each virtual page to the physical page now and zero out 
	// the physical pages and mark the virtual pages as allocated.
	for (unsigned long pageIndex = 0; pageIndex < numberOfPagesToAllocate; pageIndex++, virtualPageNumber++) {
		void* virtualPageAddress = getVirtualPageAddress(virtualPageNumber);
		toggleBitVirtualBitmapPN(virtualPageNumber);
		//add_TLB(virtualPageAddress, physicalAddresses[pageIndex]);
		if (page_map(pageDirectoryBase, virtualPageAddress, physicalAddresses[pageIndex]) == -1) {
			// Failed to map, reverting all changes to virtual bitmap and physical bitmap. 
			write(2, "Failed to map the virtual to physical!\n", sizeof("Failed to map the virtual to physical!\n"));
			virtualPageNumber = getVirtualPageNumber(startingVirtualPageAddress);
			for (unsigned long index = 0; index <= pageIndex; index++, virtualPageNumber++) {
				//remove_TLB(getVirtualPageAddress(virtualPageNumber));
				toggleBitVirtualBitmapPN(virtualPageNumber);
			}
			for (pageIndex = 0; pageIndex < numberOfPagesToAllocate; pageIndex++) {
				toggleBitPhysicalBitmapPA(physicalAddresses[pageIndex]);
			}
			pthread_mutex_unlock(&mallocLock);
			free(physicalAddresses);
			return NULL;
		}
		zeroOutPhysicalPage(physicalAddresses[pageIndex]);
	}
	
	pthread_mutex_unlock(&mallocLock);
	free(physicalAddresses);
    return startingVirtualPageAddress;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {
	
    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
     
     // Note, our first page should never be allocated therefore if va = 0x0 or 
     // the address of the 1st page, it should not be freed otherwise in 
     // a_malloc, the user cannot tell if the address returned is the 0x0 address 
     // or failed to allocate.
     unsigned long virtualPageNumber = getVirtualPageNumber(va);
     if (virtualPageNumber == 0 || checkValidVirtualAddress((void*) ((char*) va + (sizeof(char) * size))) == -1) {
     	return;
     }
     
     unsigned long numberOfPagesToFree = (unsigned long) ceil((double)size / PGSIZE);
     pthread_mutex_lock(&mallocLock);

     // Safety check to see if the pages are actually allocated
     for(unsigned long pageIndex = 0; pageIndex < numberOfPagesToFree; pageIndex++, virtualPageNumber++) {
     	if(checkAllocatedVirtualBitmapPN(virtualPageNumber) == 0) {
     		pthread_mutex_unlock(&mallocLock);
     		return;
     	}
     }
     // For each virtual page number, find the associated physical address and 
     // set the bit maps accordingly. (Performing lazy free)
     
     // Do I have to reset the entries as well or since our bitmap controls 
     // which pages are free and which are not then we do not need to reset the 
     // entry since technically the VA should not be accessed again unless the user 
     // is malicious and inserts a freed VA or if they do get_value/put_value, 
     // they put a size greater than the a_malloc size they gave.
     // To solve this: we would have to do a traversal of the page table to the 
     // last page table and then set the entry to NULL (note if we were to reset every entry
     // per page table, we would have to check if the page table has other entries still in use
     // This wouldn't be too hard, we would have to create a mask of PGSIZE and isolate the 
     // page table the entry is in and & it with the mask to see if there is any 
     // entries set, if it is 0 after BITWISE ANDing, then we know the page table 
     // can be dealloced and the physical page can be freed (repeat for each level of page table except page directory)  
     virtualPageNumber = getVirtualPageNumber(va);
     for(unsigned long pageIndex = 0; pageIndex < numberOfPagesToFree; pageIndex++, virtualPageNumber++) {
     	void* virtualPageAddress = getVirtualPageAddress(virtualPageNumber);
     	void* physicalAddress = check_TLB(virtualPageAddress);
     	if (physicalAddress == NULL) {
     		physicalAddress = translate(pageDirectoryBase, virtualPageAddress);
     		tlbMiss++;
     	} else { 
     		remove_TLB(virtualPageAddress);
     	}
     	toggleBitPhysicalBitmapPA(physicalAddress);
     	toggleBitVirtualBitmapPN(virtualPageNumber);
     }
     
     pthread_mutex_unlock(&mallocLock);
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
    unsigned long virtualPageNumber = getVirtualPageNumber(va);
    if(virtualPageNumber == 0) {
    	return;
    }
    	
    unsigned long offset = ((unsigned long) va) & OFFSET_MASK;
    unsigned long numberOfPagesToAllocate = 0;
    // Check if the size to put is greater than the available space in the given
    // page
    if (size > ((PGSIZE) - offset)) { 
    	unsigned long remainingSize = size - ((PGSIZE) - offset);
    	numberOfPagesToAllocate = (unsigned long) ceil((double)remainingSize / (PGSIZE));
    } 
	unsigned long copied = 0;
	unsigned long copy = size > ((PGSIZE) - offset) ? ((PGSIZE) - offset) : size;
	pthread_mutex_lock(&mallocLock);
	// Safety check to see if the pages are actually allocated
	for(unsigned long pageIndex = 0; pageIndex <= numberOfPagesToAllocate; pageIndex++, virtualPageNumber++) {
     	if (checkAllocatedVirtualBitmapPN(virtualPageNumber) == 0) {
     		pthread_mutex_unlock(&mallocLock);
     		return;
    	}
    }
    
   	void* physicalAddress = check_TLB(va);
   	if (physicalAddress == NULL) {
   		physicalAddress = translate(pageDirectoryBase, va);
 		if (physicalAddress != NULL) { 
 			add_TLB(va, physicalAddress);
 		} else { 
 			write(2, "[E]: Translation failed but virtual bitmap said page was allocated\n", sizeof("[E]: Translation failed but virtual bitmap said page was allocated\n")); 
 		}
   	}
   	
   	unsigned long physicaloffset = (unsigned long) (((char*)physicalAddress - (char*)physicalMemoryBase) % PGSIZE);
   	if(offset != physicaloffset) { 
   		//printf("Physical Page %lu, starting physical address %lu, base address %lu\n", physicalPage, (unsigned long) startPhysicalAddress, (unsigned long) physicalMemoryBase);
   		//unsigned long physicalPage = getPhysicalPageNumber(physicalAddress);
   		//void* startPhysicalAddress = getPhysicalPageAddress(physicalPage);
   		printf("Virtual address %lu, Physical Address %lu\n", (unsigned long) va, (unsigned long)physicalAddress);
   		//printf("Physical Offset %lu, Virtual Offset %llu\n", offset, ((unsigned long)physicalAddress) & OFFSET_MASK);
   	}
   	   	
   	memcpy(physicalAddress, ((char*)val) + copied, sizeof(char) * copy);
   	size -= copy;
   	copied += sizeof(char) * copy;
   	copy = size < (PGSIZE) ? size : (PGSIZE);
   
	virtualPageNumber = getVirtualPageNumber(va) + 1;
	for(unsigned long pageIndex = 0; pageIndex < numberOfPagesToAllocate && size > 0; pageIndex++, virtualPageNumber++) { 
     	void* virtualPageAddress = getVirtualPageAddress(virtualPageNumber);
		void* physicalAddress = check_TLB(virtualPageAddress);
     	if (physicalAddress == NULL) {
     		physicalAddress = translate(pageDirectoryBase, virtualPageAddress);
     		if (physicalAddress != NULL) { 
     			add_TLB(virtualPageAddress, physicalAddress);
     		} else { 
     			write(2, "[E]: Translation failed but virtual bitmap said page was allocated\n", sizeof("[E]: Translation failed but virtual bitmap said page was allocated\n")); 
     		}
     	}
     	memcpy(physicalAddress, ((char*)val) + copied, sizeof(char) * copy);
     	size -= copy;
     	copied += sizeof(char) * copy;
     	copy = size < (PGSIZE) ? size : (PGSIZE);
    }
	pthread_mutex_unlock(&mallocLock);
}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    unsigned long virtualPageNumber = getVirtualPageNumber(va);
    if(virtualPageNumber == 0) {
    	return;
    }
    
    unsigned long offset = ((unsigned long) va) & OFFSET_MASK;
    unsigned long numberOfPagesToAllocate = 0;
    
    // Check if the size to get is greater than the available space in the given
    // page
    if (size > ((PGSIZE) - offset)) { 
    	unsigned long remainingSize = size - ((PGSIZE) - offset);
    	numberOfPagesToAllocate = (unsigned long) ceil((double)remainingSize / (PGSIZE));
    } 
	unsigned long copied = 0;
	unsigned long copy = size > ((PGSIZE) - offset) ? ((PGSIZE) - offset) : size;

	pthread_mutex_lock(&mallocLock);
	// Safety check to see if the pages are actually allocated
	for(unsigned long pageIndex = 0; pageIndex <= numberOfPagesToAllocate; pageIndex++, virtualPageNumber++) {
     	if(checkAllocatedVirtualBitmapPN(virtualPageNumber) == 0) {
     		pthread_mutex_unlock(&mallocLock);
     		return;
    	}
    }
    
   	void* physicalAddress = check_TLB(va);
   	if (physicalAddress == NULL) {
   		physicalAddress = translate(pageDirectoryBase, va);
 		if (physicalAddress != NULL) { 
 			add_TLB(va, physicalAddress);
 		} else { 
 			write(2, "[E]: Translation failed but virtual bitmap said page was allocated\n", sizeof("[E]: Translation failed but virtual bitmap said page was allocated\n")); 
 		}
   	}
   	
   	unsigned long physicaloffset = (unsigned long) (((char*)physicalAddress - (char*)physicalMemoryBase) % PGSIZE);
   	if(offset != physicaloffset) { 
   		//printf("Physical Page %lu, starting physical address %lu, base address %lu\n", physicalPage, (unsigned long) startPhysicalAddress, (unsigned long) physicalMemoryBase);
   		//unsigned long physicalPage = getPhysicalPageNumber(physicalAddress);
   		//void* startPhysicalAddress = getPhysicalPageAddress(physicalPage);
   		printf("Virtual address %lu, Physical Address %lu\n", (unsigned long) va, (unsigned long)physicalAddress);
   		//printf("Physical Offset %lu, Virtual Offset %llu\n", offset, ((unsigned long)physicalAddress) & OFFSET_MASK);
   	}
   	
   	memcpy(((char*)val) + copied, physicalAddress, sizeof(char) * copy);
   	size -= copy;
   	copied += sizeof(char) * copy;
   	copy = size < (PGSIZE) ? size : (PGSIZE);
   
	virtualPageNumber = getVirtualPageNumber(va) + 1;
	for(unsigned long pageIndex = 0; pageIndex < numberOfPagesToAllocate && size > 0; pageIndex++, virtualPageNumber++) { 
     	void* virtualPageAddress = getVirtualPageAddress(virtualPageNumber);
		void* physicalAddress = check_TLB(virtualPageAddress);
     	if (physicalAddress == NULL) {
     		physicalAddress = translate(pageDirectoryBase, virtualPageAddress);
     		if (physicalAddress != NULL) { 
     			add_TLB(virtualPageAddress, physicalAddress);
     		} else { 
     			write(2, "[E]: Translation failed but virtual bitmap said page was allocated\n", sizeof("[E]: Translation failed but virtual bitmap said page was allocated\n")); 
     		}
     	}
     	memcpy(((char*)val) + copied, physicalAddress, sizeof(char) * copy);
     	size -= copy;
     	copied += sizeof(char) * copy;
     	copy = size < (PGSIZE) ? size : (PGSIZE);
    }
	pthread_mutex_unlock(&mallocLock);
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    unsigned long address_a = 0, address_b = 0;
    unsigned long address_c = 0;
    int x = 0, y = 0;
	for (int i = 0; i < size; i++) {
    	for (int j = 0; j < size; j++) {
    		int z = 0;
    		for(int temp = 0; temp < size; temp++) {
    			address_a = ((unsigned long)mat1) + ((i * size * sizeof(int))) + (temp * sizeof(int));
		    	address_b = ((unsigned long)mat2) + ((temp * size * sizeof(int))) + (j * sizeof(int));
		    	get_value((void *)address_a, &x, sizeof(int));
		    	get_value((void *)address_b, &y, sizeof(int));
		    	z += (x * y);
    		}
    		address_c = ((unsigned long)answer) + ((i * size * sizeof(int))) + (j * sizeof(int));
        	put_value((void *)address_c, &z, sizeof(int));
        }
    }
}

/*
Helper Functions
*/

static unsigned long getPhysicalPageNumber(void* physicalPageAddress) {
	unsigned long pageNumber = (unsigned long) (((char*)physicalPageAddress - (char*)physicalMemoryBase) / PGSIZE);
	return pageNumber;
}

static void* getPhysicalPageAddress(unsigned long pageNumber) {
	return (void*) (physicalMemoryBase + (pageNumber * PGSIZE));
}

static unsigned long getVirtualPageNumber(void* virtualPageAddress) {
	unsigned long pageNumber = ((unsigned long)virtualPageAddress) >> OFFSET_BITS;
	return pageNumber;
}

static void* getVirtualPageAddress(unsigned long pageNumber) {
	//printf("Converting %ld pageNumber to virtual address %ld\n", pageNumber, (pageNumber << OFFSET_BITS));
	return (void*)(pageNumber << OFFSET_BITS);
}

static void toggleBitPhysicalBitmapPA (void* physicalPageAddress) {
	unsigned long pageNumber = (unsigned long) (((char*)physicalPageAddress - (char*)physicalMemoryBase) / PGSIZE);
	char* pagesLocation = physicalBitmap + (pageNumber / 8);
	int bitMask = 1 << (pageNumber % 8);
	(*pagesLocation) ^= (bitMask);
}

static void toggleBitPhysicalBitmapPN (unsigned long pageNumber) {
	char* pagesLocation = physicalBitmap + (pageNumber / 8);
	int bitMask = 1 << (pageNumber % 8);
	(*pagesLocation) ^= (bitMask);
}

static void toggleBitVirtualBitmapPN(unsigned long pageNumber) {
	char* pagesLocation = virtualBitmap + (pageNumber / 8);
	int bitMask = 1 << (pageNumber % 8);
	(*pagesLocation) ^= (bitMask);
}

static void toggleBitVirtualBitmapVA(void* virtualPageAddress) {
	toggleBitVirtualBitmapPN(getVirtualPageNumber(virtualPageAddress));
}

static int checkAllocatedVirtualBitmapPN(unsigned long pageNumber) {
	char* pagesLocation = virtualBitmap + (pageNumber / 8);
	int bitMask = 1 << (pageNumber % 8);
	return (*pagesLocation) & (bitMask);
}

static int checkAllocatedVirtualBitmapVA(void* virtualPageAddress) {
	checkAllocatedVirtualBitmapPN(getVirtualPageNumber(virtualPageAddress));
}

static int checkAllocatedPhysicalBitmapPN(unsigned long pageNumber) {
	char* pagesLocation = physicalBitmap + (pageNumber / 8);
	int bitMask = 1 << (pageNumber % 8);
	return (*pagesLocation) & (bitMask);
}

static int checkAllocatedPhysicalBitmapPA(void* physicalPageAddress) {
	unsigned long pageNumber = (unsigned long) (((char*)physicalPageAddress - (char*)physicalMemoryBase) / PGSIZE);
	char* pagesLocation = physicalBitmap + (pageNumber / 8);
	int bitMask = 1 << (pageNumber % 8);
	return (*pagesLocation) & (bitMask);
}

static int checkValidVirtualAddress(void* virtualPageAddress) {
	if(((unsigned long) virtualPageAddress) > MAX_VIRTUAL_ADDRESS) {
		return -1;
	}
	return 1;
}

static int checkValidVirtualPageNumber(unsigned long virtualPageNumber) {
	if (virtualPageNumber > MAX_VIRTUAL_PAGE_INDEX) {
		return -1;
	}
	return 1;
}

static void zeroOutPhysicalPage(void* physicalPageAddress) {
	memset(physicalPageAddress, '\0', sizeof(char) * PGSIZE);
}

static void toggleAllocationLinkedList(allocationLinkedList* list) {
	allocationNode* current = list->head;
	while (current != NULL) {
		*((unsigned long*)current->virtualPageEntry) = (unsigned long) NULL;
		toggleBitPhysicalBitmapPA(current->physicalAddress);
		current = current->next;
	}
}

static void freeAllocationLinkedList(allocationLinkedList* list) {
	if (list == NULL) {
		return;
	}
	allocationNode* current = list->head;
	while (current != NULL) {
		allocationNode* temp = current;
		current = current->next;
		free(temp);
	}
	free(list);
}

static void insert(allocationLinkedList* list, void* pageEntry, void* physicalAddress) {
	allocationNode* temp = malloc(sizeof(allocationNode*));
	temp->virtualPageEntry = pageEntry;
	temp->physicalAddress = physicalAddress;
	temp->next = list->head == NULL ? NULL : list->head;
	list->head = temp;
} 

static int checkIfFirstVirtualPageIsAllocated() {
	return *virtualBitmap & 1;
}

static void createTLB() {
	tlbBaseAddress = calloc(TLB_ENTRIES, sizeof(tlbNode));
}








