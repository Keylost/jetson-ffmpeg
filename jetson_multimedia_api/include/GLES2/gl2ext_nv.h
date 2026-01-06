/*
 * Copyright (c) 2011 - 2016 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __gl2ext_nv_h_
#define __gl2ext_nv_h_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GL_ARB_half_float_pixel
#define GL_ARB_half_float_pixel 1
#define GL_HALF_FLOAT_ARB                          0x140B
#endif /* GL_ARB_half_float_pixel */

#ifndef GL_ARB_texture_rectangle
#define GL_ARB_texture_rectangle 1
#define GL_TEXTURE_RECTANGLE_ARB                0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE_ARB        0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE_ARB          0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB       0x84F8
#define GL_SAMPLER_2D_RECT_ARB                  0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB           0x8B64
#endif /* GL_ARB_texture_rectangle */

#ifndef GL_EXT_texture_compression_latc
#define GL_EXT_texture_compression_latc 1
#define GL_COMPRESSED_LUMINANCE_LATC1_EXT               0x8C70
#define GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT        0x8C71
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT         0x8C72
#define GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT  0x8C73
#endif /* GL_EXT_texture_compression_latc */

#ifndef GL_EXT_texture_lod_bias
#define GL_EXT_texture_lod_bias 1
#define GL_MAX_TEXTURE_LOD_BIAS_EXT       0x84FD
#define GL_TEXTURE_FILTER_CONTROL_EXT     0x8500
#define GL_TEXTURE_LOD_BIAS_EXT           0x8501
#endif /* GL_EXT_texture_lod_bias */

#ifndef GL_NV_3dvision_settings
#define GL_NV_3dvision_settings 1
#define GL_3DVISION_STEREO_NV                 0x90F4
#define GL_STEREO_SEPARATION_NV               0x90F5
#define GL_STEREO_CONVERGENCE_NV              0x90F6
#define GL_STEREO_CUTOFF_NV                   0x90F7
#define GL_STEREO_PROJECTION_NV               0x90F8
#define GL_STEREO_PROJECTION_PERSPECTIVE_NV   0x90F9
#define GL_STEREO_PROJECTION_ORTHO_NV         0x90FA
typedef void (GL_APIENTRYP PFNGLSTEREOPARAMETERFNVPROC) (GLenum pname, GLfloat params);
typedef void (GL_APIENTRYP PFNGLSTEREOPARAMETERINVPROC) (GLenum pname, GLint params);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glStereoParameterfNV(GLenum pname, GLfloat params);
GL_APICALL void GL_APIENTRY glStereoParameteriNV(GLenum pname, GLint params);
#endif
#endif /* GL_NV_3dvision_settings */

#ifndef GL_NV_bgr
#define GL_NV_bgr 1
#define GL_BGR_NV                                               0x80E0
#endif /* GL_NV_bgr */

#ifndef GL_NV_copy_image
#define GL_NV_copy_image 1
typedef void (GL_APIENTRYP PFNGLCOPYIMAGESUBDATANVPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glCopyImageSubDataNV(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);
#endif
#endif /* GL_NV_copy_image */

#ifndef GL_NV_draw_path
#define GL_NV_draw_path 1
#define GL_PATH_QUALITY_NV          0x8ED8
#define GL_FILL_RULE_NV             0x8ED9
#define GL_STROKE_CAP0_STYLE_NV     0x8EE0
#define GL_STROKE_CAP1_STYLE_NV     0x8EE1
#define GL_STROKE_CAP2_STYLE_NV     0x8EE2
#define GL_STROKE_CAP3_STYLE_NV     0x8EE3
#define GL_STROKE_JOIN_STYLE_NV     0x8EE8
#define GL_STROKE_MITER_LIMIT_NV    0x8EE9
#define GL_EVEN_ODD_NV              0x8EF0
#define GL_NON_ZERO_NV              0x8EF1
#define GL_CAP_BUTT_NV              0x8EF4
#define GL_CAP_ROUND_NV             0x8EF5
#define GL_CAP_SQUARE_NV            0x8EF6
#define GL_CAP_TRIANGLE_NV          0x8EF7
#define GL_JOIN_MITER_NV            0x8EFC
#define GL_JOIN_ROUND_NV            0x8EFD
#define GL_JOIN_BEVEL_NV            0x8EFE
#define GL_JOIN_CLIPPED_MITER_NV    0x8EFF
#define GL_MATRIX_PATH_TO_CLIP_NV   0x8F04
#define GL_MATRIX_STROKE_TO_PATH_NV 0x8F05
#define GL_MATRIX_PATH_COORD0_NV    0x8F08
#define GL_MATRIX_PATH_COORD1_NV    0x8F09
#define GL_MATRIX_PATH_COORD2_NV    0x8F0A
#define GL_MATRIX_PATH_COORD3_NV    0x8F0B
#define GL_FILL_PATH_NV             0x8F18
#define GL_STROKE_PATH_NV           0x8F19
#define GL_QUADRATIC_BEZIER_TO_NV   0x02
#define GL_CUBIC_BEZIER_TO_NV       0x03
#define GL_START_MARKER_NV          0x20
#define GL_CLOSE_NV                 0x21
#define GL_CLOSE_FILL_NV            0x22
#define GL_STROKE_CAP0_NV           0x40
#define GL_STROKE_CAP1_NV           0x41
#define GL_STROKE_CAP2_NV           0x42
#define GL_STROKE_CAP3_NV           0x43
typedef GLuint (GL_APIENTRYP PFNGLCREATEPATHNVPROC) (GLenum datatype, GLsizei numCommands, const GLubyte* commands);
typedef void (GL_APIENTRYP PFNGLDELETEPATHNVPROC) (GLuint path);
typedef void (GL_APIENTRYP PFNGLPATHVERTICESNVPROC) (GLuint path, const void* vertices);
typedef GLuint (GL_APIENTRYP PFNGLCREATEPATHPROGRAMNVPROC) (void);
typedef void (GL_APIENTRYP PFNGLPATHMATRIXNVPROC) (GLenum target, const GLfloat* value);
typedef void (GL_APIENTRYP PFNGLDRAWPATHNVPROC) (GLuint path, GLenum mode);
typedef GLuint (GL_APIENTRYP PFNGLCREATEPATHBUFFERNVPROC) (GLsizei capacity);
typedef void (GL_APIENTRYP PFNGLDELETEPATHBUFFERNVPROC) (GLuint buffer);
typedef void (GL_APIENTRYP PFNGLPATHBUFFERPATHNVPROC) (GLuint buffer, GLint index, GLuint path);
typedef void (GL_APIENTRYP PFNGLPATHBUFFERPOSITIONNVPROC) (GLuint buffer, GLint index, GLfloat x, GLfloat y);
typedef void (GL_APIENTRYP PFNGLDRAWPATHBUFFERNVPROC) (GLuint buffer, GLenum mode);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL GLuint GL_APIENTRY glCreatePathNV (GLenum datatype, GLsizei numCommands, const GLubyte* commands);
GL_APICALL void GL_APIENTRY glDeletePathNV (GLuint path);
GL_APICALL void GL_APIENTRY glPathVerticesNV (GLuint path, const void* vertices);
GL_APICALL GLuint GL_APIENTRY glCreatePathProgramNV (void);
GL_APICALL void GL_APIENTRY glPathMatrixNV (GLenum target, const GLfloat* value);
GL_APICALL void GL_APIENTRY glDrawPathNV (GLuint path, GLenum mode);
GL_APICALL GLuint GL_APIENTRY glCreatePathbufferNV (GLsizei capacity);
GL_APICALL void GL_APIENTRY glDeletePathbufferNV (GLuint buffer);
GL_APICALL void GL_APIENTRY glPathbufferPathNV (GLuint buffer, GLint index, GLuint path);
GL_APICALL void GL_APIENTRY glPathbufferPositionNV (GLuint buffer, GLint index, GLfloat x, GLfloat y);
GL_APICALL void GL_APIENTRY glDrawPathbufferNV (GLuint buffer, GLenum mode);
#endif
#endif /* GL_NV_draw_path */

#ifndef GL_NV_draw_texture
#define GL_NV_draw_texture 1
typedef void (GL_APIENTRYP PFNGLDRAWTEXTURENVPROC) (GLuint texture, GLuint sampler, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z, GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glDrawTextureNV(GLuint texture, GLuint sampler, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z, GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1);
#endif
#endif /* GL_NV_draw_texture */

#ifndef GL_NV_EGL_image_YUV
#define GL_NV_EGL_image_YUV 1
#define GL_IMAGE_PLANE_Y_NV                        0x313D
#define GL_IMAGE_PLANE_UV_NV                       0x313E
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DYUVNVPROC) (GLenum target, GLenum plane, GLeglImageOES image);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glEGLImageTargetTexture2DYUVNV(GLenum target, GLenum plane, GLeglImageOES image);
#endif
#endif /* GL_NV_EGL_image_YUV */

#ifndef GL_NV_framebuffer_sRGB
#define GL_NV_framebuffer_sRGB 1
#define GL_FRAMEBUFFER_SRGB_NV            0x8DB9
#endif /* GL_NV_framebuffer_sRGB */

#ifndef GL_NV_framebuffer_vertex_attrib_array
#define GL_NV_framebuffer_vertex_attrib_array 1
#define GL_FRAMEBUFFER_ATTACHABLE_NV                                  0x852A
#define GL_VERTEX_ATTRIB_ARRAY_NV                                     0x852B
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_SIZE_NV         0x852C
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_TYPE_NV         0x852D
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_NORMALIZED_NV   0x852E
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_OFFSET_NV       0x852F
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_WIDTH_NV        0x8530
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_STRIDE_NV       0x8531
#define GL_FRAMEBUFFER_ATTACHMENT_VERTEX_ATTRIB_ARRAY_HEIGHT_NV       0x8532
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERVERTEXATTRIBARRAYNVPROC) (GLenum target, GLenum attachment, GLenum buffertarget, GLuint bufferobject, GLint size, GLenum type, GLboolean normalized, GLintptr offset, GLsizeiptr width, GLsizeiptr height, GLsizei stride);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glFramebufferVertexAttribArrayNV (GLenum target, GLenum attachment, GLenum buffertarget, GLuint bufferobject, GLint size, GLenum type, GLboolean normalized, GLintptr offset, GLsizeiptr width, GLsizeiptr height, GLsizei stride);
#endif
#endif /* GL_NV_framebuffer_vertex_attrib_array */

#ifndef GL_NV_get_tex_image
#define GL_NV_get_tex_image 1
#define GL_TEXTURE_WIDTH_NV                  0x1000
#define GL_TEXTURE_HEIGHT_NV                 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT_NV        0x1003
#define GL_TEXTURE_COMPONENTS_NV             GL_TEXTURE_INTERNAL_FORMAT_NV
#define GL_TEXTURE_BORDER_NV                 0x1005
#define GL_TEXTURE_RED_SIZE_NV               0x805C
#define GL_TEXTURE_GREEN_SIZE_NV             0x805D
#define GL_TEXTURE_BLUE_SIZE_NV              0x805E
#define GL_TEXTURE_ALPHA_SIZE_NV             0x805F
#define GL_TEXTURE_LUMINANCE_SIZE_NV         0x8060
#define GL_TEXTURE_INTENSITY_SIZE_NV         0x8061
#define GL_TEXTURE_DEPTH_NV                  0x8071
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE_NV  0x86A0
#define GL_TEXTURE_COMPRESSED_NV             0x86A1
#define GL_TEXTURE_DEPTH_SIZE_NV             0x884A
#define GL_PACK_SKIP_IMAGES_NV               0x806B
#define GL_PACK_IMAGE_HEIGHT_NV              0x806C
typedef void (GL_APIENTRYP PFNGLGETTEXIMAGENVPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLvoid* img);
typedef void (GL_APIENTRYP PFNGLGETCOMPRESSEDTEXIMAGENVPROC) (GLenum target, GLint level, GLvoid* img);
typedef void (GL_APIENTRYP PFNGLGETTEXLEVELPARAMETERFVNVPROC) (GLenum target, GLint level, GLenum pname, GLfloat* params);
typedef void (GL_APIENTRYP PFNGLGETTEXLEVELPARAMETERIVNVPROC) (GLenum target, GLint level, GLenum pname, GLint* params);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGetTexImageNV (GLenum target, GLint level, GLenum format, GLenum type, GLvoid* img);
GL_APICALL void GL_APIENTRY glGetCompressedTexImageNV (GLenum target, GLint level, GLvoid* img);
GL_APICALL void GL_APIENTRY glGetTexLevelParameterfvNV (GLenum target, GLint level, GLenum pname, GLfloat* params);
GL_APICALL void GL_APIENTRY glGetTexLevelParameterivNV (GLenum target, GLint level, GLenum pname, GLint* params);
#endif
#endif /* GL_NV_get_tex_image */

#ifndef GL_NV_occlusion_query_samples
#define GL_NV_occlusion_query_samples 1
#define GL_SAMPLES_PASSED_NV                 0x8914
#endif /* GL_NV_occlusion_query_samples */

#ifndef GL_NV_pack_subimage
#define GL_NV_pack_subimage 1
#define GL_PACK_ROW_LENGTH_NV                0x0D02
#define GL_PACK_SKIP_ROWS_NV                 0x0D03
#define GL_PACK_SKIP_PIXELS_NV               0x0D04
#endif /* GL_NV_pack_subimage */

#ifndef GL_NV_packed_float
#define GL_NV_packed_float 1
#define GL_R11F_G11F_B10F_NV                0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV_NV  0x8C3B
#endif /* GL_NV_packed_float */

#ifndef GL_NV_platform_binary
#define GL_NV_platform_binary 1
#define GL_NVIDIA_PLATFORM_BINARY_NV      0x890B
#endif /* GL_NV_platform_binary */

#ifndef GL_NV_sample_mask
#define GL_NV_sample_mask 1
#define GL_SAMPLE_POSITION_NV                                   0x8E50
#define GL_SAMPLE_MASK_NV                                       0x8E51
#define GL_SAMPLE_MASK_VALUE_NV                                 0x8E52
#define GL_MAX_SAMPLE_MASK_WORDS_NV                             0x8E59
/* PFNGLGETINTEGERI_VNVPROC defined by NV_uniform_buffer_object */
typedef void (GL_APIENTRYP PFNGLGETMULTISAMPLEFVNVPROC) (GLenum pname, GLuint index, GLfloat *value);
typedef void (GL_APIENTRYP PFNGLSAMPLEMASKINVPROC) (GLuint index, GLbitfield mask);
#ifdef GL_GLEXT_PROTOTYPES
/* glGetIntegeri_vNV defined by NV_uniform_buffer_object */
GL_APICALL void GL_APIENTRY glGetMultisamplefvNV (GLenum pname, GLuint index, GLfloat *value);
GL_APICALL void GL_APIENTRY glSampleMaskiNV (GLuint index, GLbitfield mask);
#endif
#endif /* NV_sample_mask */

#ifndef GL_NV_sampler_objects
#define GL_NV_sampler_objects 1
#define GL_SAMPLER_BINDING_NV             0x8919
typedef void (GL_APIENTRYP PFNGLGENSAMPLERSNVPROC) (GLsizei count, GLuint *samplers);
typedef void (GL_APIENTRYP PFNGLDELETESAMPLERSNVPROC) (GLsizei count, const GLuint *samplers);
typedef GLboolean (GL_APIENTRYP PFNGLISSAMPLERNVPROC) (GLuint sampler);
typedef void (GL_APIENTRYP PFNGLBINDSAMPLERNVPROC) (GLuint unit, GLuint sampler);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERINVPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERFNVPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERIVNVPROC) (GLuint sampler, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERIIVNVPROC) (GLuint sampler, GLenum pname, const GLint *params);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERIUIVNVPROC) (GLuint sampler, GLenum pname, const GLuint *params);
typedef void (GL_APIENTRYP PFNGLSAMPLERPARAMETERFVNVPROC) (GLuint sampler, GLenum pname, const GLfloat *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERIVNVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERFVNVPROC) (GLuint sampler, GLenum pname, GLfloat *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERIIVNVPROC) (GLuint sampler, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETSAMPLERPARAMETERIUIVNVPROC) (GLuint sampler, GLenum pname, GLuint *params);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGenSamplersNV (GLsizei count, GLuint *samplers);
GL_APICALL void GL_APIENTRY glDeleteSamplersNV (GLsizei count, const GLuint *samplers);
GL_APICALL GLboolean GL_APIENTRY glIsSamplerNV (GLuint sampler);
GL_APICALL void GL_APIENTRY glBindSamplerNV (GLuint unit, GLuint sampler);
GL_APICALL void GL_APIENTRY glSamplerParameteriNV (GLuint sampler, GLenum pname, GLint param);
GL_APICALL void GL_APIENTRY glSamplerParameterfNV (GLuint sampler, GLenum pname, GLfloat param);
GL_APICALL void GL_APIENTRY glSamplerParameterivNV (GLuint sampler, GLenum pname, const GLint *params);
GL_APICALL void GL_APIENTRY glSamplerParameterfvNV (GLuint sampler, GLenum pname, const GLfloat *params);
GL_APICALL void GL_APIENTRY glSamplerParameterIivNV (GLuint sampler, GLenum pname, const GLint *params);
GL_APICALL void GL_APIENTRY glSamplerParameterIuivNV (GLuint sampler, GLenum pname, const GLint *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterivNV (GLuint sampler, GLenum pname, GLint *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterfvNV (GLuint sampler, GLenum pname, GLfloat *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterIivNV (GLuint sampler, GLenum pname, GLint *params);
GL_APICALL void GL_APIENTRY glGetSamplerParameterIuivNV (GLuint sampler, GLenum pname, GLuint *params);
#endif
#endif /* GL_NV_sampler_objects */

#ifndef GL_NV_shader_framebuffer_fetch
#define GL_NV_shader_framebuffer_fetch 1
#endif /* GL_NV_shader_framebuffer_fetch */

#ifndef GL_NV_texture_array
#define GL_NV_texture_array 1
#define GL_UNPACK_SKIP_IMAGES_NV          0x806D
#define GL_UNPACK_IMAGE_HEIGHT_NV         0x806E
#define GL_TEXTURE_2D_ARRAY_NV            0x8C1A
#define GL_SAMPLER_2D_ARRAY_NV            0x8DC1
#define GL_TEXTURE_BINDING_2D_ARRAY_NV    0x8C1D
#define GL_MAX_ARRAY_TEXTURE_LAYERS_NV    0x88FF
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER_NV 0x8CD4
typedef void (GL_APIENTRYP PFNGLTEXIMAGE3DNVPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
typedef void (GL_APIENTRYP PFNGLTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
typedef void (GL_APIENTRYP PFNGLCOPYTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLCOMPRESSEDTEXIMAGE3DNVPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
typedef void (GL_APIENTRYP PFNGLCOMPRESSEDTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTURELAYERNVPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glTexImage3DNV (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
GL_APICALL void GL_APIENTRY glTexSubImage3DNV (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
GL_APICALL void GL_APIENTRY glCopyTexSubImage3DNV (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL void GL_APIENTRY glCompressedTexImage3DNV (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
GL_APICALL void GL_APIENTRY glCompressedTexSubImage3DNV (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
GL_APICALL void GL_APIENTRY glFramebufferTextureLayerNV (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
#endif
#endif /* GL_NV_texture_array */

#ifndef GL_NV_texture_compression_latc
#define GL_NV_texture_compression_latc 1
#define GL_COMPRESSED_LUMINANCE_LATC1_NV                0x8C70
#define GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_NV         0x8C71
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_NV          0x8C72
#define GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_NV   0x8C73
#endif /* GL_NV_texture_compression_latc */

#ifndef GL_NV_texture_compression_s3tc
#define GL_NV_texture_compression_s3tc 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_NV    0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_NV   0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_NV   0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_NV   0x83F3
#endif /* GL_NV_texture_compression_s3tc */

#ifndef GL_NV_timer_query
#define GL_NV_timer_query 1
typedef khronos_int64_t GLint64NV;
typedef khronos_uint64_t GLuint64NV;
#define GL_TIME_ELAPSED_NV                                     0x88BF
#define GL_TIMESTAMP_NV                                        0x8E28
typedef void (GL_APIENTRYP PFNGLQUERYCOUNTERNVPROC) (GLuint id, GLenum target);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUI64VNVPROC) (GLuint id, GLenum pname, GLuint64NV *params);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glQueryCounterNV(GLuint id, GLenum target);
GL_APICALL void GL_APIENTRY glGetQueryObjectui64vNV(GLuint id, GLenum pname, GLuint64NV *params);
#endif
#endif /* GL_NV_timer_query */

#ifndef GL_NV_uniform_buffer_object_es2
#define GL_NV_uniform_buffer_object_es2
#define GL_UNIFORM_BUFFER_NV                                  0x8A11
#define GL_UNIFORM_BUFFER_BINDING_NV                          0x8A28
#define GL_UNIFORM_BUFFER_START_NV                            0x8A29
#define GL_UNIFORM_BUFFER_SIZE_NV                             0x8A2A
#define GL_MAX_VERTEX_UNIFORM_BLOCKS_NV                       0x8A2B
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS_NV                     0x8A2D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS_NV                     0x8A2E
#define GL_MAX_UNIFORM_BUFFER_BINDINGS_NV                     0x8A2F
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS_NV          0x8A31
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS_NV        0x8A33
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT_NV                 0x8A34
#define GL_MAX_UNIFORM_BLOCK_SIZE_NV                          0x8A30
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH_NV            0x8A35
#define GL_ACTIVE_UNIFORM_BLOCKS_NV                           0x8A36
#define GL_UNIFORM_TYPE_NV                                    0x8A37
#define GL_UNIFORM_SIZE_NV                                    0x8A38
#define GL_UNIFORM_NAME_LENGTH_NV                             0x8A39
#define GL_UNIFORM_BLOCK_INDEX_NV                             0x8A3A
#define GL_UNIFORM_OFFSET_NV                                  0x8A3B
#define GL_UNIFORM_ARRAY_STRIDE_NV                            0x8A3C
#define GL_UNIFORM_MATRIX_STRIDE_NV                           0x8A3D
#define GL_UNIFORM_IS_ROW_MAJOR_NV                            0x8A3E
#define GL_UNIFORM_BLOCK_BINDING_NV                           0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE_NV                         0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH_NV                       0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS_NV                   0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES_NV            0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER_NV       0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER_NV     0x8A46
#define GL_INVALID_INDEX_NV                                   0xFFFFFFFFu
typedef void (GL_APIENTRYP PFNGLGETUNIFORMINDICESNVPROC) (GLuint program, GLsizei uniformCount, const GLchar * const *uniformNames, GLuint *uniformIndices);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMSIVNVPROC) (GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMNAMENVPROC) (GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName);
typedef GLuint (GL_APIENTRYP PFNGLGETUNIFORMBLOCKINDEXNVPROC) (GLuint program, const GLchar* uniformBlockName);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMBLOCKIVNVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
typedef void (GL_APIENTRYP PFNGLGETACTIVEUNIFORMBLOCKNAMENVPROC) (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
typedef void (GL_APIENTRYP PFNGLBINDBUFFERRANGENVPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GL_APIENTRYP PFNGLBINDBUFFERBASENVPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (GL_APIENTRYP PFNGLGETINTEGERI_VNVPROC) (GLenum target, GLuint index, GLint* data);
typedef void (GL_APIENTRYP PFNGLGETINTEGER64I_VNVPROC) (GLenum target, GLuint index, GLint64NV* data);
typedef void (GL_APIENTRYP PFNGLGETINTEGER64VNVPROC) (GLenum target, GLint64NV* data);
typedef void (GL_APIENTRYP PFNGLUNIFORMBLOCKBINDINGNVPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glGetUniformIndicesNV(GLuint program, GLsizei uniformCount, const GLchar * const *uniformNames, GLuint *uniformIndices);
GL_APICALL void GL_APIENTRY glGetActiveUniformsivNV(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
GL_APICALL void GL_APIENTRY glGetActiveUniformNameNV(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName);
GL_APICALL GLuint GL_APIENTRY glGetUniformBlockIndexNV(GLuint program, const GLchar* uniformBlockName);
GL_APICALL void GL_APIENTRY glGetActiveUniformBlockivNV(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
GL_APICALL void GL_APIENTRY glGetActiveUniformBlockNameNV(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
GL_APICALL void GL_APIENTRY glBindBufferRangeNV(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
GL_APICALL void GL_APIENTRY glBindBufferBaseNV(GLenum target, GLuint index, GLuint buffer);
GL_APICALL void GL_APIENTRY glGetIntegeri_vNV(GLenum target, GLuint index, GLint* data);
GL_APICALL void GL_APIENTRY glGetInteger64i_vNV(GLenum target, GLuint index, GLint64NV* data);
GL_APICALL void GL_APIENTRY glGetInteger64vNV(GLenum target, GLint64NV *data);
GL_APICALL void GL_APIENTRY glUniformBlockBindingNV(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
#endif
#endif /* GL_NV_uniform_buffer_object_es2 */

#ifndef GL_SGIS_texture_lod
#define GL_SGIS_texture_lod 1
#define GL_TEXTURE_MIN_LOD_SGIS           0x813A
#define GL_TEXTURE_MAX_LOD_SGIS           0x813B
#define GL_TEXTURE_BASE_LEVEL_SGIS        0x813C
#define GL_TEXTURE_MAX_LEVEL_SGIS         0x813D
#endif /* GL_SGIS_texture_lod */

#ifdef __cplusplus
}
#endif

#endif /* __gl2ext_nv_h_ */
