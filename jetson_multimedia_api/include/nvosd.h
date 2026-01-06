/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. The names of its contributors may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @file
 * <b>NVIDIA Multimedia Utilities: On-Screen Display Manager</b>
 *
 * This file defines the NvOSD library to be used to draw rectangles and text over the frame
 * for given parameters.
 */

/**
 * @defgroup ee_nvosd_group On-Screen Display Manager
 * Defines the NvOSD library to be used to draw rectangles and text over the frame
 * for given parameters.
 * @ingroup common_utility_group
 * @{
 */

#ifndef __NVOSD_DEFS__
#define __NVOSD_DEFS__

#ifdef __cplusplus
extern "C"
{
#endif

#define NVOSD_MAX_NUM_RECTS 128
#define MAX_BG_CLR 20

/**
 * Holds the color parameters of the box or text to be overlayed.
 */
typedef struct _NvOSD_ColorParams {
    double red;                 /**< Holds red component of color.
                                     Value must be in the range 0-1. */

    double green;               /**< Holds green component of color.
                                     Value must be in the range 0-1.*/

    double blue;                /**< Holds blue component of color.
                                     Value must be in the range 0-1.*/

    double alpha;               /**< Holds alpha component of color.
                                     Value must be in the range 0-1.*/
}NvOSD_ColorParams;


/**
 * Holds the font parameters of the text to be overlayed.
 */
typedef struct _NvOSD_FontParams {
    char * font_name;            /**< Holds pointer to the string containing
                                      font name. */

    unsigned int font_size;         /**< Holds size of the font. */

    NvOSD_ColorParams font_color;   /**< Holds font color. */
}NvOSD_FontParams;


/**
 * Holds the text parameters of the text to be overlayed.
 */

typedef struct _NvOSD_TextParams {
    char * display_text; /**< Holds the text to be overlayed. */

    unsigned int x_offset; /**< Holds horizontal offset w.r.t top left pixel of
                             the frame. */
    unsigned int y_offset; /**< Holds vertical offset w.r.t top left pixel of
                             the frame. */

    NvOSD_FontParams font_params; /**< font_params. */

    int set_bg_clr; /**< Boolean to indicate text has background color. */

    NvOSD_ColorParams text_bg_clr; /**< Background color for text. */

}NvOSD_TextParams;


typedef struct _NvOSD_Color_info {
    int id;
    NvOSD_ColorParams color;
}NvOSD_Color_info;


/**
 * Holds the box parameters of the box to be overlayed.
 */
typedef struct _NvOSD_RectParams {
    unsigned int left;   /**< Holds left coordinate of the box in pixels. */

    unsigned int top;    /**< Holds top coordinate of the box in pixels. */

    unsigned int width;  /**< Holds width of the box in pixels. */

    unsigned int height; /**< Holds height of the box in pixels. */

    unsigned int border_width; /**< Holds border_width of the box in pixels. */

    NvOSD_ColorParams border_color; /**< Holds color params of the border
                                      of the box. */

    unsigned int has_bg_color;  /**< Holds boolean value indicating whether box
                                    has background color. */

    unsigned int reserved; /** Reserved field for future usage.
                             For internal purpose only */

    NvOSD_ColorParams bg_color; /**< Holds background color of the box. */

    int has_color_info;
    int color_id;

}NvOSD_RectParams;

/**
 * Holds the box parameters of the line to be overlayed.
 */
typedef struct _NvOSD_LineParams {
  unsigned int x1;   /**< Holds left coordinate of the box in pixels. */

  unsigned int y1;    /**< Holds top coordinate of the box in pixels. */

  unsigned int x2;  /**< Holds width of the box in pixels. */

  unsigned int y2; /**< Holds height of the box in pixels. */

  unsigned int line_width; /**< Holds border_width of the box in pixels. */

  NvOSD_ColorParams line_color; /**< Holds color params of the border
                                        of the box. */
} NvOSD_LineParams;

/**
 * Holds the arrow parameters to be overlayed.
 */
typedef struct _NvOSD_ArrowParams {
    unsigned int x1;   /**< Holds start horizontal coordinate in pixels. */

    unsigned int y1;    /**< Holds start vertical coordinate in pixels. */

    unsigned int x2;  /**< Holds end horizontal coordinate in pixels. */

    unsigned int y2; /**< Holds end vertical coordinate in pixels. */

    unsigned int arrow_width; /**< Holds arrow_width in pixels. */

    unsigned int start_arrow_head; /** Holds boolean value indicating whether
                                     arrow head is at start or at end.
                                     Setting to value 1 indicates arrow head is
                                     at start. Otherwise it is at end. */

    NvOSD_ColorParams arrow_color; /**< Holds color params of the arrow box. */

    unsigned int reserved; /**< reserved field for future usage.
                             For internal purpose only. */

}NvOSD_ArrowParams;


/**
 * Holds the circle parameters to be overlayed.
 */
typedef struct _NvOSD_CircleParams {
    unsigned int xc;   /**< Holds start horizontal coordinate in pixels. */

    unsigned int yc;    /**< Holds start vertical coordinate in pixels. */

    unsigned int radius;    /**< Holds radius of circle in pixels. */

    NvOSD_ColorParams circle_color; /**< Holds color params of the arrow box. */

    unsigned int reserved; /**< reserved field for future usage.
                             For internal purpose only. */

}NvOSD_CircleParams;

/**
 * List modes used to overlay boxes and text.
 */
typedef enum{
    MODE_CPU, /**< Selects CPU for OSD processing.
                Works with RGBA data only */
    MODE_GPU, /**< Selects GPU for OSD processing.
                Yet to be implemented */
    MODE_HW   /**< Selects NV HW engine for rectangle draw and mask.
                   This mode works with both YUV and RGB data.
                   It does not consider alpha parameter.
                   Not applicable for drawing text. */
} NvOSD_Mode;

/**
 * Creates NvOSD context.
 *
 * @returns A pointer to NvOSD context, NULL in case of failure.
 */
void *nvosd_create_context(void);

/**
 * Destroys NvOSD context.
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context.
 */

void nvosd_destroy_context(void *nvosd_ctx);

/**
 * Sets clock parameters for the given context.
 *
 * The clock is overlayed when nvosd_put_text() is called.
 * If no other text is to be overlayed, nvosd_put_text must be called with
 * @a num_strings as 0 and @a text_params_list as NULL.
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context.
 * @param[in] clk_params A pointer to NvOSD_TextParams structure for the clock
 *            to be overlayed; NULL to disable the clock.
 */
void nvosd_set_clock_params(void *nvosd_ctx, NvOSD_TextParams *clk_params);


/**
 * Overlays clock and given text at given location on a buffer.
 *
 * To overlay the clock, you must set clock params using
 * nvosd_set_clock_params().
 * You must ensure that the length of @a text_params_list is at least
 * @a num_strings.
 *
 * @note Currently only #MODE_CPU is supported. Specifying other modes wil have
 * no effect.
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context.
 * @param[in] mode Mode selection to draw the text.
 * @param[in] fd DMABUF FD of buffer on which text is to be overlayed.
 * @param[in] num_strings Number of strings to be overlayed.
 * @param[in] text_params_list A pointer to an array of NvOSD_TextParams
 *            structure for the clock and text to be overlayed.
 *
 * @returns 0 for success, -1 for failure.
 */
int nvosd_put_text(void *nvosd_ctx, NvOSD_Mode mode, int fd, int num_strings,
        NvOSD_TextParams *text_params_list);


/**
 * Overlays boxes at given location on a buffer.
 *
 * Boxes can be configured with:
 * a. Only border
 *    To draw boxes with only border, you must set @a border_width and set
 *    @a has_bg_color to 0 for the given box.
 * b. Border and background color
 *    To draw boxes with border and background color, you must set @a
 *    border_width and set @a has_bg_color to 1, and specify background color
 *    parameters for the given box.
 * c. Solid fill acting as mask region
 *    To draw boxes with solid fill acting as mask region, you must set @a
 *    border_width to 0 and @a has_bg_color to 1 for the given box.
 *
 *
 * You must ensure that the length of @a rect_params_list is at least
 * @a num_rects.
 *
 * @param[in] nvosd_ctx A pointer to NvOSD context.
 * @param[in] mode Mode selection to draw the boxes.
 * @param[in] fd DMABUF FD of buffer on which boxes are to be overlayed.
 * @param[in] num_rects Number of boxes to be overlayed.
 * @param[in] rect_params_list A pointer to an array of NvOSD_TextParams
 *            structure for the clock and text to be overlayed.
 *
 * @returns 0 for success, -1 for failure.
 */
int nvosd_draw_rectangles(void *nvosd_ctx, NvOSD_Mode mode, int fd,
        int num_rects, NvOSD_RectParams *rect_params_list);

int nvosd_init_colors_for_hw_blend(void *nvosd_ctx, NvOSD_Color_info * color_info,
        int num_classes);


int nvosd_draw_arrows(void *nvosd_ctx, NvOSD_Mode mode, int fd,
        int num_arrows, NvOSD_ArrowParams *arrow_params_list);

int nvosd_draw_circles(void *nvosd_ctx, NvOSD_Mode mode, int fd,
        int num_circles, NvOSD_CircleParams *circle_params_list);

int nvosd_draw_lines(void *nvosd_ctx, NvOSD_Mode mode, int fd,
        int num_lines, NvOSD_LineParams *line_params_list);

#ifdef __cplusplus
}
#endif
/** @} */
#endif
