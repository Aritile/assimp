// ---------------------------------------------------------------------------
// Simple Assimp Directx11 Sample
// This is a very basic sample and only reads diffuse texture
// but this can load both embedded textures in fbx and non-embedded textures
//
//
// Replace ourModel->Load(hwnd, dev, devcon, "Models/myModel.fbx") this with your
// model name (line 480)
// If your model isn't a fbx with embedded textures make sure your model's
// textures are in same directory as your model
//
//
// Written by IAS. :)
// ---------------------------------------------------------------------------

#include <assimp/types.h>

#include <Windows.h>
#include <shellapi.h>
#include <stdexcept>
#include <windowsx.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <utf8.h>

#include "ModelLoader.h"
#include "SafeRelease.hpp"

#ifdef _MSC_VER
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "Dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment (lib, "dxguid.lib")
#endif // _MSC_VER

using namespace DirectX;

#define VERTEX_SHADER_FILE L"VertexShader.hlsl"
#define PIXEL_SHADER_FILE L"PixelShader.hlsl"

// ------------------------------------------------------------
//                        Structs
// ------------------------------------------------------------
struct ConstantBuffer {
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};

// ------------------------------------------------------------
//                        Window Variables
// ------------------------------------------------------------
static constexpr uint32_t SCREEN_WIDTH = 800;
static constexpr uint32_t SCREEN_HEIGHT = 600;

constexpr char g_szClassName[] = "directxWindowClass";

static std::string g_ModelPath;

UINT width, height;
HWND g_hwnd = nullptr;

// ------------------------------------------------------------
//                        DirectX Variables
// ------------------------------------------------------------
D3D_DRIVER_TYPE g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device *dev = nullptr;
ID3D11Device1 *dev1 = nullptr;
ID3D11DeviceContext *devcon = nullptr;
ID3D11DeviceContext1 *devcon1 = nullptr;
IDXGISwapChain *swapchain = nullptr;
IDXGISwapChain1 *swapchain1 = nullptr;
ID3D11RenderTargetView *backbuffer = nullptr;
ID3D11VertexShader *pVS = nullptr;
ID3D11PixelShader *pPS = nullptr;
ID3D11InputLayout *pLayout = nullptr;
ID3D11Buffer *pConstantBuffer = nullptr;
ID3D11Texture2D *g_pDepthStencil = nullptr;
ID3D11DepthStencilView *g_pDepthStencilView = nullptr;
ID3D11SamplerState *TexSamplerState = nullptr;
ID3D11RasterizerState *rasterstate = nullptr;
ID3D11Debug* d3d11debug = nullptr;

XMMATRIX m_World;
XMMATRIX m_View;
XMMATRIX m_Projection;

// ------------------------------------------------------------
//                      Function identifiers
// ------------------------------------------------------------

void InitD3D(HINSTANCE hinstance, HWND hWnd);
void CleanD3D(void);
void RenderFrame(void);

void InitPipeline();
void InitGraphics();

HRESULT	CompileShaderFromFile(LPCWSTR pFileName, const D3D_SHADER_MACRO* pDefines, LPCSTR pEntryPoint, LPCSTR pShaderModel, ID3DBlob** ppBytecodeBlob);
void Throwanerror(LPCSTR errormessage);

// ------------------------------------------------------------
//                        Our Model
// ------------------------------------------------------------

ModelLoader *ourModel = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
	LPWSTR /*lpCmdLine*/, int nCmdShow)
{
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv) {
		MessageBox(nullptr,
			TEXT("An error occurred while reading command line arguments."),
			TEXT("Error!"),
			MB_ICONERROR | MB_OK);
		return EXIT_FAILURE;
	}

	// Free memory allocated from CommandLineToArgvW.
	auto free_command_line_allocated_memory = [&argv]() {
		if (argv) {
			LocalFree(argv);
			argv = nullptr;
		}
	};

	// Ensure that a model file has been specified.
	if (argc < 2) {
		MessageBox(nullptr,
			TEXT("No model file specified. The program will now close."),
			TEXT("Error!"),
			MB_ICONERROR | MB_OK);
		free_command_line_allocated_memory();
		return EXIT_FAILURE;
	}

	// Retrieve the model file path.
    std::wstring filename(argv[1]);

	char *targetStart = new char[filename.size()+1];
    memset(targetStart, '\0', filename.size() + 1);

	utf8::utf16to8(filename.c_str(), filename.c_str() + filename.size(), targetStart);
    g_ModelPath = targetStart;
    delete[] targetStart;
	free_command_line_allocated_memory();

	WNDCLASSEX wc;
	MSG msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(nullptr, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	RECT wr = { 0,0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	g_hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		g_szClassName,
		" Simple Textured Directx11 Sample ",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, hInstance, nullptr
	);

	if (g_hwnd == nullptr)
	{
		MessageBox(nullptr, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(g_hwnd, nCmdShow);
	UpdateWindow(g_hwnd);

	width = wr.right - wr.left;
	height = wr.bottom - wr.top;

	try {
		InitD3D(hInstance, g_hwnd);

		while (true)
		{

			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);

				if (msg.message == WM_QUIT)
					break;
			}

			RenderFrame();
		}

		CleanD3D();
		return static_cast<int>(msg.wParam);
	} catch (const std::exception& e) {
		MessageBox(g_hwnd, e.what(), TEXT("Error!"), MB_ICONERROR | MB_OK);
		CleanD3D();
		return EXIT_FAILURE;
	} catch (...) {
		MessageBox(g_hwnd, TEXT("Caught an unknown exception."), TEXT("Error!"), MB_ICONERROR | MB_OK);
		CleanD3D();
		return EXIT_FAILURE;
	}
}

void InitD3D(HINSTANCE /*hinstance*/, HWND hWnd)
{
	HRESULT hr;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &dev, &g_featureLevel, &devcon);

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &dev, &g_featureLevel, &devcon);
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		Throwanerror("Directx Device Creation Failed!");

#if _DEBUG
	hr = dev->QueryInterface(IID_PPV_ARGS(&d3d11debug));
	if (FAILED(hr))
		OutputDebugString(TEXT("Failed to retrieve DirectX 11 debug interface.\n"));
#endif

	UINT m4xMsaaQuality;
	dev->CheckMultisampleQualityLevels(
		DXGI_FORMAT_R8G8B8A8_UNORM, 4, &m4xMsaaQuality);


	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = dev->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		Throwanerror("DXGI Factory couldn't be obtained!");

	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = dev->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&dev1));
		if (SUCCEEDED(hr))
		{
			(void)devcon->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&devcon1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = SCREEN_WIDTH;
		sd.Height = SCREEN_HEIGHT;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 4;
		sd.SampleDesc.Quality = m4xMsaaQuality - 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(dev, hWnd, &sd, nullptr, nullptr, &swapchain1);
		if (SUCCEEDED(hr))
		{
			hr = swapchain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&swapchain));
		}

		dxgiFactory2->Release();
	}
	else
	{
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = SCREEN_WIDTH;
		sd.BufferDesc.Height = SCREEN_HEIGHT;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = m4xMsaaQuality - 1;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(dev, &sd, &swapchain);
	}

	// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER);

	dxgiFactory->Release();

	if (FAILED(hr))
		Throwanerror("Swapchain Creation Failed!");

	ID3D11Texture2D *pBackBuffer;
	swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

	dev->CreateRenderTargetView(pBackBuffer, nullptr, &backbuffer);
	pBackBuffer->Release();

	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = SCREEN_WIDTH;
	descDepth.Height = SCREEN_HEIGHT;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 4;
	descDepth.SampleDesc.Quality = m4xMsaaQuality - 1;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = dev->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
	if (FAILED(hr))
		Throwanerror("Depth Stencil Texture couldn't be created!");

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = dev->CreateDepthStencilView(g_pDepthStencil, 0, &g_pDepthStencilView);
	if (FAILED(hr))
	{
		Throwanerror("Depth Stencil View couldn't be created!");
	}

	devcon->OMSetRenderTargets(1, &backbuffer, g_pDepthStencilView);

	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	dev->CreateRasterizerState(&rasterDesc, &rasterstate);
	devcon->RSSetState(rasterstate);

	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.Width = SCREEN_WIDTH;
	viewport.Height = SCREEN_HEIGHT;

	devcon->RSSetViewports(1, &viewport);

	InitPipeline();
	InitGraphics();
}

void CleanD3D(void)
{
	if (swapchain)
		swapchain->SetFullscreenState(FALSE, nullptr);

	if (ourModel) {
		ourModel->Close();
		delete ourModel;
		ourModel = nullptr;
	}
	SafeRelease(TexSamplerState);
	SafeRelease(pConstantBuffer);
	SafeRelease(pLayout);
	SafeRelease(pVS);
	SafeRelease(pPS);
	SafeRelease(rasterstate);
	SafeRelease(g_pDepthStencilView);
	SafeRelease(g_pDepthStencil);
	SafeRelease(backbuffer);
	SafeRelease(swapchain);
	SafeRelease(swapchain1);
	SafeRelease(devcon1);
	SafeRelease(dev1);
	SafeRelease(devcon);
#if _DEBUG
	if (d3d11debug) {
		OutputDebugString(TEXT("Dumping DirectX 11 live objects.\n"));
		d3d11debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		SafeRelease(d3d11debug);
	} else {
		OutputDebugString(TEXT("Unable to dump live objects: no DirectX 11 debug interface available.\n"));
	}
#endif
	SafeRelease(dev);
}

void RenderFrame(void)
{
	static float t = 0.0f;
	static ULONGLONG timeStart = 0;
	ULONGLONG timeCur = GetTickCount64();
	if (timeStart == 0)
		timeStart = timeCur;
	t = (timeCur - timeStart) / 1000.0f;

	float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
	devcon->ClearRenderTargetView(backbuffer, clearColor);
	devcon->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_World = XMMatrixRotationY(-t);

	ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose(m_World);
	cb.mView = XMMatrixTranspose(m_View);
	cb.mProjection = XMMatrixTranspose(m_Projection);
	devcon->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);

	devcon->VSSetShader(pVS, 0, 0);
	devcon->VSSetConstantBuffers(0, 1, &pConstantBuffer);
	devcon->PSSetShader(pPS, 0, 0);
	devcon->PSSetSamplers(0, 1, &TexSamplerState);
	ourModel->Draw(devcon);

	swapchain->Present(0, 0);
}

void InitPipeline()
{
	ID3DBlob *VS, *PS;
	if(FAILED(CompileShaderFromFile(SHADER_PATH VERTEX_SHADER_FILE, 0, "main", "vs_4_0", &VS)))
		Throwanerror("Failed to compile shader from file");
	if(FAILED(CompileShaderFromFile(SHADER_PATH PIXEL_SHADER_FILE, 0, "main", "ps_4_0", &PS)))
		Throwanerror("Failed to compile shader from file ");

	dev->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &pVS);
	dev->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &pPS);

	D3D11_INPUT_ELEMENT_DESC ied[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	dev->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &pLayout);
	devcon->IASetInputLayout(pLayout);
}

void InitGraphics()
{
	HRESULT hr;

	m_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.01f, 1000.0f);

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;

	hr = dev->CreateBuffer(&bd, nullptr, &pConstantBuffer);
	if (FAILED(hr))
		Throwanerror("Constant buffer couldn't be created");

	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = dev->CreateSamplerState(&sampDesc, &TexSamplerState);
	if (FAILED(hr))
		Throwanerror("Texture sampler state couldn't be created");

	XMVECTOR Eye = XMVectorSet(0.0f, 5.0f, -300.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 100.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_View = XMMatrixLookAtLH(Eye, At, Up);

	ourModel = new ModelLoader;
	if (!ourModel->Load(g_hwnd, dev, devcon, g_ModelPath))
		Throwanerror("Model couldn't be loaded");
}

HRESULT	CompileShaderFromFile(LPCWSTR pFileName, const D3D_SHADER_MACRO* pDefines, LPCSTR pEntryPoint, LPCSTR pShaderModel, ID3DBlob** ppBytecodeBlob)
{
	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;

#ifdef _DEBUG
	compileFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob = nullptr;

	HRESULT result = D3DCompileFromFile(pFileName, pDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, pEntryPoint, pShaderModel, compileFlags, 0, ppBytecodeBlob, &pErrorBlob);
	if (FAILED(result))
	{
		if (pErrorBlob != nullptr)
			OutputDebugStringA((LPCSTR)pErrorBlob->GetBufferPointer());
	}

	if (pErrorBlob != nullptr)
		pErrorBlob->Release();

	return result;
}

void Throwanerror(LPCSTR errormessage)
{
	throw std::runtime_error(errormessage);
}
