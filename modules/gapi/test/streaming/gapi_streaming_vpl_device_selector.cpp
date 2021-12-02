// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2021 Intel Corporation


#include "../test_precomp.hpp"

#include "../common/gapi_tests_common.hpp"

#include <opencv2/gapi/cpu/core.hpp>
#include <opencv2/gapi/ocl/core.hpp>

#include <opencv2/gapi/streaming/onevpl/source.hpp>

#ifdef HAVE_DIRECTX
#ifdef HAVE_D3D11
#pragma comment(lib,"d3d11.lib")

// get rid of generate macro max/min/etc from DX side
#define D3D11_NO_HELPERS
#define NOMINMAX
#include <d3d11.h>
#include <codecvt>
#include "opencv2/core/directx.hpp"
#undef D3D11_NO_HELPERS
#undef NOMINMAX
#endif // HAVE_D3D11
#endif // HAVE_DIRECTX

#ifdef HAVE_ONEVPL
#include <vpl/mfxvideo.h>
#include "streaming/onevpl/cfg_param_device_selector.hpp"

namespace opencv_test
{
namespace
{

void test_dev_eq(const typename cv::gapi::wip::onevpl::IDeviceSelector::DeviceScoreTable::value_type &scored_device,
                 cv::gapi::wip::onevpl::IDeviceSelector::Score expected_score,
                 cv::gapi::wip::onevpl::AccelType expected_type,
                 cv::gapi::wip::onevpl::Device::Ptr expected_ptr) {
    EXPECT_EQ(std::get<0>(scored_device), expected_score);
    EXPECT_EQ(std::get<1>(scored_device).get_type(), expected_type);
    EXPECT_EQ(std::get<1>(scored_device).get_ptr(), expected_ptr);
}

void test_ctx_eq(const typename cv::gapi::wip::onevpl::IDeviceSelector::DeviceContexts::value_type &ctx,
                 cv::gapi::wip::onevpl::AccelType expected_type,
                 cv::gapi::wip::onevpl::Context::Ptr expected_ptr) {
    EXPECT_EQ(ctx.get_type(), expected_type);
    EXPECT_EQ(ctx.get_ptr(), expected_ptr);
}

void test_host_dev_eq(const typename cv::gapi::wip::onevpl::IDeviceSelector::DeviceScoreTable::value_type &scored_device,
                      cv::gapi::wip::onevpl::IDeviceSelector::Score expected_score) {
    test_dev_eq(scored_device, expected_score,
                cv::gapi::wip::onevpl::AccelType::HOST, nullptr);
}

void test_host_ctx_eq(const typename cv::gapi::wip::onevpl::IDeviceSelector::DeviceContexts::value_type &ctx) {
    test_ctx_eq(ctx, cv::gapi::wip::onevpl::AccelType::HOST, nullptr);
}

TEST(OneVPL_Source_Device_Selector_CfgParam, DefaultDevice)
{
    using namespace cv::gapi::wip::onevpl;
    CfgParamDeviceSelector selector;
    IDeviceSelector::DeviceScoreTable devs = selector.select_devices();
    EXPECT_EQ(devs.size(), 1);
    test_host_dev_eq(*devs.begin(), IDeviceSelector::Score::MaxActivePriority);

    IDeviceSelector::DeviceContexts ctxs = selector.select_context();
    EXPECT_EQ(ctxs.size(), 1);
    test_host_ctx_eq(*ctxs.begin());
}

TEST(OneVPL_Source_Device_Selector_CfgParam, DefaultDeviceWithEmptyCfgParam)
{
    using namespace cv::gapi::wip::onevpl;
    std::vector<CfgParam> empty_params;
    CfgParamDeviceSelector selector(empty_params);
    IDeviceSelector::DeviceScoreTable devs = selector.select_devices();
    EXPECT_EQ(devs.size(), 1);
    test_host_dev_eq(*devs.begin(), IDeviceSelector::Score::MaxActivePriority);
    IDeviceSelector::DeviceContexts ctxs = selector.select_context();
    EXPECT_EQ(ctxs.size(), 1);
    test_host_ctx_eq(*ctxs.begin());
}

TEST(OneVPL_Source_Device_Selector_CfgParam, DefaultDeviceWithAccelNACfgParam)
{
    using namespace cv::gapi::wip::onevpl;
    std::vector<CfgParam> cfg_params_w_no_accel;
    cfg_params_w_no_accel.push_back(CfgParam::create_acceleration_mode(MFX_ACCEL_MODE_NA));
    CfgParamDeviceSelector selector(cfg_params_w_no_accel);
    IDeviceSelector::DeviceScoreTable devs = selector.select_devices();
    EXPECT_EQ(devs.size(), 1);
    test_host_dev_eq(*devs.begin(), IDeviceSelector::Score::MaxActivePriority);

    IDeviceSelector::DeviceContexts ctxs = selector.select_context();
    EXPECT_EQ(ctxs.size(), 1);
    test_host_ctx_eq(*ctxs.begin());
}

#ifdef HAVE_DIRECTX
#ifdef HAVE_D3D11
TEST(OneVPL_Source_Device_Selector_CfgParam, DefaultDeviceWithEmptyCfgParam_DX11_ENABLED)
{
    using namespace cv::gapi::wip::onevpl;
    std::vector<CfgParam> empty_params;
    CfgParamDeviceSelector selector(empty_params);
    IDeviceSelector::DeviceScoreTable devs = selector.select_devices();
    EXPECT_EQ(devs.size(), 1);
    test_host_dev_eq(*devs.begin(), IDeviceSelector::Score::MaxActivePriority);

    IDeviceSelector::DeviceContexts ctxs = selector.select_context();
    EXPECT_EQ(ctxs.size(), 1);
    test_host_ctx_eq(*ctxs.begin());
}

TEST(OneVPL_Source_Device_Selector_CfgParam, DefaultDeviceWithDX11AccelCfgParam_DX11_ENABLED)
{
    using namespace cv::gapi::wip::onevpl;
    std::vector<CfgParam> cfg_params_w_dx11;
    cfg_params_w_dx11.push_back(CfgParam::create_acceleration_mode(MFX_ACCEL_MODE_VIA_D3D11));
    std::unique_ptr<CfgParamDeviceSelector> selector_ptr;
    EXPECT_NO_THROW(selector_ptr.reset(new CfgParamDeviceSelector(cfg_params_w_dx11)));
    IDeviceSelector::DeviceScoreTable devs = selector_ptr->select_devices();

    EXPECT_EQ(devs.size(), 1);
    test_dev_eq(*devs.begin(), IDeviceSelector::Score::MaxActivePriority,
                AccelType::DX11,
                std::get<1>(*devs.begin()).get_ptr() /* compare just type */);

    IDeviceSelector::DeviceContexts ctxs = selector_ptr->select_context();
    EXPECT_EQ(ctxs.size(), 1);
    EXPECT_TRUE(ctxs.begin()->get_ptr());
}

TEST(OneVPL_Source_Device_Selector_CfgParam, NULLDeviceWithDX11AccelCfgParam_DX11_ENABLED)
{
    using namespace cv::gapi::wip::onevpl;
    std::vector<CfgParam> cfg_params_w_dx11;
    cfg_params_w_dx11.push_back(CfgParam::create_acceleration_mode(MFX_ACCEL_MODE_VIA_D3D11));
    Device::Ptr empty_device_ptr = nullptr;
    Context::Ptr empty_ctx_ptr = nullptr;
    EXPECT_THROW(CfgParamDeviceSelector sel(empty_device_ptr, "GPU",
                                            empty_ctx_ptr,
                                            cfg_params_w_dx11),
                 std::logic_error); // empty_device_ptr must be invalid
}

TEST(OneVPL_Source_Device_Selector_CfgParam, ExternalDeviceWithDX11AccelCfgParam_DX11_ENABLED)
{
    using namespace cv::gapi::wip::onevpl;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext* device_context = nullptr;
    {
        UINT flags = 0;
        D3D_FEATURE_LEVEL features[] = { D3D_FEATURE_LEVEL_11_1,
                                         D3D_FEATURE_LEVEL_11_0,
                                       };
        D3D_FEATURE_LEVEL feature_level;

        // Create the Direct3D 11 API device object and a corresponding context.
        HRESULT err = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE,
                                        nullptr, flags,
                                        features,
                                        ARRAYSIZE(features), D3D11_SDK_VERSION,
                                        &device, &feature_level, &device_context);
        EXPECT_FALSE(FAILED(err));
    }

    std::unique_ptr<CfgParamDeviceSelector> selector_ptr;
    std::vector<CfgParam> cfg_params_w_dx11;
    cfg_params_w_dx11.push_back(CfgParam::create_acceleration_mode(MFX_ACCEL_MODE_VIA_D3D11));
    EXPECT_NO_THROW(selector_ptr.reset(new CfgParamDeviceSelector(device, "GPU",
                                                                  device_context,
                                                                  cfg_params_w_dx11)));
    IDeviceSelector::DeviceScoreTable devs = selector_ptr->select_devices();

    EXPECT_EQ(devs.size(), 1);
    test_dev_eq(*devs.begin(), IDeviceSelector::Score::MaxActivePriority,
                AccelType::DX11, device);

    IDeviceSelector::DeviceContexts ctxs = selector_ptr->select_context();
    EXPECT_EQ(ctxs.size(), 1);
    EXPECT_EQ(reinterpret_cast<ID3D11DeviceContext*>(ctxs.begin()->get_ptr()),
              device_context);
}

#endif // HAVE_D3D11
#endif // HAVE_DIRECTX

#ifndef HAVE_DIRECTX
#ifndef HAVE_D3D11
TEST(OneVPL_Source_Device_Selector_CfgParam, DX11DeviceFromCfgParamWithDX11Disabled)
{
    using namespace cv::gapi::wip::onevpl;
    std::vector<CfgParam> cfg_params_w_non_existed_dx11;
    cfg_params_w_not_existed_dx11.push_back(CfgParam::create_acceleration_mode(MFX_ACCEL_MODE_VIA_D3D11));
    EXPECT_THROW(CfgParamDeviceSelector{cfg_params_w_non_existed_dx11},
                 std::logic_error);
}
#endif // HAVE_D3D11
#endif // HAVE_DIRECTX

TEST(OneVPL_Source_Device_Selector_CfgParam, UnknownPtrDeviceFromCfgParam)
{
    using namespace cv::gapi::wip::onevpl;
    std::vector<CfgParam> empty_params;
    Device::Ptr empty_device_ptr = nullptr;
    Context::Ptr empty_ctx_ptr = nullptr;
    EXPECT_THROW(CfgParamDeviceSelector sel(empty_device_ptr, "",
                                            empty_ctx_ptr,
                                            empty_params),
                 std::logic_error); // params must describe device_ptr explicitly
}
}
} // namespace opencv_test
#endif // HAVE_ONEVPL
