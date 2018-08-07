#include "GameApp.h"
#include <filesystem>
#include <algorithm>

using namespace DirectX;
using namespace std::experimental;

GameApp::GameApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

GameApp::~GameApp()
{
}

bool GameApp::Init()
{
	if (!D3DApp::Init())
		return false;

	if (!InitEffect())
		return false;

	if (!InitResource())
		return false;

	// ��ʼ����꣬���̲���Ҫ
	mMouse->SetWindow(mhMainWnd);
	mMouse->SetMode(DirectX::Mouse::MODE_RELATIVE);

	return true;
}

void GameApp::OnResize()
{
	assert(md2dFactory);
	assert(mdwriteFactory);
	// �ͷ�D2D�������Դ
	mColorBrush.Reset();
	md2dRenderTarget.Reset();

	D3DApp::OnResize();

	// ΪD2D����DXGI������ȾĿ��
	ComPtr<IDXGISurface> surface;
	HR(mSwapChain->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(surface.GetAddressOf())));
	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
	HR(md2dFactory->CreateDxgiSurfaceRenderTarget(surface.Get(), &props, md2dRenderTarget.GetAddressOf()));

	surface.Reset();
	// �����̶���ɫˢ���ı���ʽ
	HR(md2dRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White),
		mColorBrush.GetAddressOf()));
	HR(mdwriteFactory->CreateTextFormat(L"����", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15, L"zh-cn",
		mTextFormat.GetAddressOf()));
	
	// ����������ʾ
	if (mConstantBuffers[3] != nullptr)
	{
		mCamera->SetFrustum(XM_PIDIV2, AspectRatio(), 0.5f, 1000.0f);
		mCBOnReSize.proj = mCamera->GetProj();
		md3dImmediateContext->UpdateSubresource(mConstantBuffers[3].Get(), 0, nullptr, &mCBOnReSize, 0, 0);
		md3dImmediateContext->VSSetConstantBuffers(3, 1, mConstantBuffers[3].GetAddressOf());
	}
}

void GameApp::UpdateScene(float dt)
{

	// ��������¼�����ȡ���ƫ����
	Mouse::State mouseState = mMouse->GetState();
	Mouse::State lastMouseState = mMouseTracker.GetLastState();
	mMouseTracker.Update(mouseState);

	Keyboard::State keyState = mKeyboard->GetState();
	mKeyboardTracker.Update(keyState);

	// ��ȡ����
	auto cam3rd = std::dynamic_pointer_cast<ThirdPersonCamera>(mCamera);
	auto cam1st = std::dynamic_pointer_cast<FirstPersonCamera>(mCamera);
	
	if (mCameraMode == CameraMode::FirstPerson || mCameraMode == CameraMode::Free)
	{
		// ��һ�˳�/����������Ĳ���

		// �����ƶ�
		if (keyState.IsKeyDown(Keyboard::W))
		{
			if (mCameraMode == CameraMode::FirstPerson)
				cam1st->Walk(dt * 3.0f);
			else
				cam1st->MoveForward(dt * 3.0f);
		}
		if (keyState.IsKeyDown(Keyboard::S))
		{
			if (mCameraMode == CameraMode::FirstPerson)
				cam1st->Walk(dt * -3.0f);
			else
				cam1st->MoveForward(dt * -3.0f);
		}
		if (keyState.IsKeyDown(Keyboard::A))
			cam1st->Strafe(dt * -3.0f);
		if (keyState.IsKeyDown(Keyboard::D))
			cam1st->Strafe(dt * 3.0f);

		// ��λ��������[-8.9f, 8.9f]��������
		// ����������
		XMFLOAT3 adjustedPos;
		XMStoreFloat3(&adjustedPos, cam1st->GetPositionXM());
		cam1st->SetPosition(adjustedPos);

		// ���ڵ�һ�˳�ģʽ�ƶ�����
		if (mCameraMode == CameraMode::FirstPerson)
			mWireFence.SetWorldMatrix(XMMatrixTranslation(adjustedPos.x, adjustedPos.y, adjustedPos.z));
		// ��Ұ��ת����ֹ��ʼ�Ĳ�ֵ�����µ�ͻȻ��ת
		cam1st->Pitch(mouseState.y * dt * 1.25f);
		cam1st->RotateY(mouseState.x * dt * 1.25f);
	}
	else if (mCameraMode == CameraMode::ThirdPerson)
	{
		// �����˳�������Ĳ���

		cam3rd->SetTarget(mWireFence.GetPosition());

		// ��������ת
		cam3rd->RotateX(mouseState.y * dt * 1.25f);
		cam3rd->RotateY(mouseState.x * dt * 1.25f);
		cam3rd->Approach(-mouseState.scrollWheelValue / 120 * 1.0f);
	}

	// ���¹۲����
	mCamera->UpdateViewMatrix();
	XMStoreFloat4(&mCBFrame.eyePos, mCamera->GetPositionXM());
	mCBFrame.view = mCamera->GetView();

	// ���ù���ֵ
	mMouse->ResetScrollWheelValue();

	// �����ģʽ�л�
	
	if (keyState.IsKeyDown(Keyboard::D1) && mCameraMode != CameraMode::ThirdPerson)
	{
		if (!cam3rd)
		{
			cam3rd.reset(new ThirdPersonCamera);
			cam3rd->SetFrustum(XM_PIDIV2, AspectRatio(), 0.5f, 1000.0f);
			mCamera = cam3rd;
		}
		XMFLOAT3 target = mWireFence.GetPosition();
		cam3rd->SetTarget(target);
		cam3rd->SetDistance(8.0f);
		cam3rd->SetDistanceMinMax(3.0f, 20.0f);
		// ��ʼ��ʱ������󷽿�
		// cam3rd->RotateY(-XM_PIDIV2);

		mCameraMode = CameraMode::ThirdPerson;
	}
	else if (keyState.IsKeyDown(Keyboard::D2) && mCameraMode != CameraMode::Free)
	{
		if (!cam1st)
		{
			cam1st.reset(new FirstPersonCamera);
			cam1st->SetFrustum(XM_PIDIV2, AspectRatio(), 0.5f, 1000.0f);
			mCamera = cam1st;
		}
		// �������Ϸ���ʼ
		XMFLOAT3 pos = mWireFence.GetPosition();
		XMFLOAT3 look{ 0.0f, 0.0f, 1.0f };
		XMFLOAT3 up{ 0.0f, 1.0f, 0.0f };
		pos.y += 3;
		cam1st->LookTo(pos, look, up);

		mCameraMode = CameraMode::Free;
	}
	
	// �˳���������Ӧ�򴰿ڷ���������Ϣ
	if (keyState.IsKeyDown(Keyboard::Escape))
		SendMessage(MainWnd(), WM_DESTROY, 0, 0);
	
	md3dImmediateContext->UpdateSubresource(mConstantBuffers[2].Get(), 0, nullptr, &mCBFrame, 0, 0);
}

void GameApp::DrawScene()
{
	assert(md3dImmediateContext);
	assert(mSwapChain);

	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView.Get(), reinterpret_cast<const float*>(&Colors::Black));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	
	
	// *********************
	// 1. �����淴������д��ֵ1��ģ�建����
	// 

	// �ü�������������
	// ��Ǿ��������ģ��ֵΪ1
	// ��д��������ɫ
	md3dImmediateContext->RSSetState(nullptr);
	md3dImmediateContext->OMSetDepthStencilState(RenderStates::DSSMarkMirror.Get(), 1);
	md3dImmediateContext->OMSetBlendState(RenderStates::BSNoColorWrite.Get(), nullptr, 0xFFFFFFFF);


	mMirror.Draw(md3dImmediateContext);

	// ***********************
	// 2. ���Ʋ�͸���ķ�������
	//

	// �����������
	XMINT4 reflectionState = { 1, 0, 0, 0 };
	md3dImmediateContext->UpdateSubresource(mConstantBuffers[1].Get(), 0, nullptr, &reflectionState, 0, 0);
	
	// ���Ʋ�͸�����壬��Ҫ˳ʱ��ü�
	// ����ģ��ֵΪ1�ľ����������
	md3dImmediateContext->RSSetState(RenderStates::RSCullClockWise.Get());
	md3dImmediateContext->OMSetDepthStencilState(RenderStates::DSSDrawReflection.Get(), 1);
	md3dImmediateContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	
	mWalls[2].Draw(md3dImmediateContext);
	mWalls[3].Draw(md3dImmediateContext);
	mWalls[4].Draw(md3dImmediateContext);
	mFloor.Draw(md3dImmediateContext);

	// ***********************
	// 3. ����͸���ķ�������
	//

	// �ر�˳��ʱ��ü�
	// ����ģ��ֵΪ1�ľ����������
	// ͸�����
	md3dImmediateContext->RSSetState(RenderStates::RSNoCull.Get());
	md3dImmediateContext->OMSetDepthStencilState(RenderStates::DSSDrawReflection.Get(), 1);
	md3dImmediateContext->OMSetBlendState(RenderStates::BSTransparent.Get(), nullptr, 0xFFFFFFFF);

	mWireFence.Draw(md3dImmediateContext);
	mWater.Draw(md3dImmediateContext);
	mMirror.Draw(md3dImmediateContext);
	
	// �رշ������
	reflectionState.x = 0;
	md3dImmediateContext->UpdateSubresource(mConstantBuffers[1].Get(), 0, nullptr, &reflectionState, 0, 0);
	

	// ************************
	// 4. ���Ʋ�͸������������
	//

	md3dImmediateContext->RSSetState(nullptr);
	md3dImmediateContext->OMSetDepthStencilState(nullptr, 0);
	md3dImmediateContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	for (auto& wall : mWalls)
		wall.Draw(md3dImmediateContext);
	mFloor.Draw(md3dImmediateContext);

	// ***********************
	// 5. ����͸������������
	//

	// �ر�˳��ʱ��ü�
	// ͸�����
	md3dImmediateContext->RSSetState(RenderStates::RSNoCull.Get());
	md3dImmediateContext->OMSetDepthStencilState(nullptr, 0);
	md3dImmediateContext->OMSetBlendState(RenderStates::BSTransparent.Get(), nullptr, 0xFFFFFFFF);

	mWireFence.Draw(md3dImmediateContext);
	mWater.Draw(md3dImmediateContext);

	//
	// ����Direct2D����
	//
	md2dRenderTarget->BeginDraw();
	std::wstring text = L"�л������ģʽ: 1-�����˳� 2-�����ӽ�\n"
		"W/S/A/D ǰ��/����/��ƽ��/��ƽ�� (�����˳���Ч)  Esc�˳�\n"
		"����ƶ�������Ұ ���ֿ��Ƶ����˳ƹ۲����\n"
		"��ǰģʽ: ";
	if (mCameraMode == CameraMode::ThirdPerson)
		text += L"�����˳�";
	else
		text += L"�����ӽ�";
	md2dRenderTarget->DrawTextW(text.c_str(), (UINT32)text.length(), mTextFormat.Get(),
		D2D1_RECT_F{ 0.0f, 0.0f, 500.0f, 60.0f }, mColorBrush.Get());
	HR(md2dRenderTarget->EndDraw());

	HR(mSwapChain->Present(0, 0));
}

HRESULT GameApp::CompileShaderFromFile(const WCHAR * szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob ** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// ���� D3DCOMPILE_DEBUG ��־���ڻ�ȡ��ɫ��������Ϣ���ñ�־���������������飬
	// ����Ȼ������ɫ�������Ż�����
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// ��Debug�����½����Ż��Ա������һЩ�����������
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> errorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, errorBlob.GetAddressOf());
	if (FAILED(hr))
	{
		if (errorBlob != nullptr)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		return hr;
	}


	return S_OK;
}



bool GameApp::InitEffect()
{
	ComPtr<ID3DBlob> blob;

	// �Ѿ�����õ���ɫ���ļ���
	std::wstring pso2DPath = L"HLSL\\Basic_PS_2D.pso", vso2DPath = L"HLSL\\Basic_VS_2D.vso";
	std::wstring pso3DPath = L"HLSL\\Basic_PS_3D.pso", vso3DPath = L"HLSL\\Basic_VS_3D.vso";
	// ******************************************************
	// Ѱ���Ƿ����Ѿ�����õĶ�����ɫ��(2D)�������������ڱ���
	if (filesystem::exists(vso2DPath))
	{
		HR(D3DReadFileToBlob(vso2DPath.c_str(), blob.GetAddressOf()));
	}
	else
	{
		HR(CompileShaderFromFile(L"HLSL\\Basic.fx", "VS_2D", "vs_5_0", blob.GetAddressOf()));
	}
	// ����������ɫ��(2D)
	HR(md3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, mVertexShader2D.GetAddressOf()));
	// �������㲼��(2D)
	HR(md3dDevice->CreateInputLayout(VertexPosNormalTex::inputLayout, ARRAYSIZE(VertexPosNormalTex::inputLayout),
		blob->GetBufferPointer(), blob->GetBufferSize(), mVertexLayout2D.GetAddressOf()));

	blob.Reset();
	
	// ******************************************************
	// Ѱ���Ƿ����Ѿ�����õĶ�����ɫ��(3D)�������������ڱ���
	if (filesystem::exists(vso3DPath))
	{
		HR(D3DReadFileToBlob(vso3DPath.c_str(), blob.GetAddressOf()));
	}
	else
	{
		HR(CompileShaderFromFile(L"HLSL\\Basic.fx", "VS_3D", "vs_5_0", blob.GetAddressOf()));
	}
	// ����������ɫ��(3D)
	HR(md3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, mVertexShader3D.GetAddressOf()));
	// �������㲼��(3D)
	HR(md3dDevice->CreateInputLayout(VertexPosNormalTex::inputLayout, ARRAYSIZE(VertexPosNormalTex::inputLayout),
		blob->GetBufferPointer(), blob->GetBufferSize(), mVertexLayout3D.GetAddressOf()));
	blob.Reset();

	// ******************************************************
	// Ѱ���Ƿ����Ѿ�����õ�������ɫ��(2D)�������������ڱ���
	if (filesystem::exists(pso2DPath))
	{
		HR(D3DReadFileToBlob(pso2DPath.c_str(), blob.GetAddressOf()));
	}
	else
	{
		HR(CompileShaderFromFile(L"HLSL\\Basic.fx", "PS_2D", "ps_5_0", blob.GetAddressOf()));
	}
	// ����������ɫ��(2D)
	HR(md3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, mPixelShader2D.GetAddressOf()));
	blob.Reset();

	// ******************************************************
	// Ѱ���Ƿ����Ѿ�����õ�������ɫ��(3D)�������������ڱ���
	if (filesystem::exists(pso3DPath))
	{
		HR(D3DReadFileToBlob(pso3DPath.c_str(), blob.GetAddressOf()));
	}
	else
	{
		HR(CompileShaderFromFile(L"HLSL\\Basic.fx", "PS_3D", "ps_5_0", blob.GetAddressOf()));
	}
	// ����������ɫ��(3D)
	HR(md3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, mPixelShader3D.GetAddressOf()));
	blob.Reset();

	return true;
}

bool GameApp::InitResource()
{
	
	// ******************
	// ���ó�������������
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = 0;
	// �½�����VS��PS�ĳ���������
	cbd.ByteWidth = sizeof(CBChangesEveryDrawing);
	HR(md3dDevice->CreateBuffer(&cbd, nullptr, mConstantBuffers[0].GetAddressOf()));
	cbd.ByteWidth = 16;
	HR(md3dDevice->CreateBuffer(&cbd, nullptr, mConstantBuffers[1].GetAddressOf()));
	cbd.ByteWidth = sizeof(CBChangesEveryFrame);
	HR(md3dDevice->CreateBuffer(&cbd, nullptr, mConstantBuffers[2].GetAddressOf()));
	cbd.ByteWidth = sizeof(CBChangesOnResize);
	HR(md3dDevice->CreateBuffer(&cbd, nullptr, mConstantBuffers[3].GetAddressOf()));
	cbd.ByteWidth = sizeof(CBNeverChange);
	HR(md3dDevice->CreateBuffer(&cbd, nullptr, mConstantBuffers[4].GetAddressOf()));
	// ******************
	// ��ʼ����Ϸ����
	ComPtr<ID3D11ShaderResourceView> texture;
	Material material;
	material.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);
	// ��ʼ����ʺ�
	HR(CreateDDSTextureFromFile(md3dDevice.Get(), L"Texture\\WireFence.dds", nullptr, texture.GetAddressOf()));
	mWireFence.SetBuffer(md3dDevice, Geometry::CreateBox());
	// ̧��߶ȱ�����Ȼ�������Դ����
	mWireFence.SetWorldMatrix(XMMatrixTranslation(0.0f, 0.01f, 7.5f));
	mWireFence.SetTexTransformMatrix(XMMatrixIdentity());
	mWireFence.SetTexture(texture);
	mWireFence.SetMaterial(material);
	
	

	// ��ʼ���ذ�
	HR(CreateDDSTextureFromFile(md3dDevice.Get(), L"Texture\\floor.dds", nullptr, texture.ReleaseAndGetAddressOf()));
	mFloor.SetBuffer(md3dDevice, 
		Geometry::CreatePlane(XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(20.0f, 20.0f), XMFLOAT2(5.0f, 5.0f)));
	mFloor.SetWorldMatrix(XMMatrixIdentity());
	mFloor.SetTexTransformMatrix(XMMatrixIdentity());
	mFloor.SetTexture(texture);
	mFloor.SetMaterial(material);

	// ��ʼ��ǽ��
	mWalls.resize(5);
	HR(CreateDDSTextureFromFile(md3dDevice.Get(), L"Texture\\brick.dds", nullptr, texture.ReleaseAndGetAddressOf()));
	// �������ǽ�����������ɣ�0��1���м�λ�����ڷ��þ���
	//     ____     ____
	//    /| 0 |   | 1 |\
	//   /4|___|___|___|2\
	//  /_/_ _ _ _ _ _ _\_\
	// | /       3       \ |
	// |/_________________\|
	//
	for (int i = 0; i < 5; ++i)
	{
		mWalls[i].SetMaterial(material);
		mWalls[i].SetTexTransformMatrix(XMMatrixIdentity());
		mWalls[i].SetTexture(texture);
	}
	mWalls[0].SetBuffer(md3dDevice, Geometry::CreatePlane(XMFLOAT3(), XMFLOAT2(6.0f, 8.0f), XMFLOAT2(1.5f, 2.0f)));
	mWalls[1].SetBuffer(md3dDevice, Geometry::CreatePlane(XMFLOAT3(), XMFLOAT2(6.0f, 8.0f), XMFLOAT2(1.5f, 2.0f)));
	mWalls[2].SetBuffer(md3dDevice, Geometry::CreatePlane(XMFLOAT3(), XMFLOAT2(20.0f, 8.0f), XMFLOAT2(5.0f, 2.0f)));
	mWalls[3].SetBuffer(md3dDevice, Geometry::CreatePlane(XMFLOAT3(), XMFLOAT2(20.0f, 8.0f), XMFLOAT2(5.0f, 2.0f)));
	mWalls[4].SetBuffer(md3dDevice, Geometry::CreatePlane(XMFLOAT3(), XMFLOAT2(20.0f, 8.0f), XMFLOAT2(5.0f, 2.0f)));
	
	mWalls[0].SetWorldMatrix(XMMatrixRotationX(-XM_PIDIV2) * XMMatrixTranslation(-7.0f, 3.0f, 10.0f));
	mWalls[1].SetWorldMatrix(XMMatrixRotationX(-XM_PIDIV2) * XMMatrixTranslation(7.0f, 3.0f, 10.0f));
	mWalls[2].SetWorldMatrix(XMMatrixRotationY(-XM_PIDIV2) * XMMatrixRotationZ(XM_PIDIV2) * XMMatrixTranslation(10.0f, 3.0f, 0.0f));
	mWalls[3].SetWorldMatrix(XMMatrixRotationX(XM_PIDIV2) * XMMatrixTranslation(0.0f, 3.0f, -10.0f));
	mWalls[4].SetWorldMatrix(XMMatrixRotationY(XM_PIDIV2) * XMMatrixRotationZ(-XM_PIDIV2) * XMMatrixTranslation(-10.0f, 3.0f, 0.0f));
	
		
	// ��ʼ��ˮ
	material.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	material.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 32.0f);
	HR(CreateDDSTextureFromFile(md3dDevice.Get(), L"Texture\\water.dds", nullptr, texture.ReleaseAndGetAddressOf()));
	mWater.SetBuffer(md3dDevice,
		Geometry::CreatePlane(XMFLOAT3(), XMFLOAT2(20.0f, 20.0f), XMFLOAT2(10.0f, 10.0f)));
	mWater.SetWorldMatrix(XMMatrixIdentity());
	mWater.SetTexTransformMatrix(XMMatrixIdentity());
	mWater.SetTexture(texture);
	mWater.SetMaterial(material);

	// ��ʼ������
	material.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	material.Specular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);
	HR(CreateDDSTextureFromFile(md3dDevice.Get(), L"Texture\\ice.dds", nullptr, texture.ReleaseAndGetAddressOf()));
	mMirror.SetBuffer(md3dDevice,
		Geometry::CreatePlane(XMFLOAT3(), XMFLOAT2(8.0f, 8.0f), XMFLOAT2(1.0f, 1.0f)));
	mMirror.SetWorldMatrix(XMMatrixRotationX(-XM_PIDIV2) * XMMatrixTranslation(0.0f, 3.0f, 10.0f));
	mMirror.SetTexTransformMatrix(XMMatrixIdentity());
	mMirror.SetTexture(texture);
	mMirror.SetMaterial(material);

	// ��ʼ��������״̬
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR(md3dDevice->CreateSamplerState(&sampDesc, mSamplerState.GetAddressOf()));

	
	// ******************
	// ��ʼ��������������ֵ
	// ��ʼ��ÿ֡���ܻ�仯��ֵ
	mCameraMode = CameraMode::ThirdPerson;
	auto camera = std::shared_ptr<ThirdPersonCamera>(new ThirdPersonCamera);
	mCamera = camera;
	
	camera->SetTarget(XMFLOAT3(0.0f, 0.5f, 0.0f));
	camera->SetDistance(5.0f);
	camera->SetDistanceMinMax(2.0f, 14.0f);
	mCBFrame.view = mCamera->GetView();
	XMStoreFloat4(&mCBFrame.eyePos, mCamera->GetPositionXM());

	// ��ʼ�����ڴ��ڴ�С�䶯ʱ�޸ĵ�ֵ
	mCamera->SetFrustum(XM_PI / 3, AspectRatio(), 0.5f, 1000.0f);
	mCBOnReSize.proj = mCamera->GetProj();

	// ��ʼ������仯��ֵ
	mCBNeverChange.reflection = XMMatrixReflect(XMVectorSet(0.0f, 0.0f, -1.0f, 10.0f));
	// ������
	mCBNeverChange.dirLight[0].Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mCBNeverChange.dirLight[0].Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	mCBNeverChange.dirLight[0].Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mCBNeverChange.dirLight[0].Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
	// �ƹ�
	mCBNeverChange.pointLight[0].Position = XMFLOAT3(0.0f, 15.0f, 0.0f);
	mCBNeverChange.pointLight[0].Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mCBNeverChange.pointLight[0].Diffuse = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
	mCBNeverChange.pointLight[0].Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mCBNeverChange.pointLight[0].Att = XMFLOAT3(0.0f, 0.1f, 0.0f);
	mCBNeverChange.pointLight[0].Range = 25.0f;
	mCBNeverChange.numDirLight = 1;
	mCBNeverChange.numPointLight = 1;
	mCBNeverChange.numSpotLight = 0;
	


	// ���²����ױ��޸ĵĳ�����������Դ
	md3dImmediateContext->UpdateSubresource(mConstantBuffers[3].Get(), 0, nullptr, &mCBOnReSize, 0, 0);
	md3dImmediateContext->UpdateSubresource(mConstantBuffers[4].Get(), 0, nullptr, &mCBNeverChange, 0, 0);

	// ��ʼ��������Ⱦ״̬
	RenderStates::InitAll(md3dDevice);
	
	
	// ******************************
	// ���ú���Ⱦ���߸��׶�������Դ

	md3dImmediateContext->IASetInputLayout(mVertexLayout3D.Get());
	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	md3dImmediateContext->VSSetShader(mVertexShader3D.Get(), nullptr, 0);
	// Ԥ�Ȱ󶨸�������Ļ�����������ÿ֡���µĻ�������Ҫ�󶨵�������������
	md3dImmediateContext->VSSetConstantBuffers(0, 1, mConstantBuffers[0].GetAddressOf());
	md3dImmediateContext->VSSetConstantBuffers(1, 1, mConstantBuffers[1].GetAddressOf());
	md3dImmediateContext->VSSetConstantBuffers(2, 1, mConstantBuffers[2].GetAddressOf());
	md3dImmediateContext->VSSetConstantBuffers(3, 1, mConstantBuffers[3].GetAddressOf());
	md3dImmediateContext->VSSetConstantBuffers(4, 1, mConstantBuffers[4].GetAddressOf());


	md3dImmediateContext->PSSetConstantBuffers(0, 1, mConstantBuffers[0].GetAddressOf());
	md3dImmediateContext->PSSetConstantBuffers(2, 1, mConstantBuffers[2].GetAddressOf());
	md3dImmediateContext->PSSetConstantBuffers(4, 1, mConstantBuffers[4].GetAddressOf());
	md3dImmediateContext->PSSetShader(mPixelShader3D.Get(), nullptr, 0);
	md3dImmediateContext->PSSetSamplers(0, 1, mSamplerState.GetAddressOf());
	return true;
}



DirectX::XMFLOAT3 GameApp::GameObject::GetPosition() const
{
	return XMFLOAT3(mWorldMatrix(3, 0), mWorldMatrix(3, 1), mWorldMatrix(3, 2));
}


void GameApp::GameObject::SetBuffer(ComPtr<ID3D11Device> device, const Geometry::MeshData& meshData)
{
	// �ͷž���Դ
	mVertexBuffer.Reset();
	mIndexBuffer.Reset();

	// ���ö��㻺��������
	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = (UINT)meshData.vertexVec.size() * sizeof(VertexPosNormalTex);
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	// �½����㻺����
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = meshData.vertexVec.data();
	HR(device->CreateBuffer(&vbd, &InitData, mVertexBuffer.GetAddressOf()));


	// ������������������
	mIndexCount = (int)meshData.indexVec.size();
	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(ibd));
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(WORD) * mIndexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	// �½�����������
	InitData.pSysMem = meshData.indexVec.data();
	HR(device->CreateBuffer(&ibd, &InitData, mIndexBuffer.GetAddressOf()));



}

void GameApp::GameObject::SetTexture(ComPtr<ID3D11ShaderResourceView> texture)
{
	mTexture = texture;
}

void GameApp::GameObject::SetMaterial(const Material & material)
{
	mMaterial = material;
}

void GameApp::GameObject::SetWorldMatrix(const XMFLOAT4X4 & world)
{
	mWorldMatrix = world;
}

void GameApp::GameObject::SetWorldMatrix(FXMMATRIX world)
{
	XMStoreFloat4x4(&mWorldMatrix, world);
}

void GameApp::GameObject::SetTexTransformMatrix(const DirectX::XMFLOAT4X4 & texTransform)
{
	mWorldMatrix = mTexTransform;
}

void GameApp::GameObject::SetTexTransformMatrix(DirectX::FXMMATRIX texTransform)
{
	XMStoreFloat4x4(&mTexTransform, texTransform);
}

void GameApp::GameObject::Draw(ComPtr<ID3D11DeviceContext> deviceContext)
{
	// ���ö���/����������
	UINT strides = sizeof(VertexPosNormalTex);
	UINT offsets = 0;
	deviceContext->IASetVertexBuffers(0, 1, mVertexBuffer.GetAddressOf(), &strides, &offsets);
	deviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

	// ��ȡ֮ǰ�Ѿ��󶨵���Ⱦ�����ϵĳ����������������޸�
	ComPtr<ID3D11Buffer> cBuffer = nullptr;
	deviceContext->VSGetConstantBuffers(0, 1, cBuffer.GetAddressOf());
	CBChangesEveryDrawing mCBDrawing;
	mCBDrawing.world = XMLoadFloat4x4(&mWorldMatrix);
	mCBDrawing.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, mCBDrawing.world));
	mCBDrawing.texTransform = XMLoadFloat4x4(&mTexTransform);
	mCBDrawing.material = mMaterial;
	deviceContext->UpdateSubresource(cBuffer.Get(), 0, nullptr, &mCBDrawing, 0, 0);
	// ��������
	deviceContext->PSSetShaderResources(0, 1, mTexture.GetAddressOf());
	// ���Կ�ʼ����
	deviceContext->DrawIndexed(mIndexCount, 0, 0);
}