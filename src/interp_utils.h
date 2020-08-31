 
#define TOSTR(x)        #x
#define STRINGIFY(x)    TOSTR(x)
#define EOL             "\n"

#define ASSERT(expr)                    if (!(expr)) { DBG_PRINTF("Assertion failed (%s) at %s:%d\n", #expr , __FILE__, __LINE__); for(;;); }

#define BIT_BLEND(a,b,mask)             (((a) & ~mask) | ((b) & mask))

#define ALIGN_DOWN(addr,boundary)       ((size_t)(addr) & -(boundary))
#define ALIGN_UP(addr,boundary)         (((size_t)(addr) + ((boundary) - 1)) & -(boundary))

#define IS_ALIGNED(addr,b)              (0 == ((size_t)(addr) & ((b)-1)))

#ifndef ALLOC_EXEC_PAGE_SIZE
#define ALLOC_EXEC_PAGE_SIZE 1024
#endif

static inline
void *memcpy32 (void *dst, const void *src, size_t n)
{
    ASSERT(IS_ALIGNED(dst,4));
    ASSERT(IS_ALIGNED(src,4));
    ASSERT(IS_ALIGNED(n,4));

    uint32_t* d = (uint32_t*)dst;
    const uint32_t* s = (const uint32_t*)src;
    n /= sizeof(*d);
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

static inline
void mempatch(void* dest, size_t len, size_t dummy, size_t value) {
    char* mem = (char*)dest;
    char* end = mem+len-sizeof(dummy);
    
    // Simple implementation of memmem (not available on all platforms)
    void* off = NULL;
    while (mem < end) {
        if (!memcmp(mem, &dummy, sizeof(dummy))) {
            off = mem;
            break;
        }
        mem++;
    }
    ASSERT(off);

    // Patch memory
    *(size_t*)off = value;
}

#if defined(USE_INLINE)

    #if defined(ESP8266)
        // TODO: Couldn't find any way to allocate IRAM on ESP8266,
        // so here is a scratchpad (it's content is overwritten with the actual code)
        static
        void* IRAM_ATTR __attribute__ ((noinline)) malloc_exec()
        {
            volatile bool dummy = true;
            if (dummy) {
                return (void*)ALIGN_UP(&&scratchpad, 4);
            }

        scratchpad:
            asm volatile(".fill " STRINGIFY(ALLOC_EXEC_PAGE_SIZE) ",1,0xFF");

            return 0;
        }
    #elif defined(ESP32)
        static
        void* malloc_exec()
        {
            DBG_PRINTF("CAP_EXEC free: %u Kb, max block: %u Kb\n",
                        //heap_caps_get_total_size(MALLOC_CAP_EXEC)/1024,
                        heap_caps_get_free_size(MALLOC_CAP_EXEC)/1024,
                        heap_caps_get_largest_free_block(MALLOC_CAP_EXEC)/1024);

            return heap_caps_malloc(ALLOC_EXEC_PAGE_SIZE, MALLOC_CAP_EXEC);
        }
    #elif defined(__riscv)
        static char gProg[ALLOC_EXEC_PAGE_SIZE];
        static
        void* malloc_exec()
        {
            return (void*)gProg;
        }
    #elif defined(__linux__)
        #include <sys/mman.h>

        static
        void* malloc_exec()
        {
            return mmap(0,  getpagesize(),
                 PROT_WRITE | PROT_EXEC,
                 MAP_ANONYMOUS | MAP_PRIVATE,
                 -1, 0);
        }
    #elif defined(WIN32)
        #error Win32 not implemented        // TODO
    #else
        static
        void* malloc_exec()
        {
            return (void*)ALIGN_UP(malloc(ALLOC_EXEC_PAGE_SIZE), 4);
        }
    #endif
#else
    static
    void* malloc_exec()
    {
        return (void*)ALIGN_UP(malloc(ALLOC_EXEC_PAGE_SIZE), 4);
    }
#endif
