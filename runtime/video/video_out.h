/* SPDX-License-Identifier: Apache-2.0 */
/* @orig none — trabalho original (quovix), Apache-2.0 */
/* Renderer mínimo da casca: abre SceVideoOut, aloca framebuffers e apresenta
   frames. Primeiro pixel do produto; base pro blit da arte lida do dump em
   runtime. Implementação própria da sequência padrão de video-out do PS4. */
#ifndef QVX_VIDEO_OUT_H
#define QVX_VIDEO_OUT_H

#include <stdint.h>

/* Inicializa o video-out (1920x1080, 2 buffers). 0 em sucesso, <0 em erro. */
int qvx_video_init(void);

int qvx_video_width(void);
int qvx_video_height(void);

/* Ponteiro do back buffer atual (width*height pixels, 32 bits cada). */
uint32_t *qvx_video_backbuffer(void);

/* Preenche o back buffer inteiro com uma cor (0xAARRGGBB). */
void qvx_video_clear(uint32_t argb);

/* Apresenta o back buffer (flip + vblank) e alterna pro próximo. */
void qvx_video_present(void);

/* Blita uma imagem RGBA (0xAARRGGBB, sw*sh) no back buffer em (dx,dy), com
   composição alpha sobre o conteúdo atual e clip nas bordas. */
void qvx_video_blit_rgba(const uint32_t *src, int sw, int sh, int dx, int dy);

/* Blita centralizado no back buffer. */
void qvx_video_blit_centered(const uint32_t *src, int sw, int sh);

#endif
