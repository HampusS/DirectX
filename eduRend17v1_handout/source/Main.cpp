
#define VSYNC
#define USECONSOLE

#include "stdafx.h"
#include "ShaderBuffers.h"
#include "InputHandler.h"
#include "Camera.h"
#include "Geometry.h"

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE				g_hInst					= nullptr;  
HWND					g_hWnd					= nullptr;

IDXGISwapChain*         g_SwapChain				= nullptr;
ID3D11RenderTargetView* g_RenderTargetView		= nullptr;
ID3D11Texture2D*        g_DepthStencil			= nullptr;
ID3D11DepthStencilView* g_DepthStencilView		= nullptr;
ID3D11Device*			g_Device				= nullptr;
ID3D11DeviceContext*	g_DeviceContext			= nullptr;
ID3D11RasterizerState*	g_RasterState			= nullptr;

ID3D11InputLayout*		g_InputLayout			= nullptr;
ID3D11VertexShader*		g_VertexShader			= nullptr;
ID3D11PixelShader*		g_PixelShader			= nullptr;

ID3D11Buffer*			g_MatrixBuffer = nullptr;
InputHandler*			g_InputHandler = nullptr;

int width, height;
ID3D11Buffer*			g_Light_Buffer = nullptr;
ID3D11Buffer*			g_Phong_Buffer = nullptr;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT             InitWindow( HINSTANCE hInstance, int nCmdShow );
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT				Render(float deltaTime);
HRESULT				Update(float deltaTime);
HRESULT				InitDirect3DAndSwapChain(int width, int height);
void				InitRasterizerState();
HRESULT				CreateRenderTargetView();
HRESULT				CreateDepthStencilView(int width, int height);
void				SetViewport(int width, int height);
HRESULT				CreateShadersAndInputLayout();
void				InitShaderBuffers();
void				Release();

//
// Declarations
//
camera_t* camera;
Cube* cube;
Cube* cube_child;
Cube* cube_grandchild;
OBJModel_t* sun;
OBJModel_t* hand;
// Object model-to-world transformation matrices
float angle = 0;			// A per-frame updated rotation angle (radians)...
float angle_vel = fPI / 2;	// ...and its velocity
// World-to-view matrix
mat4f Mview;
// Projection matrix
mat4f Mproj;

mat4f Mcube;
mat4f Mderivedcube;
mat4f Mderivedchildcube;
mat4f Mhand;
mat4f Msun;

float camera_vel = 5.0f;	// Camera movement velocity in units/s
vec4f lightposition;
int selectMe = 1;

//
// Initialize objects
//
void initObjects()
{
	// Create camera
	camera = new camera_t(fPI/4,				// field-of-view (radians)
						(float)width / height,	// aspect ratio
						1.0f,					// z-near plane (everything closer will be clipped/removed)
						500.0f);				// z-far plane (everything further will be clipped/removed)
	
	// Move camera to (0,0,5)
	// The camera will look toward (0,0,0)  
	camera->moveTo({ 0, 0, 25 });

	// Create objects
	cube = new Cube(g_Device, g_DeviceContext);
	cube_child = new Cube(g_Device, g_DeviceContext);
	cube_grandchild = new Cube(g_Device, g_DeviceContext);
	sun = new OBJModel_t("C:/Users/hampz/Desktop/assets/sphere/sphere.obj", g_Device, g_DeviceContext);
	hand = new OBJModel_t("C:/Users/hampz/Desktop/assets/hand/hand.obj", g_Device, g_DeviceContext);
}

void SendLightBufferToPS(ID3D11Buffer* tempBuff, float4 col, float4 lightpos, float4 camerapos) {
	D3D11_MAPPED_SUBRESOURCE resource;
	g_DeviceContext->Map(tempBuff, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	PointLightBuffer_t* buffer = (PointLightBuffer_t*)resource.pData;
	buffer->my_color = col;
	buffer->my_pos = lightpos;
	buffer->camera_pos = camerapos;

	g_DeviceContext->Unmap(tempBuff, 0);
}

void SendPhongBufferToPS(ID3D11Buffer* tempBuff, float4 amb_color, float4 diffuse_color, float4 spec_color, float shine) {
	D3D11_MAPPED_SUBRESOURCE resource;
	g_DeviceContext->Map(tempBuff, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	PhongBuffer_t* buffer = (PhongBuffer_t*)resource.pData;
	buffer->ambient_color = amb_color;
	buffer->diffuse_color = diffuse_color;
	buffer->specular_color = spec_color;
	//buffer->shine = shine;
	g_DeviceContext->Unmap(tempBuff, 0);
}

//
// Per-frame: read inputs and update object transformations
//
void updateObjects(float dt)
{
	camera->RotateCamera(g_InputHandler->GetMouseDeltaY()* dt, g_InputHandler->GetMouseDeltaX()* dt);
	SendLightBufferToPS(g_Light_Buffer, { 0.1f, 0.1f, 0.5f, 0.5f }, lightposition, { camera->position.x, camera->position.y, camera->position.z, 1 });
	SendPhongBufferToPS(g_Phong_Buffer, { 0.2f, 0.2f, 0.2f, 0.5f }, { 0.1f, 0.4f, 0.1f, 0.5f }, { 0.5f, 0.1f, 0.1f, 0.5f }, 100);
	// Basic camera control from user inputs
	if (g_InputHandler->IsKeyPressed(Keys::Down))
		selectMe = 1;
	if (g_InputHandler->IsKeyPressed(Keys::Up))
		selectMe = 2;

	if (selectMe == 1) {
		if (g_InputHandler->IsKeyPressed(Keys::W))
			camera->move({ 0.0f, 0.0f, -dt });
		if (g_InputHandler->IsKeyPressed(Keys::A))
			camera->move({ -dt, 0.0f, 0.0f });
		if (g_InputHandler->IsKeyPressed(Keys::S))
			camera->move({ 0.0f, 0.0f, dt });
		if (g_InputHandler->IsKeyPressed(Keys::D))
			camera->move({ dt, 0.0f, 0.0f });
	}
	else if (selectMe == 2) {
		if (g_InputHandler->IsKeyPressed(Keys::W))
			lightposition.z += -dt * 25;
		if (g_InputHandler->IsKeyPressed(Keys::A))
			lightposition.x += -dt * 25;
		if (g_InputHandler->IsKeyPressed(Keys::S))
			lightposition.z += dt * 25;
		if (g_InputHandler->IsKeyPressed(Keys::D))
			lightposition.x += dt * 25;
	}

	// Now set/update object transformations
	// This can be done using any sequence of transformation matrices,
	// but the T*R*S order is most common; i.e. scale, then rotate, and then translate.
	// If no transformation is desired, an identity matrix can be obtained 
	// via e.g. Mquad = linalg::mat4f_identity; 

	// Cube
	Mcube = mat4f::translation(0, 7, 0) *					// No translation
		mat4f::rotation(-angle, 0.0f, 1.0f, 0.0f) *		// Rotate continuously around the y-axis
		mat4f::scaling(1.5, 1.5, 1.5);					// Scale uniformly to 150%
	
	Mderivedcube = Mcube * (mat4f::translation(2, 2, 0)* mat4f::rotation(-angle, 1.0f, 0.0f, 0)*mat4f::scaling(1.5, 1.5, 1.5));
	Mderivedchildcube = Mderivedcube* (mat4f::translation(2, 2, 0)* mat4f::rotation(-angle, 0, 1.0f, 0)*mat4f::scaling(1.5, 1.5, 1.5));

	//SUN & HAND
	Msun = mat4f::translation(lightposition.x, lightposition.y, lightposition.z);
	Mhand = mat4f::translation(0, -5, 0) *
		mat4f::rotation(0.0f, 0.0f, 1.0f, 0.0f) *
		mat4f::scaling(15, 15, 15);
	
	// Increase the rotation angle. dt is the frame time step.
	angle += angle_vel * dt;
}

//
// per frame, render object
//
void renderObjects()
{
	// Obtain the matrices needed for rendering from the camera
	Mview = camera->get_WorldToViewMatrix();
	Mproj = camera->get_ProjectionMatrix();
	// CUBE
	cube->MapMatrixBuffers(g_MatrixBuffer, Mcube, Mview, Mproj);
	cube->render();
	cube_child->MapMatrixBuffers(g_MatrixBuffer, Mderivedcube, Mview, Mproj);
	cube_child->render();
	cube_grandchild->MapMatrixBuffers(g_MatrixBuffer, Mderivedchildcube, Mview, Mproj);
	cube_grandchild->render();
	//HAND
	hand->MapMatrixBuffers(g_MatrixBuffer, Mhand, Mview, Mproj);
	hand->render();
	//SUN
	sun->MapMatrixBuffers(g_MatrixBuffer, Msun, Mview, Mproj);
	sun->render();
}

//
// Object deallocation, at program termination
//
void releaseObjects()
{
	SAFE_DELETE(camera);
	SAFE_DELETE(cube);
	SAFE_DELETE(cube_child);
	SAFE_DELETE(cube_grandchild);

}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	// load console and redirect some I/O to it
	// note: this has to be done before the win32 window is initialized, or DirectInput will fail miserably
#ifdef USECONSOLE
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
#endif

	// init the win32 window
	if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
		return 0;

#ifdef USECONSOLE
	printf("Win32-window created...\n");
#ifdef _DEBUG
	printf("Running in DEBUG mode\n");
#else
	printf("Running in RELEASE mode\n");
#endif
#endif

	HRESULT hr = S_OK;
	RECT rc;
	GetClientRect( g_hWnd, &rc );
	width = rc.right - rc.left;
	height = rc.bottom - rc.top;

	if(SUCCEEDED(hr = InitDirect3DAndSwapChain(width, height)))
	{
		InitRasterizerState();

		if (SUCCEEDED(hr = CreateRenderTargetView()) &&
			SUCCEEDED(hr = CreateDepthStencilView(width, height)))
		{
			SetViewport(width, height);

			g_DeviceContext->OMSetRenderTargets( 1, &g_RenderTargetView, g_DepthStencilView );

			if(SUCCEEDED(hr = CreateShadersAndInputLayout()))
			{
				InitShaderBuffers();
				initObjects();
			}
		}
	}

	__int64 cntsPerSec = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&cntsPerSec);
	float secsPerCnt = 1.0f / (float)cntsPerSec;

	__int64 prevTimeStamp = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&prevTimeStamp);

	g_InputHandler = new InputHandler();
	g_InputHandler->Initialize(hInstance, g_hWnd, width, height);

	printf("Entering main loop...\n");

	// Main message loop
	MSG msg = {0};
	while(WM_QUIT != msg.message)
	{
		if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
		{
			__int64 currTimeStamp = 0;
			QueryPerformanceCounter((LARGE_INTEGER*)&currTimeStamp);
			float dt = (currTimeStamp - prevTimeStamp) * secsPerCnt;

			g_InputHandler->Update();
			Update(dt);
			Render(dt);

			prevTimeStamp = currTimeStamp;
		}
	}

	Release();
#ifdef USECONSOLE
	FreeConsole();
#endif
	return (int) msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = 0;
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = nullptr;
	wcex.lpszClassName  = "DA307A_eduRend";
	wcex.hIconSm        = 0;
	if( !RegisterClassEx(&wcex) )
		return E_FAIL;

	// Adjust and create window
	g_hInst = hInstance; 
	RECT rc = { 0, 0, 768, 768 };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	
	if(!(g_hWnd = CreateWindow(
							"DA307A_eduRend",
							"DA307A - eduRend",
							WS_OVERLAPPEDWINDOW,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							rc.right - rc.left,
							rc.bottom - rc.top,
							nullptr,
							nullptr,
							hInstance,
							nullptr)))
	{
		return E_FAIL;
	}

	ShowWindow( g_hWnd, nCmdShow );

	return S_OK;
}

HRESULT CompileShader(char* shaderFile, char* pEntrypoint, char* pTarget, D3D10_SHADER_MACRO* pDefines, ID3DBlob** pCompiledShader)
{
	DWORD dwShaderFlags =	D3DCOMPILE_ENABLE_STRICTNESS |
							D3DCOMPILE_IEEE_STRICTNESS;

	std::string shader_code;
	std::ifstream in(shaderFile, std::ios::in | std::ios::binary);
	if(in)
	{
		in.seekg(0, std::ios::end);
		shader_code.resize((UINT)in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&shader_code[0], shader_code.size());
		in.close();
	}

	ID3DBlob* pErrorBlob = nullptr;
	HRESULT hr = D3DCompile(
		shader_code.data(),
		shader_code.size(),
		nullptr,
		pDefines,
		nullptr,
		pEntrypoint,
		pTarget, 
		dwShaderFlags,
		0,
		pCompiledShader,
		&pErrorBlob);

	if (pErrorBlob)
	{
		// output error message
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
#ifdef USECONSOLE
		printf("%s\n", (char*)pErrorBlob->GetBufferPointer());
#endif
		SAFE_RELEASE(pErrorBlob);
	}

    return hr;
}

HRESULT CreateShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	printf("\nCompiling vertex shader...\n");
	ID3DBlob* pVertexShader = nullptr;
	if(SUCCEEDED(hr = CompileShader("../../assets/shaders/DrawTri.vs", "VS_main", "vs_5_0", nullptr, &pVertexShader)))
	{
		if(SUCCEEDED(hr = g_Device->CreateVertexShader(
			pVertexShader->GetBufferPointer(),
			pVertexShader->GetBufferSize(),
			nullptr,
			&g_VertexShader)))
		{
			D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};

			hr = g_Device->CreateInputLayout(
						inputDesc,
						ARRAYSIZE(inputDesc),
						pVertexShader->GetBufferPointer(),
						pVertexShader->GetBufferSize(),
						&g_InputLayout);
		}

		SAFE_RELEASE(pVertexShader);
	}
	else
	{
		MessageBoxA(nullptr, "Failed to create vertex shader (check Output window for more info)", 0, 0);
	}

	printf("\nCompiling pixel shader...\n\n");
	ID3DBlob* pPixelShader = nullptr;
	if(SUCCEEDED(hr = CompileShader("../../assets/shaders/DrawTri.ps", "PS_main", "ps_5_0", nullptr, &pPixelShader)))
	{
		hr = g_Device->CreatePixelShader(
			pPixelShader->GetBufferPointer(),
			pPixelShader->GetBufferSize(),
			nullptr,
			&g_PixelShader);

		SAFE_RELEASE(pPixelShader);
	}
	else
	{
		MessageBoxA(nullptr, "Failed to create pixel shader (check Output window for more info)", 0, 0);
	}

	return hr;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDirect3DAndSwapChain(int width, int height)
{
	D3D_DRIVER_TYPE driverTypes[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE };

	DXGI_SWAP_CHAIN_DESC sd;
	memset(&sd, 0, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	D3D_FEATURE_LEVEL featureLevelsToTry[] = { D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL initiatedFeatureLevel;

	HRESULT hr = E_FAIL;
	for( UINT driverTypeIndex = 0; driverTypeIndex < ARRAYSIZE(driverTypes) && FAILED(hr); driverTypeIndex++ )
	{
		hr = D3D11CreateDeviceAndSwapChain(
			nullptr,
			driverTypes[driverTypeIndex],
			nullptr,
			0,
			featureLevelsToTry,
			ARRAYSIZE(featureLevelsToTry),
			D3D11_SDK_VERSION,
			&sd,
			&g_SwapChain,
			&g_Device,
			&initiatedFeatureLevel,
			&g_DeviceContext);
	}
#ifdef VSYNC
	g_SwapChain-> Present(1, 0);
#endif

	return hr;
}

void InitRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerState;
	rasterizerState.FillMode = D3D11_FILL_SOLID;
	rasterizerState.CullMode = D3D11_CULL_BACK;
	rasterizerState.FrontCounterClockwise = true;
	rasterizerState.DepthBias = false;
	rasterizerState.DepthBiasClamp = 0;
	rasterizerState.SlopeScaledDepthBias = 0;
	rasterizerState.DepthClipEnable = true;
	rasterizerState.ScissorEnable = false;
	rasterizerState.MultisampleEnable = false;
	rasterizerState.AntialiasedLineEnable = false;

	g_Device->CreateRasterizerState(&rasterizerState, &g_RasterState);
	g_DeviceContext->RSSetState(g_RasterState);
}

void InitShaderBuffers()
{
	HRESULT hr;

	// Matrix buffer
	D3D11_BUFFER_DESC MatrixBuffer_desc = { 0 };
	MatrixBuffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	MatrixBuffer_desc.ByteWidth = sizeof(MatrixBuffer_t);
	MatrixBuffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	MatrixBuffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	MatrixBuffer_desc.MiscFlags = 0;
	MatrixBuffer_desc.StructureByteStride = 0;

	ASSERT(hr = g_Device->CreateBuffer(&MatrixBuffer_desc, nullptr, &g_MatrixBuffer));

	// Light buffer
	D3D11_BUFFER_DESC LightBuffer_desc = { 0 };
	LightBuffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	LightBuffer_desc.ByteWidth = sizeof(PointLightBuffer_t);
	LightBuffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	LightBuffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	LightBuffer_desc.MiscFlags = 0;
	LightBuffer_desc.StructureByteStride = 0;

	ASSERT(hr = g_Device->CreateBuffer(&LightBuffer_desc, nullptr, &g_Light_Buffer));

	// Phong buffer
	D3D11_BUFFER_DESC PhongBuffer_desc = { 1 };
	PhongBuffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	PhongBuffer_desc.ByteWidth = sizeof(PhongBuffer_t);
	PhongBuffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	PhongBuffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	PhongBuffer_desc.MiscFlags = 0;
	PhongBuffer_desc.StructureByteStride = 0;

	ASSERT(hr = g_Device->CreateBuffer(&PhongBuffer_desc, nullptr, &g_Phong_Buffer));
}

HRESULT CreateRenderTargetView()
{
	HRESULT hr = S_OK;
	// Create a render target view
	ID3D11Texture2D* pBackBuffer;
	if(SUCCEEDED(hr = g_SwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&pBackBuffer)))
	{
		hr = g_Device->CreateRenderTargetView( pBackBuffer, nullptr, &g_RenderTargetView );
		SAFE_RELEASE(pBackBuffer);
	}

	return hr;
}

HRESULT CreateDepthStencilView(int width, int height)
{
	HRESULT hr = S_OK;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D32_FLOAT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_Device->CreateTexture2D( &descDepth, nullptr, &g_DepthStencil );
	if( FAILED(hr) )
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_Device->CreateDepthStencilView( g_DepthStencil, &descDSV, &g_DepthStencilView );
	
	return hr;
}

void SetViewport(int width, int height)
{
	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (float)width;
	vp.Height = (float)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_DeviceContext->RSSetViewports( 1, &vp );
}

HRESULT Update(float deltaTime)
{
	updateObjects(deltaTime);

	return S_OK;
}

HRESULT Render(float deltaTime)
{
	//clear back buffer, black color
	static float ClearColor[4] = { 0, 0, 0, 1 };
	g_DeviceContext->ClearRenderTargetView( g_RenderTargetView, ClearColor );
	
	//clear depth buffer
	g_DeviceContext->ClearDepthStencilView( g_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );
	
	//set topology
	g_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); /// D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	
	//set vertex description
	g_DeviceContext->IASetInputLayout(g_InputLayout);
	
	//set shaders
	g_DeviceContext->VSSetShader(g_VertexShader, nullptr, 0);
	g_DeviceContext->HSSetShader(nullptr, nullptr, 0);
	g_DeviceContext->DSSetShader(nullptr, nullptr, 0);
	g_DeviceContext->GSSetShader(nullptr, nullptr, 0);
	g_DeviceContext->PSSetShader(g_PixelShader, nullptr, 0);
	
	// set matrix buffers
	g_DeviceContext->VSSetConstantBuffers(0, 1, &g_MatrixBuffer);
	g_DeviceContext->PSSetConstantBuffers(0, 1, &g_Light_Buffer);
	g_DeviceContext->PSSetConstantBuffers(1, 1, &g_Phong_Buffer);

	// time to render our objects
	renderObjects();

	//swap front and back buffer
	return g_SwapChain->Present( 0, 0 );
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) 
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

		// MSDN: Handling Window Resizing
		// https://msdn.microsoft.com/en-us/library/windows/desktop/bb205075(v=vs.85).aspx#Handling_Window_Resizing
		// See comments for additions (released and re-created a DepthStencilView, updated camera etc)
		//
	case WM_SIZE:
		if (g_SwapChain)
		{
			// Added
			width = (int)LOWORD(lParam);
			height = (int)HIWORD(lParam);

			g_DeviceContext->OMSetRenderTargets(0, 0, 0);

			// Release all outstanding references to the swap chain's buffers.
			g_RenderTargetView->Release();

			HRESULT hr;
			// Preserve the existing buffer count and format.
			// Automatically choose the width and height to match the client rect for HWNDs.
			hr = g_SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

			// Perform error handling here!

			// Get buffer and create a render-target-view.
			ID3D11Texture2D* pBuffer;
			hr = g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
				(void**)&pBuffer);
			// Perform error handling here!

			hr = g_Device->CreateRenderTargetView(pBuffer, NULL,
				&g_RenderTargetView);
			// Perform error handling here!
			pBuffer->Release();

			// Added
			SAFE_RELEASE(g_DepthStencil);
			SAFE_RELEASE(g_DepthStencilView);
			CreateDepthStencilView(width, height);

			// Added
			//g_DeviceContext->OMSetRenderTargets(1, &g_RenderTargetView, NULL);
			g_DeviceContext->OMSetRenderTargets(1, &g_RenderTargetView, g_DepthStencilView);

			// Set up the viewport.
			D3D11_VIEWPORT vp;
			vp.Width = width;
			vp.Height = height;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;
			g_DeviceContext->RSSetViewports(1, &vp);

			// Added
			if (camera)
				camera->aspect = float(width) / height;
		}
		break;

	case WM_KEYDOWN:

		switch(wParam)
		{
			case VK_ESCAPE:
				PostQuitMessage(0);
				break;
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

void Release()
{
	// deallocate objects
	releaseObjects();

	// free D3D stuff
	SAFE_RELEASE(g_SwapChain);
	SAFE_RELEASE(g_RenderTargetView);
	SAFE_RELEASE(g_DepthStencil);
	SAFE_RELEASE(g_DepthStencilView);
	SAFE_RELEASE(g_RasterState);

	SAFE_RELEASE(g_InputLayout);
	SAFE_RELEASE(g_VertexShader);
	SAFE_RELEASE(g_PixelShader);

	SAFE_RELEASE(g_VertexShader);
	SAFE_RELEASE(g_DeviceContext);
	SAFE_RELEASE(g_Device);
}