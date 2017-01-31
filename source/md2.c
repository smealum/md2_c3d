/*
 * md2.c -- md2 model loader
 * last modification: aug. 14, 2007
 *
 * Copyright(c) 2005-2007 David HENRY
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files(the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * gcc -Wall -ansi -lGL -lGLU -lglut md2.c -o md2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>
#include "md2.h"
#include "texture.h"
#include "md2_shbin.h"

// #include "md2_vsh_shbin.h"

u8 normalPermutation[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 100, 54, 55, 56, 57, 58, 59, 60, 61, 62, 82, 63, 97, 64, 98, 101, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 93, 75, 94, 76, 87, 84, 77, 95, 92, 96, 88, 90, 89, 91, 85, 83, 86, 105, 104, 106, 102, 103, 99, 115, 114, 116, 110, 112, 111, 113, 108, 107, 109, 117, 119, 118, 123, 124, 125, 126, 122, 120, 121, 142, 143, 153, 145, 151, 152, 149, 148, 150, 159, 157, 158, 155, 154, 156, 78, 79, 80, 136, 147, 134, 146, 133, 144, 137, 138, 139, 128, 135, 130, 140, 131, 141, 162, 161, 160, 127, 132, 81};

DVLB_s* md2Dvlb;
shaderProgram_s md2Program;
extern u32 __linear_heap;
#define md2BaseAddr __linear_heap

int md2UniformModelview;
int md2UniformProjection;
int md2UniformScale0;
int md2UniformTranslation0;
int md2UniformScale1;
int md2UniformTranslation1;
int md2UniformFrameParam;

int md2GsMode;

void md2Init()
{
	md2Dvlb = DVLB_ParseFile((u32*)md2_shbin, md2_shbin_size);

	if(!md2Dvlb)return;

	shaderProgramInit(&md2Program);
	shaderProgramSetVsh(&md2Program, &md2Dvlb->DVLE[0]);
	C3D_BindProgram(&md2Program);

	// grab uniform locations
	md2UniformModelview = shaderInstanceGetUniformLocation(md2Program.vertexShader, "modelview");
	md2UniformProjection = shaderInstanceGetUniformLocation(md2Program.vertexShader, "projection");
	md2UniformScale0 = shaderInstanceGetUniformLocation(md2Program.vertexShader, "scale0");
	md2UniformTranslation0 = shaderInstanceGetUniformLocation(md2Program.vertexShader, "translation0");
	md2UniformScale1 = shaderInstanceGetUniformLocation(md2Program.vertexShader, "scale1");
	md2UniformTranslation1 = shaderInstanceGetUniformLocation(md2Program.vertexShader, "translation1");
	md2UniformFrameParam = shaderInstanceGetUniformLocation(md2Program.vertexShader, "frameParam");

	// attributes
	// GPU_ATTRIBFMT(0, 4, GPU_UNSIGNED_BYTE)|GPU_ATTRIBFMT(1, 4, GPU_UNSIGNED_BYTE)|GPU_ATTRIBFMT(2, 2, GPU_SHORT), // we want v0 (n1 frame vertex pos), v1 (n2 frame vertex pos) and v2 (texcoord)
}

void md2Exit()
{
	// gsUnregisterRenderMode(md2GsMode);
	// shaderProgramFree(&md2Program);
	// DVLB_Free(md2Dvlb);
}

// destructive
void trimName(char* name)
{
	if(!name)return;
	while(*name)
	{
		if(*name>='0' && *name<='9')
		{
			*name=0;
			return;
		}
		name++;
	}
}

void md2ComputeAnimations(md2_model_t* mdl)
{
	if(!mdl || !mdl->frames || !mdl->header.num_frames)return;

	int i, n=1;
	char* oldstr=mdl->frames[0].name;

	for(i=0; i<mdl->header.num_frames; i++)
	{
		md2_frame_t *pframe=&mdl->frames[i];
		trimName(pframe->name);
		if(strcmp(pframe->name,oldstr))n++;
		oldstr=pframe->name;
	}

	mdl->num_animations=n;
	mdl->animations=malloc(sizeof(md2_anim_t)*mdl->num_animations);

	n=0;
	mdl->animations[0].start=0;
	oldstr=mdl->frames[0].name;

	for(i=0;i<mdl->header.num_frames;i++)
	{
		md2_frame_t *pframe=&mdl->frames[i];
		if(strcmp(pframe->name,oldstr))
		{
			mdl->animations[n++].end=i-1;
			mdl->animations[n].start=i;
		}
		oldstr=pframe->name;
	}

	mdl->animations[n].end=i-1;
	
	for(i=0;i<mdl->num_animations;i++)
	{
		int j;
		u16 n=mdl->animations[i].end-mdl->animations[i].start+1;
		for(j=0;j<n;j++)
		{
			mdl->frames[mdl->animations[i].start+j].next = mdl->animations[i].start+((j+1)%n);
		}
	}
}

int md2ReadModel(md2_model_t *mdl, const char *filename)
{
	if(!mdl)return -1;

	FILE *fp;
	int i, j, k;

	fp = fopen(filename, "rb");
	if(!fp)
	{
		printf("Error: couldn't open \"%s\"!\n", filename);
		return 0;
	}

	/* Read header */
	fread(&mdl->header, 1, sizeof(md2_header_t), fp);

	if((mdl->header.ident != 844121161) ||(mdl->header.version != 8))
	{
		/* Error! */
		printf("Error: bad version or identifier\n");
		fclose(fp);
		return 0;
	}

	/* Memory allocations */
	mdl->skins = (md2_skin_t*) malloc(sizeof(md2_skin_t)*mdl->header.num_skins);
	mdl->texcoords = (md2_texCoord_t*) malloc(sizeof(md2_texCoord_t)*mdl->header.num_st);
	mdl->triangles = (md2_triangle_t*) malloc(sizeof(md2_triangle_t)*mdl->header.num_tris);
	mdl->frames = (md2_frame_t*) malloc(sizeof(md2_frame_t)*mdl->header.num_frames);

	/* Read model data */
	fseek(fp, mdl->header.offset_skins, SEEK_SET);
	fread(mdl->skins, sizeof(md2_skin_t), mdl->header.num_skins, fp);

	fseek(fp, mdl->header.offset_st, SEEK_SET);
	fread(mdl->texcoords, sizeof(md2_texCoord_t), mdl->header.num_st, fp);

	fseek(fp, mdl->header.offset_tris, SEEK_SET);
	fread(mdl->triangles, sizeof(md2_triangle_t), mdl->header.num_tris, fp);

	mdl->permutation = (md2_vertperm_t*) malloc(sizeof(md2_vertperm_t)*mdl->header.num_tris*3);
	mdl->indices = (u16*) linearAlloc(mdl->header.num_tris * 3 * sizeof(u16));

	// not really efficient but it only happens once at load time so it should be ok-ish
	// could be improved by using a better-suited data structure for lookups
	mdl->permutation_size = 0;
	for(i = 0; i < mdl->header.num_tris; i++)
	{
		for(j = 0; j < 3; j++)
		{
			u16 v = mdl->triangles[i].vertex[j];
			u16 st = mdl->triangles[i].st[j];

			for(k = 0; k < mdl->permutation_size; k++)
			{
				if(mdl->permutation[k].v == v && (mdl->permutation[k].st == st || (mdl->texcoords[mdl->permutation[k].st].s == mdl->texcoords[st].s && mdl->texcoords[mdl->permutation[k].st].t == mdl->texcoords[st].t)))break;
			}

			if(k == mdl->permutation_size) mdl->permutation[mdl->permutation_size++] = (md2_vertperm_t){v, st};
			mdl->indices[i * 3 + j] = k;
		}
	}

	mdl->permutation = (md2_vertperm_t*) realloc(mdl->permutation, sizeof(md2_vertperm_t)*mdl->permutation_size);
	// printf("permutation %d vs %d vs %d vs %d\n", (int)mdl->permutation_size, (int)mdl->header.num_tris*3, (int)mdl->header.num_vertices, (int)mdl->header.num_st);

	// permute texcoords
	md2_texCoord_t* tmp_texcoords = mdl->texcoords;
	mdl->header.num_st = mdl->permutation_size;
	mdl->texcoords = (md2_texCoord_t*) linearAlloc(sizeof(md2_texCoord_t)*mdl->header.num_st);
	for(i = 0; i < mdl->header.num_st; i++) mdl->texcoords[i] = tmp_texcoords[mdl->permutation[i].st];
	free(tmp_texcoords);

	/* Read frames */
	fseek(fp, mdl->header.offset_frames, SEEK_SET);
	md2_vertex_t* tmp_vertices = (md2_vertex_t*) malloc(sizeof(md2_vertex_t)*mdl->header.num_vertices);
	for(i = 0; i < mdl->header.num_frames; ++i)
	{
		/* Memory allocation for vertices of this frame */
		mdl->frames[i].verts=(md2_vertex_t*)linearAlloc(sizeof(md2_vertex_t)*mdl->permutation_size);

		/* Read frame data */
		fread(&mdl->frames[i].scale, sizeof(vect3Df_s), 1, fp);
		fread(&mdl->frames[i].translate, sizeof(vect3Df_s), 1, fp);
		fread(&mdl->frames[i].name, sizeof(char), 16, fp);
		fread(tmp_vertices, sizeof(md2_vertex_t), mdl->header.num_vertices, fp);

		for(j=0; j < mdl->header.num_vertices; j++) tmp_vertices[j].normalIndex = normalPermutation[tmp_vertices[j].normalIndex];
		for(j=0; j < mdl->permutation_size; j++) mdl->frames[i].verts[j] = tmp_vertices[mdl->permutation[j].v];
	}

	mdl->header.num_vertices = mdl->permutation_size;

	fclose(fp);

	md2ComputeAnimations(mdl);

	printf("successfully read md2\n");

	return 1;
}

void md2FreeModel(md2_model_t *mdl)
{
	if(!mdl)return;

	if(mdl->skins)
	{
		free(mdl->skins);
		mdl->skins = NULL;
	}

	if(mdl->texcoords)
	{
		linearFree(mdl->texcoords);
		mdl->texcoords = NULL;
	}

	if(mdl->triangles)
	{
		free(mdl->triangles);
		mdl->triangles = NULL;
	}

	if(mdl->permutation)
	{
		free(mdl->permutation);
		mdl->permutation = NULL;
	}

	if(mdl->indices)
	{
		linearFree(mdl->indices);
		mdl->indices = NULL;
	}

	if(mdl->frames)
	{
		int i;
		for(i = 0; i < mdl->header.num_frames; ++i)
		{
			linearFree(mdl->frames[i].verts);
			mdl->frames[i].verts = NULL;
		}
		free(mdl->frames);
		mdl->frames = NULL;
	}
}

void GPU_SetTextureEnable(GPU_TEXUNIT units)
{
	GPUCMD_AddMaskedWrite(0x006F, 0x2, units << 8); 			// enables texcoord outputs
	GPUCMD_AddWrite(GPUREG_TEXUNIT_CONFIG, 0x00011000 | units);	// enables texture units
}

void md2StartDrawing()
{
// 	gsSetShader(&md2Program);
	C3D_BindProgram(&md2Program);

	// attribute config
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_UNSIGNED_BYTE, 4); // v0: frame0 position
	AttrInfo_AddLoader(attrInfo, 1, GPU_UNSIGNED_BYTE, 4); // v1: frame1 position
	AttrInfo_AddLoader(attrInfo, 2, GPU_SHORT, 2); // v2: texcoord

	C3D_CullFace(GPU_CULL_FRONT_CCW);

	GPU_SetTextureEnable(GPU_TEXUNIT0);
}

void md2RenderFrame(md2_model_t *mdl, int n1, int n2, float interp, float alpha, float brightness, texture_s* t, C3D_Mtx* projection, C3D_Mtx* modelView)
{
	if(!mdl)return;

	md2StartDrawing();

	n1 %= mdl->header.num_frames;
	n2 %= mdl->header.num_frames;

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, md2UniformProjection, projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, md2UniformModelview, modelView);

	C3D_FVUnifSet(GPU_VERTEX_SHADER, md2UniformScale0, mdl->frames[n1].scale.x, mdl->frames[n1].scale.y, mdl->frames[n1].scale.z, brightness);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, md2UniformTranslation0, mdl->frames[n1].translate.x, mdl->frames[n1].translate.y, mdl->frames[n1].translate.z, 1.0f);

	C3D_FVUnifSet(GPU_VERTEX_SHADER, md2UniformScale1, mdl->frames[n2].scale.x, mdl->frames[n2].scale.y, mdl->frames[n2].scale.z, 1.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, md2UniformTranslation1, mdl->frames[n2].translate.x, mdl->frames[n2].translate.y, mdl->frames[n2].translate.z, 1.0f);

	C3D_FVUnifSet(GPU_VERTEX_SHADER, md2UniformFrameParam, interp, 1.0f / mdl->header.skinwidth, 1.0f / mdl->header.skinheight, alpha);

	// gsUpdateTransformation();

	textureBind(t, GPU_TEXUNIT0);

	// buffer config
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, mdl->frames[n1].verts, 4 * sizeof(u8), 1, 0x0);
	BufInfo_Add(bufInfo, mdl->frames[n2].verts, 4 * sizeof(u8), 1, 0x1);
	BufInfo_Add(bufInfo, mdl->texcoords, 2 * sizeof(u16), 1, 0x2);

	C3D_DrawElements(GPU_TRIANGLES, mdl->header.num_tris * 3, 1, mdl->indices); // 1 == short indices
}

void md2InstanceInit(md2_instance_t* mi, md2_model_t* mdl, texture_s* t)
{
	if(!mi || !mdl) return;

	mi->currentAnim = 0;
	mi->currentFrame = 0;
	mi->interpolation = 0.0f;
	mi->speed = 0.25f;
	mi->alpha = 1.0f;
	mi->brightness = 1.0f;
	mi->nextFrame = 0;
	mi->texture = t;
	mi->model = mdl;
}

void md2InstanceChangeAnimation(md2_instance_t* mi, u16 newAnim, bool oneshot)
{
	if(!mi || mi->currentAnim == newAnim || newAnim >= mi->model->num_animations) return;
	if(!oneshot && mi->oneshot)
	{
		mi->oldAnim=newAnim;
		return;
	}
	mi->oneshot = oneshot;
	mi->oldAnim = mi->currentAnim;
	mi->currentAnim = newAnim;
	mi->nextFrame = mi->model->animations[mi->currentAnim].start;
}

void md2InstanceUpdate(md2_instance_t* mi)
{
	if(!mi) return;
	if(!mi->oneshot) mi->oldAnim = mi->currentAnim;
	mi->interpolation += mi->speed;
	if(mi->interpolation >= 1.0f)
	{
		mi->interpolation = 0.0f;
		mi->currentFrame = mi->nextFrame;
		if(mi->currentFrame>=mi->model->animations[mi->currentAnim].end)
		{
			if(mi->oneshot)
			{
				u8 oa = mi->currentAnim;
				mi->currentAnim = mi->oldAnim;
				mi->oldAnim = oa;
				mi->oneshot = false;
			}
			mi->nextFrame = mi->model->animations[mi->currentAnim].start;
		}else mi->nextFrame++;
	}
}

void md2InstanceDraw(md2_instance_t* mi, C3D_Mtx* projection, C3D_Mtx* modelView)
{
	if(!mi || !mi->model || !mi->texture) return;

	// gsPushMatrix();
	// 	gsScale(1.0f/6, 1.0f/6, 1.0f/6);
	// 	gsRotateX(M_PI/2);
		// md2RenderFrame(mi->model, mi->currentFrame, mi->nextFrame, mi->interpolation, mi->alpha, mi->brightness, mi->texture);
		md2RenderFrame(mi->model, mi->currentFrame, mi->nextFrame, mi->interpolation, mi->alpha, mi->brightness, mi->texture, projection, modelView);
	// gsPopMatrix();
}
