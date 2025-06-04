#include "vex.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

uint8_t *emu_memory;
size_t emu_offset;
size_t emu_size;

// Function to open a file and load its contents into memory
int emu_open(const char *path, int)
{
    emu_memory = nullptr;
    emu_offset = 0;
    emu_size = 0;

    FILE *f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    emu_size = ftell(f);
    fseek(f, 0, SEEK_SET); // same as rewind(f);

    emu_memory = static_cast<uint8_t *>(malloc(emu_size + 1));
    if (!emu_memory)
    {
        fclose(f);
        return -1;
    }

    fread(emu_memory, emu_size, 1, f);
    fclose(f);

    return 1;
}

// Function to close the memory-mapped file
int emu_close(int)
{
    if (emu_memory)
    {
        free(emu_memory);
    }
    return 0;
}

// Function to read data from the memory-mapped file
int emu_read(int, void *buffer, size_t len)
{
    if (!emu_memory || !buffer || len == 0)
    {
        return -1;
    }

    if (emu_offset >= emu_size)
    {
        return 0; // EOF
    }

    size_t read_length = ((emu_size - 1) < emu_offset + len) ? emu_size - emu_offset : len;
    memcpy(buffer, emu_memory + emu_offset, read_length);
    emu_offset += read_length;
    return read_length;
}

// Function to seek to a position in the memory-mapped file
int emu_lseek(int, size_t value, int type)
{
    size_t last_offset = emu_offset;

    switch (type)
    {
    case SEEK_SET:
        emu_offset = value;
        break;
    case SEEK_CUR:
        emu_offset += value;
        break;
    case SEEK_END:
        emu_offset = emu_size - 1;
        break;
    }

    if (emu_offset >= emu_size)
    {
        emu_offset = last_offset;
        return -1;
    }

    return emu_offset;
}
#define read emu_read
#define lseek emu_lseek
#define open emu_open
#define close emu_close
// Redefine standard file operations to use the memory-mapped functions

// Define macros for minimum and maximum values
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

// Define a structure for an LZW table entry
typedef struct Entry
{
    uint16_t length;
    uint16_t prefix;
    uint8_t suffix;
} Entry;

// Define a structure for the LZW table
typedef struct Table
{
    int bulk;
    int nentries;
    Entry *entries;
} Table;

// Function to read a 16-bit number from a file descriptor
static uint16_t read_num(int fd)
{
    uint8_t bytes[2];
    read(fd, bytes, 2);
    return bytes[0] + (((uint16_t)bytes[1]) << 8);
}

// Function to open a GIF file and initialize the gd_GIF structure
gd_GIF *gd_open_gif(const char *fname)
{
    int fd;
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    size_t i;
    const uint8_t *bgcolor;
    int gct_sz;
    gd_GIF *gif = nullptr;

    fd = open(fname, O_RDONLY);
    if (fd == -1)
        return nullptr;

    // Read and validate the GIF header
    read(fd, sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0)
    {
        logHandler("gd_open_gif", "invalid signature", Log::Level::Error, 3);
        close(fd);
        return nullptr;
    }

    // Read and validate the GIF version
    read(fd, sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0)
    {
        logHandler("gd_open_gif", "invalid version", Log::Level::Error, 3);
        close(fd);
        return nullptr;
    }

    // Read the logical screen width and height
    width = read_num(fd);
    height = read_num(fd);

    // Validate dimensions to prevent excessive memory allocation
    if (width == 0 || height == 0 || width > 4096 || height > 4096)
    {
        logHandler("gd_open_gif", "invalid or excessive dimensions", Log::Level::Error, 3);
        close(fd);
        return nullptr;
    }
    
    // Read the packed fields
    read(fd, &fdsz, 1);

    // Check for the presence of a global color table
    if (!(fdsz & 0x80))
    {
        logHandler("gd_open_gif", "no global color table", Log::Level::Error, 3);
        close(fd);
        return nullptr;
    }

    // Read the color resolution and the size of the global color table
    depth = ((fdsz >> 4) & 7) + 1;
    gct_sz = 1 << ((fdsz & 0x07) + 1);

    // Read the background color index and the pixel aspect ratio
    read(fd, &bgidx, 1);
    read(fd, &aspect, 1);

    // Allocate memory for the gd_GIF structure
    gif = static_cast<gd_GIF *>(calloc(1, sizeof(*gif)));
    if (!gif)
    {
        close(fd);
        return nullptr;
    }

    // Initialize the gd_GIF structure
    gif->fd = fd;
    gif->width = width;
    gif->height = height;
    gif->depth = depth;

    // Read the global color table
    gif->gct.size = gct_sz;
    read(fd, gif->gct.colors, 3 * gif->gct.size);
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;

    // Allocate memory for the frame and canvas
    gif->frame = static_cast<uint8_t *>(calloc(4, width * height));
    if (!gif->frame)
    {
        free(gif);
        close(fd);
        return nullptr;
    }
    gif->canvas = &gif->frame[width * height];

    // Initialize the frame with the background color
    if (gif->bgindex)
        memset(gif->frame, gif->bgindex, gif->width * gif->height);
    bgcolor = &gif->palette->colors[gif->bgindex * 3];
    if (bgcolor[0] || bgcolor[1] || bgcolor[2])
        for (i = 0; i < gif->width * gif->height; i++)
            memcpy(&gif->canvas[i * 3], bgcolor, 3);

    // Set the animation start position
    gif->anim_start = lseek(fd, 0, SEEK_CUR);
    return gif;
}

// Function to discard sub-blocks in the GIF file
static void discard_sub_blocks(const gd_GIF *gif)
{
    uint8_t first_try = 1;
    uint8_t seek_pos = 0;
    uint8_t size;

    do
    {
        read(gif->fd, &size, 1);
        if (!first_try && size == seek_pos) // To prevent infinite loop
            break;
        lseek(gif->fd, size, SEEK_CUR);
        seek_pos = size;
        first_try = 0;
    } while (size);
}

// Function to read a plain text extension block
static void read_plain_text_ext(gd_GIF *gif)
{
    if (gif->plain_text)
    {
        uint16_t tx, ty, tw, th;
        uint8_t cw, ch, fg, bg;
        off_t sub_block;
        lseek(gif->fd, 1, SEEK_CUR); // block size = 12
        tx = read_num(gif->fd);
        ty = read_num(gif->fd);
        tw = read_num(gif->fd);
        th = read_num(gif->fd);
        read(gif->fd, &cw, 1);
        read(gif->fd, &ch, 1);
        read(gif->fd, &fg, 1);
        read(gif->fd, &bg, 1);
        sub_block = lseek(gif->fd, 0, SEEK_CUR);
        gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
        lseek(gif->fd, sub_block, SEEK_SET);
    }
    else
    {
        // Discard plain text metadata
        lseek(gif->fd, 13, SEEK_CUR);
    }
    // Discard plain text sub-blocks
    discard_sub_blocks(gif);
}

// Function to read a graphic control extension block
static void read_graphic_control_ext(gd_GIF *gif)
{
    uint8_t rdit;

    // Discard block size (always 0x04)
    lseek(gif->fd, 1, SEEK_CUR);
    read(gif->fd, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif->fd);
    read(gif->fd, &gif->gce.tindex, 1);
    // Skip block terminator
    lseek(gif->fd, 1, SEEK_CUR);
}

// Function to read a comment extension block
static void read_comment_ext(gd_GIF *gif)
{
    if (gif->comment)
    {
        off_t sub_block = lseek(gif->fd, 0, SEEK_CUR);
        gif->comment(gif);
        lseek(gif->fd, sub_block, SEEK_SET);
    }
    // Discard comment sub-blocks
    discard_sub_blocks(gif);
}

// Function to read an application extension block
static void read_application_ext(gd_GIF *gif)
{
    char app_id[8];
    char app_auth_code[3];

    // Discard block size (always 0x0B)
    lseek(gif->fd, 1, SEEK_CUR);
    // Application Identifier
    read(gif->fd, app_id, 8);
    // Application Authentication Code
    read(gif->fd, app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id)))
    {
        // Discard block size (0x03) and constant byte (0x01)
        lseek(gif->fd, 2, SEEK_CUR);
        gif->loop_count = read_num(gif->fd);
        // Skip block terminator
        lseek(gif->fd, 1, SEEK_CUR);
    }
    else if (gif->application)
    {
        off_t sub_block = lseek(gif->fd, 0, SEEK_CUR);
        gif->application(gif, app_id, app_auth_code);
        lseek(gif->fd, sub_block, SEEK_SET);
        discard_sub_blocks(gif);
    }
    else
    {
        discard_sub_blocks(gif);
    }
}

// Function to read an extension block from the GIF file
static void read_ext(gd_GIF *gif)
{
    uint8_t label;

    if (read(gif->fd, &label, 1) < 1)
        return;

    switch (label)
    {
    case 0x01:
        read_plain_text_ext(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE:
        read_comment_ext(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        logHandler("read_ext", "unknown extension: " + std::to_string(label), Log::Level::Warn, 3);
    }
}

// Function to create a new LZW table
static Table *new_table(int key_size)
{
    uint8_t key;
    int init_bulk = std::max(1 << (key_size + 1), 0x100);
    Table *table = static_cast<Table *>(malloc(sizeof(*table) + sizeof(Entry) * init_bulk));
    if (table)
    {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = reinterpret_cast<Entry *>(&table[1]);
        for (key = 0; key < (1 << key_size); key++)
            table->entries[key] = {1, 0xFFF, key};
    }
    return table;
}

// Function to add an entry to the LZW table
static int add_entry(Table **tablep, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    Table *table = *tablep;
    if (table->nentries == table->bulk)
    {
        table->bulk *= 2;
        table = static_cast<Table *>(realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk));
        if (!table)
            return -1;
        table->entries = reinterpret_cast<Entry *>(&table[1]);
        *tablep = table;
    }
    table->entries[table->nentries] = {length, prefix, suffix};
    table->nentries++;
    if ((table->nentries & (table->nentries - 1)) == 0)
        return 1;
    return 0;
}

// Function to get a key from the GIF file
static uint16_t get_key(const gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key = 0;

    for (bits_read = 0; bits_read < key_size; bits_read += frag_size)
    {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0)
        {
            if (*sub_len == 0)
            {
                read(gif->fd, sub_len, 1); // Must be nonzero!
                if (*sub_len == 0)
                    return 0x1000;
            }
            read(gif->fd, byte, 1);
            (*sub_len)--;
        }
        frag_size = std::min(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t)((*byte) >> rpad)) << bits_read;
    }
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

// Function to compute the output index of the y-th input line in an interlaced frame
static int interlaced_line_index(int h, int y)
{
    int p = (h - 1) / 8 + 1;
    if (y < p)
        return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p)
        return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p)
        return y * 4 + 2;
    y -= p;
    return y * 2 + 1;
}

// Function to decompress image pixels
static int read_image_data(gd_GIF *gif, int interlace)
{
    uint8_t sub_len = 0, shift = 0, byte = 0;
    int init_key_size, key_size, table_is_full = 0;
    int frm_off = 0, frm_size, str_len = 0, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table *table;
    Entry entry = {0, 0, 0};
    off_t start, end;

    read(gif->fd, &byte, 1);
    key_size = static_cast<int>(byte);
    if (key_size < 2 || key_size > 8)
        return -1;

    start = lseek(gif->fd, 0, SEEK_CUR);
    discard_sub_blocks(gif);
    end = lseek(gif->fd, 0, SEEK_CUR);
    lseek(gif->fd, start, SEEK_SET);
    clear = 1 << key_size;
    stop = clear + 1;
    table = new_table(key_size);
    key_size++;
    init_key_size = key_size;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); // clear code
    frm_size = gif->fw * gif->fh;
    while (frm_off < frm_size)
    {
        if (key == clear)
        {
            key_size = init_key_size;
            table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        }
        else if (!table_is_full)
        {
            ret = add_entry(&table, str_len + 1, key, entry.suffix);
            if (ret == -1)
            {
                free(table);
                return -1;
            }
            if (table->nentries == 0x1000)
            {
                ret = 0;
                table_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear)
            continue;
        if (key == stop || key == 0x1000)
            break;
        if (key >= table->nentries)
            break;
        if (ret == 1)
            key_size++;
        entry = table->entries[key];
        str_len = entry.length;
        for (i = 0; i < str_len; i++)
        {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if (interlace)
                y = interlaced_line_index(static_cast<int>(gif->fh), y);
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
            if (entry.prefix == 0xFFF || entry.prefix >= table->nentries)
                break;
            else
                entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < table->nentries - 1 && !table_is_full)
            table->entries[table->nentries - 1].suffix = entry.suffix;
    }
    free(table);
    if (key == stop)
        read(gif->fd, &sub_len, 1); // Must be zero!
    lseek(gif->fd, end, SEEK_SET);
    return 0;
}

// Function to read an image from the GIF file
static int read_image(gd_GIF *gif)
{
    uint8_t fisrz;
    int interlace;

    gif->fx = read_num(gif->fd);
    gif->fy = read_num(gif->fd);

    if (gif->fx >= gif->width || gif->fy >= gif->height)
        return -1;

    gif->fw = read_num(gif->fd);
    gif->fh = read_num(gif->fd);

    gif->fw = std::min<uint16_t>(gif->fw, gif->width - gif->fx);
    gif->fh = std::min<uint16_t>(gif->fh, gif->height - gif->fy);

    read(gif->fd, &fisrz, 1);
    interlace = fisrz & 0x40;
    if (fisrz & 0x80)
    {
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        read(gif->fd, gif->lct.colors, 3 * gif->lct.size);
        gif->palette = &gif->lct;
    }
    else
    {
        gif->palette = &gif->gct;
    }
    return read_image_data(gif, interlace);
}

// Function to render a frame rectangle - optimized for better performance
static void render_frame_rect(gd_GIF *gif, uint8_t *buffer)
{
    int i, j, k;
    uint8_t index;
    const uint8_t *color;
    const uint8_t tindex = gif->gce.tindex;
    const bool has_transparency = gif->gce.transparency;
    
    i = gif->fy * gif->width + gif->fx;
    for (j = 0; j < gif->fh; j++)
    {
        for (k = 0; k < gif->fw; k++)
        {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
            if (!has_transparency || index != tindex)
            {
                color = &gif->palette->colors[index * 3];
                uint8_t *dest = &buffer[(i + k) * 3];
                dest[0] = color[0];
                dest[1] = color[1];
                dest[2] = color[2];
            }
        }
        i += gif->width;
    }
}

// Function to dispose of the current frame based on the disposal method - optimized
static void dispose(gd_GIF *gif)
{
    int i, j, k;
    const uint8_t *bgcolor;
    switch (gif->gce.disposal)
    {
    case 2: // Restore to background color
        bgcolor = &gif->palette->colors[gif->bgindex * 3];
        i = gif->fy * gif->width + gif->fx;
        for (j = 0; j < gif->fh; j++)
        {
            for (k = 0; k < gif->fw; k++)
            {
                uint8_t *dest = &gif->canvas[(i + k) * 3];
                dest[0] = bgcolor[0];
                dest[1] = bgcolor[1];
                dest[2] = bgcolor[2];
            }
            i += gif->width;
        }
        break;
    case 3: // Restore to previous, i.e., don't update canvas
        break;
    default:
        // Add frame non-transparent pixels to canvas
        render_frame_rect(gif, gif->canvas);
    }
}

// Function to get the next frame from the GIF file
// Returns 1 if a frame is obtained, 0 if the GIF trailer is reached, -1 if an error occurs
int gd_get_frame(gd_GIF *gif)
{
    char sep;

    dispose(gif);
    read(gif->fd, &sep, 1);
    while (sep != ',')
    {
        if (sep == ';')
            return 0;
        if (sep == '!')
            read_ext(gif);
        else
            return -1;
        if (read(gif->fd, &sep, 1) < 1)
            return -1;
    }
    if (read_image(gif) == -1)
        return -1;
    return 1;
}

// Function to render the current frame to the buffer
void gd_render_frame(gd_GIF *gif, uint8_t *buffer)
{
    memcpy(buffer, gif->canvas, gif->width * gif->height * 3);
    render_frame_rect(gif, buffer);
}

// Function to check if a given color is the background color
int gd_is_bgcolor(const gd_GIF *gif, const uint8_t color[3])
{
    return !memcmp(&gif->palette->colors[gif->bgindex * 3], color, 3);
}

// Function to rewind the GIF file to the start of the animation
void gd_rewind(const gd_GIF *gif)
{
    lseek(gif->fd, gif->anim_start, SEEK_SET);
}

// Function to close the GIF file and free allocated memory
void gd_close_gif(gd_GIF *gif)
{
    close(gif->fd);
    free(gif->frame);
    free(gif);
}

/*----------------------------------------------------------------------------*/
// C++ wrapper class by James
// 2019 - 2022
// MIT license
//

// static member function to handle rendering frames
//
int vex::Gif::render_task(void *arg)
{
    if (arg == nullptr)
        return 0;

    Gif *instance = static_cast<Gif *>(arg);
    gd_GIF *gif = instance->_gif;

    // Calculate frame time limit if max_fps is set
    int32_t frame_time_limit = 0;
    if (instance->_max_fps > 0)
    {
        frame_time_limit = 1000 / instance->_max_fps; // Convert to milliseconds
    }

    for (unsigned looped = 1;; looped++)
    {
        int32_t now = instance->_timer.system();
        int err = 0;
        instance->_frame = 0;

        while ((err = gd_get_frame(gif)) > 0)
        {
            int32_t frame_start = instance->_timer.system();
            
            // Only render if we have valid frame data
            if (gif->frame && instance->_buffer)
            {
                gd_render_frame(gif, static_cast<uint8_t *>(instance->_buffer));

                instance->_lcd.drawImageFromBuffer(static_cast<uint32_t *>(instance->_buffer), 
                                                 instance->_sx, instance->_sy, 
                                                 gif->width, gif->height);
                
                // Use VEX render API with vsync support
                if (instance->_enable_vsync)
                {
                    Brain.Screen.render(true, true); // Wait for vsync, run scheduler
                }
                else
                {
                    Brain.Screen.render(false, true); // Don't wait for vsync, run scheduler
                }
            }
            
            instance->_frame++;

            // Calculate frame timing
            int32_t gif_delay = gif->gce.delay * 10; // GIF delay in milliseconds
            int32_t frame_time = instance->_timer.system() - frame_start;
            
            // Apply frame rate limiting if enabled
            int32_t target_delay = gif_delay;
            if (frame_time_limit > 0 && frame_time_limit > gif_delay)
            {
                target_delay = frame_time_limit;
            }
            
            // Calculate remaining delay needed
            int32_t remaining_delay = target_delay - frame_time;
            if (remaining_delay > 0)
            {
                this_thread::sleep_for(remaining_delay);
            }

            // Update timing for next loop
            now = instance->_timer.system();
        }
        if (err == -1)
        {
            logHandler("render_task", "Gif: error", Log::Level::Error, 3);
            break;
        }
        // done?
        if (looped == gif->loop_count)
        {
            break;
        }

        gd_rewind(gif);
    }

    instance->cleanup();

    return 0;
}

vex::Gif::Gif(const char *fname, int sx, int sy)
{
    _sx = sx;
    _sy = sy;
    _enable_vsync = ConfigManager.getVsyncGif(); // Get from config by default
    _max_fps = 0; // No limit by default

    // open gif file
    // will allocate memory for background and one animation frame.
    _gif = gd_open_gif(fname);
    if (_gif == nullptr)
    {
        return;
    }

    // memory for rendering frame
    _buffer = static_cast<uint32_t *>(malloc(_gif->width * _gif->height * sizeof(uint32_t)));
    if (_buffer == nullptr)
    {
        // out of memory
        gd_close_gif(_gif);
        _gif = nullptr;
    }
    else
    {
        // Clear buffer to prevent artifacts
        memset(_buffer, 0, _gif->width * _gif->height * sizeof(uint32_t));
        // create thread to handle this gif
        _t1 = thread(render_task, static_cast<void *>(this));
    }
}

vex::Gif::Gif(const char *fname, int sx, int sy, bool enable_vsync, int max_fps)
{
    _sx = sx;
    _sy = sy;
    _enable_vsync = enable_vsync;
    _max_fps = max_fps;

    // open gif file
    // will allocate memory for background and one animation frame.
    _gif = gd_open_gif(fname);
    if (_gif == nullptr)
    {
        return;
    }

    // memory for rendering frame
    _buffer = static_cast<uint32_t *>(malloc(_gif->width * _gif->height * sizeof(uint32_t)));
    if (_buffer == nullptr)
    {
        // out of memory
        gd_close_gif(_gif);
        _gif = nullptr;
    }
    else
    {
        // Clear buffer to prevent artifacts
        memset(_buffer, 0, _gif->width * _gif->height * sizeof(uint32_t));
        // create thread to handle this gif
        _t1 = thread(render_task, static_cast<void *>(this));
    }
}

vex::Gif::~Gif()
{
    cleanup();
}

// cleanup memory when gif finishes
void vex::Gif::cleanup()
{
    // stop thread
    if (_t1.joinable())
    {
        _t1.join();
    }

    if (_buffer)
    {
        free(_buffer);
        _buffer = nullptr;
    }

    if (_gif)
    {
        gd_close_gif(_gif);
        _gif = nullptr;
    }
}

// get current rendered frame
int vex::Gif::getFrameIndex()
{
    return _frame;
}
