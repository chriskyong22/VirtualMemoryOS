#include "my_vm.h"
#include <math.h>
void* physicalMemoryBase = NULL;
void* physicalBitmap = NULL;
void* virtualBitmap = NULL;
#define ADDRESS_SPACE (sizeof(void*) * 8)
/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    physicalMemoryBase = malloc(MEMSIZE);
    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
	
	
	// Character is 1 byte and 1 byte = 8 bits 
	// therefore we can store 8 pages per character and thus the number of mappings required is 
	// # of Pages (Bytes) / Page Size (Bytes) = Number of Pages Required in Bytes 
	// # of Pages in Bytes / 8 = Number of Pages Required in Bits
	// Do I have to add + 1 incase we get a fractional amount? 
	// E.g. 101.5 would be 102 bits required to store the pages so should I add 1 always as a safety check?
	
	physicalBitmap = malloc(sizeof(char) * ((MEMSIZE) / (PGSIZE * 8)));
	virtualBitmap = malloc(sizeof(char) * ((MAX_MEMSIZE)/(PGSIZE * 8)));

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
    
	unsigned int offsetBits = (int)log2(PGSIZE);
	unsigned long offset = *((unsigned long*)va) & (PGSIZE);
	
	// Note, addressSpaceBits will be inaccurate for real 64-bit based paging 
	// since 64-bit paging only uses 48 bits
	unsigned int addressSpaceBits = ADDRESS_SPACE;
	addressSpaceBits >>= offsetBits;
	
	// Page Table Entries = Page Table Size / Entry Size 
	// Page Table Bits = log2(Page Table Entries)
	unsigned int pageTableBits = (int)log2((PGSIZE / sizeof(void*)));
	unsigned int pageTableLevels = 1;
	while(addressSpaceBits > pageTableBits) {
		pageTableLevels++;
		addressSpaceBits -= pageTableBits;
	}
	// At this point, addressSpaceBits is equal to the # of bits for last page 
	// table (Used for edgecase where, # of bits for last page table != # of bits
	// for each page table since it could not be split evenly
	unsigned long* nextAddress = pgdir;
	unsigned int usedTopBits = 0;
	// To create Page Table Mask:
	// Step 1) Prune off all bits that are not the desired page table bits 
	// Step 2) Shift the number of bits pruned off to get the value with only 
	//		   the page table bits set.
	unsigned long pageTableMask = (ADDRESS_SPACE >> (ADDRESS_SPACE - addressSpaceBits)) << (ADDRESS_SPACE - addressSpaceBits);
	usedTopBits = addressSpaceBits;
	while (pageTableLevels != 0) {
		// Mask with the Virtual Address (Assuming VA is unsigned long*) 
		unsigned long pageTableIndex = *((unsigned long*)va) & pageTableMask;
		// Access the Index in the Page Table via: Page Table Base Address + (sizeof(page table entry) * page number to access)
		nextAddress = *(nextAddress + pageTableIndex);
		pageTableLevels--;
		// Creating the new Page Table Mask:
		// Step 1) Store an inverted version of the old Page Table Mask to be used to zero out the Top Bits 
		// Step 2) Create a Mask that includes the old Page Table Bits and the newly desired Page Table Bits (zeroing out the non-page table bits)
		// Step 3) AND the new Mask with the inverted mask to zero out the top bits that were already used
		unsigned long inverseMask = ~pageTableMask;
		pageTableMask = (ADDRESS_SPACE >> (ADDRESS_SPACE - (usedTopBits + pageTableBits))) << (ADDRESS_SPACE - (usedTopBits + pageTableBits));
		pageTableMask &= inverseMask;
		usedTopBits += pageTableBits;
	}
	
    //If translation not successfull
    return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    return -1;
}

/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
}

//I Moved these 3 TLB METHODS To here, not sure why it was above part 1 stuff

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int add_TLB(void *va, void *pa) {

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */

    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t * check_TLB(void *va) {

    /* Part 2: TLB lookup code here */

}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate() {
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/


    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
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

    return NULL;
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




}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */




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

       
}

/*
Helper Functions
*/




