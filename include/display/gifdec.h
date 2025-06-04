#ifndef GIFDEC_H
#define GIFDEC_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct gd_Palette
    {
        int size;
        uint8_t colors[0x100 * 3];
    } gd_Palette;

    typedef struct gd_GCE
    {
        uint16_t delay;
        uint8_t tindex;
        uint8_t disposal;
        int input;
        int transparency;
    } gd_GCE;

    typedef struct gd_GIF
    {
        int fd;
        off_t anim_start;
        uint16_t width, height;
        uint16_t depth;
        uint16_t loop_count;
        gd_GCE gce;
        gd_Palette *palette;
        gd_Palette lct, gct;
        void (*plain_text)(
            struct gd_GIF *gif, uint16_t tx, uint16_t ty,
            uint16_t tw, uint16_t th, uint8_t cw, uint8_t ch,
            uint8_t fg, uint8_t bg);
        void (*comment)(struct gd_GIF *gif);
        void (*application)(struct gd_GIF *gif, char id[8], char auth[3]);
        uint16_t fx, fy, fw, fh;
        uint8_t bgindex;
        uint8_t *canvas, *frame;
    } gd_GIF;

    gd_GIF *gd_open_gif(const char *fname);
    int gd_get_frame(gd_GIF *gif);
    void gd_render_frame(gd_GIF *gif, uint8_t *buffer);
    int gd_is_bgcolor(const gd_GIF *gif, const uint8_t color[3]);
    void gd_rewind(const gd_GIF *gif);
    void gd_close_gif(gd_GIF *gif);

#ifdef __cplusplus
}
#endif

#endif /* GIFDEC_H */
/*----------------------------------------------------------------------------*/

namespace vex
{
    /**
     * @class Gif
     * @brief Enhanced GIF player with vsync support and frame rate control
     * 
     * This class provides improved GIF playback with:
     * - VEX render() API integration for proper vsync support
     * - Frame rate limiting capability 
     * - Better memory management and error handling
     * - Performance optimizations for smoother playback
     * - Configuration integration with ConfigManager
     */
    class Gif
    {
    private:
        gd_GIF *_gif = nullptr;
        int _sx;
        int _sy;
        void *_buffer = nullptr;
        int _frame = 0;
        bool _enable_vsync = false;
        int _max_fps = 0; // 0 means no limit

        vex::timer _timer;
        vex::brain::lcd _lcd;
        vex::thread _t1;

        static int render_task(void *arg);
        void cleanup();

    public:
        /**
         * @brief Construct GIF player with config-based settings
         * @param fname Path to GIF file
         * @param sx X position on screen
         * @param sy Y position on screen
         */
        Gif(const char *fname, int sx, int sy);
        
        /**
         * @brief Construct GIF player with explicit settings
         * @param fname Path to GIF file  
         * @param sx X position on screen
         * @param sy Y position on screen
         * @param enable_vsync Enable VEX vsync rendering
         * @param max_fps Maximum frame rate (0 = no limit)
         */
        Gif(const char *fname, int sx, int sy, bool enable_vsync, int max_fps = 0);
        
        ~Gif();
        
        int getFrameIndex();
        void setVsync(bool enable) { _enable_vsync = enable; }
        void setMaxFps(int fps) { _max_fps = fps; }
    };
}