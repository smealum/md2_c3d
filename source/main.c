#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdio.h>
#include "texture.h"
#include "md2.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static C3D_Mtx projection;

static C3D_LightEnv lightEnv;
static C3D_Light light;
static C3D_LightLut lut_Phong;

static float angleX = 0.0f, angleY = 0.0f;
static float posX = 0.0f, posY = 0.0f, posZ = 0.0f;

md2_model_t model;
md2_instance_t modelInstance;

static void sceneInit(void)
{
	// #define modelName "sdmc:/content/cube"
	#define modelName "sdmc:/content/ratman"
	// #define modelName "sdmc:/content/glados"
	// #define modelName "sdmc:/content/portalgun"

	md2ReadModel(&model, modelName ".md2");

	md2InstanceInit(&modelInstance, &model, textureCreate(modelName ".png", GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)|GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)|GPU_TEXTURE_WRAP_S(GPU_REPEAT)|GPU_TEXTURE_WRAP_T(GPU_REPEAT), 10));
	// md2InstanceInit(&modelInstance, &model, textureCreate("sdmc:/content/companion.png", GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)|GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)|GPU_TEXTURE_WRAP_S(GPU_REPEAT)|GPU_TEXTURE_WRAP_T(GPU_REPEAT), 10));

	md2InstanceChangeAnimation(&modelInstance, 1, false);

	// texenv for texture * color * fragment lighting
	{
		C3D_TexEnv* env = C3D_GetTexEnv(0);
		C3D_TexEnvSrc(env, C3D_Both, GPU_FRAGMENT_PRIMARY_COLOR, GPU_FRAGMENT_SECONDARY_COLOR, 0);
		C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_ADD);
	}
	{
		C3D_TexEnv* env = C3D_GetTexEnv(1);
		C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PREVIOUS, 0);
		C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}
	{
		C3D_TexEnv* env = C3D_GetTexEnv(2);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PREVIOUS, 0);
		C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}

	static const C3D_Material material =
	{
		{ 0.2f, 0.2f, 0.2f }, //ambient
		{ 0.4f, 0.4f, 0.4f }, //diffuse
		{ 0.8f, 0.8f, 0.8f }, //specular0
		{ 0.0f, 0.0f, 0.0f }, //specular1
		{ 0.0f, 0.0f, 0.0f }, //emission
	};

	C3D_LightEnvInit(&lightEnv);
	C3D_LightEnvBind(&lightEnv);
	C3D_LightEnvMaterial(&lightEnv, &material);

	LightLut_Phong(&lut_Phong, 30);
	C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN, false, &lut_Phong);

	C3D_FVec lightVec = { { 1.0, -0.5, 0.0, 0.0 } };

	C3D_LightInit(&light, &lightEnv);
	C3D_LightColor(&light, 1.0, 1.0, 1.0);
	C3D_LightPosition(&light, &lightVec);
}

static void sceneRender(float iod)
{
	// Compute the projection matrix
	Mtx_PerspStereoTilt(&projection, 40.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 1000.0f, iod, 2.0f, false);

	// Calculate the modelView matrix
	C3D_Mtx modelView;
	Mtx_Identity(&modelView);
	Mtx_Translate(&modelView, posX, posY, posZ - 4.0f, true);
	Mtx_RotateX(&modelView, angleX, true);
	Mtx_RotateY(&modelView, angleY, true);
	Mtx_Scale(&modelView, 0.07f, 0.07f, 0.07f);

	md2InstanceDraw(&modelInstance, &projection, &modelView);
}

static void sceneExit(void)
{
	md2FreeModel(&model);
}

int main()
{
	// Initialize graphics
	gfxInitDefault();
	gfxSet3D(true); // Enable stereoscopic 3D
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	consoleInit(GFX_BOTTOM, NULL);

	textureInit();
	md2Init();

	printf("hello\n");

	// Initialize the render targets
	C3D_RenderTarget* targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTarget* targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(targetLeft,   C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_RenderTargetSetClear(targetRight,  C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_RenderTargetSetOutput(targetLeft,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	// Initialize the scene
	sceneInit();

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		if (kDown & KEY_START) break;

		if (kHeld & KEY_LEFT) angleY += M_PI / 180;
		if (kHeld & KEY_RIGHT) angleY -= M_PI / 180;
		if (kHeld & KEY_UP) angleX += M_PI / 180;
		if (kHeld & KEY_DOWN) angleX -= M_PI / 180;

		if (kHeld & KEY_Y) posX -= 0.05f;
		if (kHeld & KEY_A) posX += 0.05f;
		if (kHeld & KEY_X) posY += 0.05f;
		if (kHeld & KEY_B) posY -= 0.05f;
		if (kHeld & KEY_R) posZ += 0.05f;
		if (kHeld & KEY_L) posZ -= 0.05f;

		float slider = osGet3DSliderState();
		float iod = slider / 3;

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		{
			C3D_FrameDrawOn(targetLeft);
			sceneRender(-iod);

			if (iod > 0.0f)
			{
				C3D_FrameDrawOn(targetRight);
				sceneRender(iod);
			}
		}
		C3D_FrameEnd(0);

		md2InstanceUpdate(&modelInstance);
	}

	// Deinitialize the scene
	sceneExit();

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}
