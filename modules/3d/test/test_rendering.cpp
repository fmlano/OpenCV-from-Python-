// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

#include "test_precomp.hpp"

namespace opencv_test { namespace {
using namespace cv;

// that was easier than using CV_ENUM() macro
namespace
{
using namespace cv;
struct CullingModeEnum
{
    static const std::array<TriangleCullingMode, 3> vals;
    static const std::array<std::string, 3> svals;

    CullingModeEnum(TriangleCullingMode v = TriangleCullingMode::None) : val(v) {}
    operator TriangleCullingMode() const { return val; }
    void PrintTo(std::ostream *os) const
    {
        int v = int(val);
        if (v >= 0 && v < (int)vals.size())
        {
            *os << svals[v];
        }
        else
        {
            *os << "UNKNOWN";
        }
    }
    static ::testing::internal::ParamGenerator<CullingModeEnum> all()
    {
        return ::testing::Values(CullingModeEnum(vals[0]),
                                 CullingModeEnum(vals[1]),
                                 CullingModeEnum(vals[2]));
    }

private:
    TriangleCullingMode val;
};

const std::array<TriangleCullingMode, 3> CullingModeEnum::vals{ TriangleCullingMode::None,
                                                                TriangleCullingMode::CW,
                                                                TriangleCullingMode::CCW
                                                              };
const std::array<std::string, 3> CullingModeEnum::svals{ std::string("None"),
                                                         std::string("CW"),
                                                         std::string("CCW")
                                                       };

static inline void PrintTo(const CullingModeEnum &t, std::ostream *os) { t.PrintTo(os); }
}

// that was easier than using CV_ENUM() macro
namespace
{
using namespace cv;
struct ShadingTypeEnum
{
    static const std::array<TriangleShadingType, 3> vals;
    static const std::array<std::string, 3> svals;

    ShadingTypeEnum(TriangleShadingType v = TriangleShadingType::White) : val(v) {}
    operator TriangleShadingType() const { return val; }
    void PrintTo(std::ostream *os) const
    {
        int v = int(val);
        if (v >= 0 && v < (int)vals.size())
        {
            *os << svals[v];
        }
        else
        {
            *os << "UNKNOWN";
        }
    }
    static ::testing::internal::ParamGenerator<ShadingTypeEnum> all()
    {
        return ::testing::Values(ShadingTypeEnum(vals[0]),
                                 ShadingTypeEnum(vals[1]),
                                 ShadingTypeEnum(vals[2]));
    }

private:
    TriangleShadingType val;
};

const std::array<TriangleShadingType, 3> ShadingTypeEnum::vals{ TriangleShadingType::White,
                                                                TriangleShadingType::Flat,
                                                                TriangleShadingType::Shaded
                                                              };
const std::array<std::string, 3> ShadingTypeEnum::svals{ std::string("White"),
                                                         std::string("Flat"),
                                                         std::string("Shaded")
                                                       };

static inline void PrintTo(const ShadingTypeEnum &t, std::ostream *os) { t.PrintTo(os); }
}


enum class ModelType
{
    Empty = 0,
    File = 1,
    Clipping = 2,
    Color = 3,
    Centered = 4
};

// that was easier than using CV_ENUM() macro
namespace
{
using namespace cv;
struct ModelTypeEnum
{
    static const std::array<ModelType, 5> vals;
    static const std::array<std::string, 5> svals;

    ModelTypeEnum(ModelType v = ModelType::Empty) : val(v) {}
    operator ModelType() const { return val; }
    void PrintTo(std::ostream *os) const
    {
        int v = int(val);
        if (v >= 0 && v < (int)vals.size())
        {
            *os << svals[v];
        }
        else
        {
            *os << "UNKNOWN";
        }
    }
    static ::testing::internal::ParamGenerator<ModelTypeEnum> all()
    {
        return ::testing::Values(ModelTypeEnum(vals[0]),
                                 ModelTypeEnum(vals[1]),
                                 ModelTypeEnum(vals[2]),
                                 ModelTypeEnum(vals[3]),
                                 ModelTypeEnum(vals[4]));
    }

private:
    ModelType val;
};

const std::array<ModelType, 5> ModelTypeEnum::vals{ ModelType::Empty,
                                                    ModelType::File,
                                                    ModelType::Clipping,
                                                    ModelType::Color,
                                                    ModelType::Centered };
const std::array<std::string, 5> ModelTypeEnum::svals{ std::string("Empty"),
                                                       std::string("File"),
                                                       std::string("Clipping"),
                                                       std::string("Color"),
                                                       std::string("Centered") };

static inline void PrintTo(const ModelTypeEnum &t, std::ostream *os) { t.PrintTo(os); }
}

template<typename T>
std::string printEnum(T v)
{
    std::string s;
    std::stringstream ss;
    v.PrintTo(&ss);
    ss >> s;
    return s;
}


static Matx44d lookAtMatrixCal(const Vec3d& position, const Vec3d& lookat, const Vec3d& upVector)
{
    Vec3d w = cv::normalize(position - lookat);
    Vec3d u = cv::normalize(upVector.cross(w));

    Vec3d v = w.cross(u);

    Matx44d res(u[0], u[1], u[2],   0,
                v[0], v[1], v[2],   0,
                w[0], w[1], w[2],   0,
                   0,    0,    0,   1.0);

    Matx44d translate(1.0,   0,   0, -position[0],
                        0, 1.0,   0, -position[1],
                        0,   0, 1.0, -position[2],
                        0,   0,   0,          1.0);
    res = res * translate;

    return res;
}

class ModelData
{
public:
    ModelData(ModelType type = ModelType::Empty)
    {
        switch (type)
        {
        case ModelType::Empty:
        {
            position = Vec3d(0.0, 0.0, 0.0);
            lookat   = Vec3d(0.0, 0.0, 0.0);
            upVector = Vec3d(0.0, 1.0, 0.0);

            fovy = 45.0;

            vertices = std::vector<Vec3f>(4, {2.0f, 0, -2.0f});
            colors   = std::vector<Vec3f>(4, {0, 0, 1.0f});
            indices = { };
        }
        break;
        case ModelType::File:
        {
            string objectPath = findDataFile("rendering/spot.obj");

            position = Vec3d( 2.4, 0.7, 1.2);
            lookat   = Vec3d( 0.0, 0.0, 0.3);
            upVector = Vec3d( 0.0, 1.0, 0.0);

            fovy = 45.0;

            std::vector<vector<int>> indvec;
            // using per-vertex normals as colors
            loadMesh(objectPath, vertices, colors, indvec);
            for (const auto &vec : indvec)
            {
                indices.push_back({vec[0], vec[1], vec[2]});
            }

            for (auto &color : colors)
            {
                color = Vec3f(abs(color[0]), abs(color[1]), abs(color[2]));
            }
        }
        break;
        case ModelType::Clipping:
        {
            position = Vec3d(0.0, 0.0, 5.0);
            lookat   = Vec3d(0.0, 0.0, 0.0);
            upVector = Vec3d(0.0, 1.0, 0.0);

            fovy = 45.0;

            vertices =
            {
                { 2.0,  0.0, -2.0}, { 0.0, -6.0, -2.0}, {-2.0,  0.0, -2.0},
                { 3.5, -1.0, -5.0}, { 2.5, -2.5, -5.0}, {-1.0,  1.0, -5.0},
                {-6.5, -1.0, -3.0}, {-2.5, -2.0, -3.0}, { 1.0,  1.0, -5.0},
            };

            indices = { {0, 1, 2}, {3, 4, 5}, {6, 7, 8} };

            Vec3f col1(217.0, 238.0, 185.0);
            Vec3f col2(185.0, 217.0, 238.0);
            Vec3f col3(150.0,  10.0, 238.0);

            col1 *= (1.f / 255.f);
            col2 *= (1.f / 255.f);
            col3 *= (1.f / 255.f);

            colors =
            {
                col1, col2, col3,
                col2, col3, col1,
                col3, col1, col2,
            };
        }
        break;
        case ModelType::Centered:
        {
            position = Vec3d(0.0, 0.0, 5.0);
            lookat   = Vec3d(0.0, 0.0, 0.0);
            upVector = Vec3d(0.0, 1.0, 0.0);

            fovy = 45.0;

            vertices =
            {
                { 2.0,  0.0, -2.0}, { 0.0, -2.0, -2.0}, {-2.0,  0.0, -2.0},
                { 3.5, -1.0, -5.0}, { 2.5, -1.5, -5.0}, {-1.0,  0.5, -5.0},
            };

            indices = { {0, 1, 2}, {3, 4, 5} };

            Vec3f col1(217.0, 238.0, 185.0);
            Vec3f col2(185.0, 217.0, 238.0);

            col1 *= (1.f / 255.f);
            col2 *= (1.f / 255.f);

            colors =
            {
                col1, col2, col1,
                col2, col1, col2,
            };
        }
        break;
        case ModelType::Color:
        {
            position = Vec3d(0.0, 0.0, 5.0);
            lookat   = Vec3d(0.0, 0.0, 0.0);
            upVector = Vec3d(0.0, 1.0, 0.0);

            fovy = 60.0;

            vertices =
            {
                { 2.0,  0.0, -2.0},
                { 0.0,  2.0, -3.0},
                {-2.0,  0.0, -2.0},
                { 0.0, -2.0,  1.0},
            };

            indices = { {0, 1, 2}, {0, 2, 3} };

            colors =
            {
                { 0.0f, 0.0f, 1.0f},
                { 0.0f, 1.0f, 0.0f},
                { 1.0f, 0.0f, 0.0f},
                { 0.0f, 1.0f, 0.0f},
            };
        }
        break;

        default:
            break;
        }
    }

    Vec3d position;
    Vec3d lookat;
    Vec3d upVector;

    double fovy;

    std::vector<Vec3f> vertices;
    std::vector<Vec3i> indices;
    std::vector<Vec3f> colors;
};


void compareDepth(const cv::Mat& gt, const cv::Mat& mat, cv::Mat& diff, double zFar, double scale,
                  double maskThreshold, double normInfThreshold, double normL2Threshold)
{
    ASSERT_EQ(gt.type(), CV_16UC1);
    ASSERT_EQ(mat.type(), CV_16UC1);
    ASSERT_EQ(gt.size(), mat.size());

    Mat gtMask  = gt  < zFar*scale;
    Mat matMask = mat < zFar*scale;

    Mat diffMask = gtMask != matMask;
    int nzDepthDiff = cv::countNonZero(diffMask);
    EXPECT_LE(nzDepthDiff, maskThreshold);

    Mat jointMask = gtMask & matMask;
    int nzJointMask = cv::countNonZero(jointMask);
    double normInfDepth = cv::norm(gt, mat, cv::NORM_INF, jointMask);
    EXPECT_LE(normInfDepth, normInfThreshold);
    double normL2Depth = nzJointMask ? (cv::norm(gt, mat, cv::NORM_L2, jointMask) / nzJointMask) : 0;
    EXPECT_LE(normL2Depth, normL2Threshold);

    // add --test_debug to output differences
    if (debugLevel > 0)
    {
        std::cout << "nzDepthDiff: "  << nzDepthDiff  << " vs " << maskThreshold << std::endl;
        std::cout << "normInfDepth: " << normInfDepth << " vs " << normInfThreshold << std::endl;
        std::cout << "normL2Depth: "  << normL2Depth  << " vs " << normL2Threshold << std::endl;
    }

    diff = (gt - mat) + (1 << 15);
}


void compareRGB(const cv::Mat& gt, const cv::Mat& mat, cv::Mat& diff, double normInfThreshold, double normL2Threshold)
{
    ASSERT_EQ(gt.type(), CV_32FC3);
    ASSERT_EQ(mat.type(), CV_32FC3);
    ASSERT_EQ(gt.size(), mat.size());

    double normInfRgb = cv::norm(gt, mat, cv::NORM_INF);
    EXPECT_LE(normInfRgb, normInfThreshold);
    double normL2Rgb = cv::norm(gt, mat, cv::NORM_L2) / gt.total();
    EXPECT_LE(normL2Rgb, normL2Threshold);
    // add --test_debug to output differences
    if (debugLevel > 0)
    {
        std::cout << "normInfRgb: " << normInfRgb << " vs " << normInfThreshold << std::endl;
        std::cout << "normL2Rgb: " << normL2Rgb << " vs " << normL2Threshold << std::endl;
    }
    diff = (gt - mat) * 0.5 + 0.5;
}


struct RenderTestThresholds
{
    RenderTestThresholds(
        double _rgbInfThreshold,
        double _rgbL2Threshold,
        double _depthMaskThreshold,
        double _depthInfThreshold,
        double _depthL2Threshold) :
        rgbInfThreshold(_rgbInfThreshold),
        rgbL2Threshold(_rgbL2Threshold),
        depthMaskThreshold(_depthMaskThreshold),
        depthInfThreshold(_depthInfThreshold),
        depthL2Threshold(_depthL2Threshold)
    { }

    double rgbInfThreshold;
    double rgbL2Threshold;
    double depthMaskThreshold;
    double depthInfThreshold;
    double depthL2Threshold;
};

// resolution, shading type, culling mode, model type, float type, index type
typedef std::tuple<std::tuple<int, int>, ShadingTypeEnum, CullingModeEnum, ModelTypeEnum, MatDepth, MatDepth> RenderTestParamType;

class RenderingTest : public ::testing::TestWithParam<RenderTestParamType>
{
protected:
    void SetUp() override
    {
        params = GetParam();
        auto wh = std::get<0>(params);
        width = std::get<0>(wh);
        height = std::get<1>(wh);
        shadingType = std::get<1>(params);
        cullingMode = std::get<2>(params);
        modelType = std::get<3>(params);
        modelData = ModelData(modelType);
        ftype = std::get<4>(params);
        itype = std::get<5>(params);

        zNear = 0.1, zFar = 50.0;
        depthScale = 1000.0;

        depth_buf = Mat(height, width, ftype, zFar);
        color_buf = Mat(height, width, CV_MAKETYPE(ftype, 3), Scalar::all(0));

        cameraPose = lookAtMatrixCal(modelData.position, modelData.lookat, modelData.upVector);
        fovYradians = modelData.fovy * (CV_PI / 180.0);

        verts = Mat(modelData.vertices);
        verts.convertTo(verts, ftype);

        if (shadingType != TriangleShadingType::White)
        {
            colors = Mat(modelData.colors);
            colors.convertTo(colors, ftype);
        }

        indices = Mat(modelData.indices);
        if (itype != CV_32S)
        {
            indices.convertTo(indices, itype);
        }

        settings = TriangleRasterizeSettings().setCullingMode(cullingMode).setShadingType(shadingType);

        triangleRasterize(verts, indices, colors, cameraPose, fovYradians, zNear, zFar,
                          settings, depth_buf, color_buf);
    }

public:
    RenderTestParamType params;
    int width, height;
    double zNear, zFar, depthScale;

    Mat depth_buf, color_buf;

    Mat verts, colors, indices;
    Matx44d cameraPose;
    double fovYradians;
    TriangleRasterizeSettings settings;

    ModelData modelData;
    ModelTypeEnum modelType;
    ShadingTypeEnum shadingType;
    CullingModeEnum cullingMode;
    int ftype, itype;
};


// depth-only or RGB-only rendering should produce the same result as usual rendering
TEST_P(RenderingTest, noArrays)
{
    Mat depthOnly(height, width, ftype, zFar);
    Mat colorOnly(height, width, CV_MAKETYPE(ftype, 3), Scalar::all(0));

    triangleRasterize(verts, indices, colors, cameraPose, fovYradians, zNear, zFar,
                      settings, depthOnly, cv::noArray());
    triangleRasterize(verts, indices, colors, cameraPose, fovYradians, zNear, zFar,
                      settings, cv::noArray(), colorOnly);

    Mat rgbDiff, depthDiff;
    compareRGB(color_buf, colorOnly, rgbDiff, 0, 0);
    depth_buf.convertTo(depth_buf, CV_16U, depthScale);
    depthOnly.convertTo(depthOnly, CV_16U, depthScale);
    compareDepth(depth_buf, depthOnly, depthDiff, zFar, depthScale, 0, 0, 0);

    // add --test_debug to output resulting images
    if (debugLevel > 0)
    {
        std::string modelName   = printEnum(modelType);
        std::string shadingName = printEnum(shadingType);
        std::string cullingName = printEnum(cullingMode);
        std::string suffix = cv::format("%s_%dx%d_Cull%s", modelName.c_str(), width, height, cullingName.c_str());

        std::string outColorPath = "noarray_color_image_" + suffix + "_" + shadingName + ".png";
        std::string outDepthPath = "noarray_depth_image_" + suffix + "_" + shadingName + ".png";

        cvtColor(color_buf, color_buf, COLOR_RGB2BGR);
        cvtColor(rgbDiff, rgbDiff, COLOR_RGB2BGR);
        imwrite(outColorPath, color_buf * 255.f);
        imwrite(outDepthPath, depth_buf);
        imwrite("diff_" + outColorPath, rgbDiff * 255.f);
        imwrite("diff_" + outDepthPath, depthDiff);
    }
}


// passing the same parameters in float should give the same result
TEST_P(RenderingTest, floatParams)
{
    Mat depth_buf2(height, width, ftype, zFar);
    Mat color_buf2(height, width, CV_MAKETYPE(ftype, 3), Scalar::all(0));

    // cameraPose can also be float, checking it
    triangleRasterize(verts, indices, colors, Matx44f(cameraPose), (float)fovYradians, (float)zNear, (float)zFar,
                      settings, depth_buf2, color_buf2);

    Mat rgbDiff, depthDiff;
    compareRGB(color_buf, color_buf2, rgbDiff, 1, 3.76e-05);
    depth_buf.convertTo(depth_buf, CV_16U, depthScale);
    depth_buf2.convertTo(depth_buf2, CV_16U, depthScale);
    compareDepth(depth_buf, depth_buf2, depthDiff, zFar, depthScale, 113, 1, 0.00077);

    // add --test_debug to output resulting images
    if (debugLevel > 0)
    {
        std::string modelName   = printEnum(modelType);
        std::string shadingName = printEnum(shadingType);
        std::string cullingName = printEnum(cullingMode);
        std::string suffix = cv::format("%s_%dx%d_Cull%s", modelName.c_str(), width, height, cullingName.c_str());

        std::string outColorPath = "float_color_image_" + suffix + "_" + shadingName + ".png";
        std::string outDepthPath = "float_depth_image_" + suffix + "_" + shadingName + ".png";

        cvtColor(color_buf, color_buf, COLOR_RGB2BGR);
        cvtColor(rgbDiff, rgbDiff, COLOR_RGB2BGR);
        imwrite(outColorPath, color_buf * 255.f);
        imwrite(outDepthPath, depth_buf);
        imwrite("diff_" + outColorPath, rgbDiff * 255.f);
        imwrite("diff_" + outDepthPath, depthDiff);
    }
}


// some culling options produce the same pictures, let's join them
TriangleCullingMode findSameCulling(ModelType modelType, TriangleShadingType shadingType, TriangleCullingMode cullingMode, bool forRgb)
{
    TriangleCullingMode sameCullingMode = cullingMode;

    if ((modelType == ModelType::Centered && cullingMode == TriangleCullingMode::CCW) ||
        (modelType == ModelType::Color    && cullingMode == TriangleCullingMode::CW)  ||
        (modelType == ModelType::File     && shadingType == TriangleShadingType::White && forRgb) ||
        (modelType == ModelType::File     && cullingMode == TriangleCullingMode::CW))
    {
        sameCullingMode = TriangleCullingMode::None;
    }

    return sameCullingMode;
}

TEST_P(RenderingTest, accuracy)
{
    cvtColor(color_buf, color_buf, cv::COLOR_RGB2BGR);
    depth_buf.convertTo(depth_buf, CV_16U, depthScale);

    if (modelType == ModelType::Empty ||
       (modelType == ModelType::Centered && cullingMode == TriangleCullingMode::CW) ||
       (modelType == ModelType::Color    && cullingMode == TriangleCullingMode::CCW))
    {
        // empty image case
        std::vector<Mat> channels(3);
        split(color_buf, channels);

        for (int i = 0; i < 3; i++)
        {
            EXPECT_EQ(countNonZero(channels[i]), 0);
        }

        Mat depthDiff;
        absdiff(depth_buf, Scalar(zFar * depthScale), depthDiff);
        double sumDepthDiff = sum(depthDiff)[0];
        EXPECT_EQ(sumDepthDiff, 0);
    }
    else
    {
        CullingModeEnum cullingModeRgb   = findSameCulling(modelType, shadingType, cullingMode, true);
        CullingModeEnum cullingModeDepth = findSameCulling(modelType, shadingType, cullingMode, false);

        std::string modelName        = printEnum(modelType);
        std::string shadingName      = printEnum(shadingType);
        std::string cullingName      = printEnum(cullingMode);
        std::string cullingRgbName   = printEnum(cullingModeRgb);
        std::string cullingDepthName = printEnum(cullingModeDepth);

        std::string path = findDataDirectory("rendering");
        std::string suffix      = cv::format("%s_%dx%d_Cull%s", modelName.c_str(), width, height, cullingName.c_str());
        std::string suffixRgb   = cv::format("%s_%dx%d_Cull%s", modelName.c_str(), width, height, cullingRgbName.c_str());
        std::string suffixDepth = cv::format("%s_%dx%d_Cull%s", modelName.c_str(), width, height, cullingDepthName.c_str());
        std::string gtPathColor = path + "/example_image_" + suffixRgb + "_" + shadingName + ".png";
        std::string gtPathDepth = path + "/depth_image_"   + suffixDepth + ".png";

        Mat rgbDiff, depthDiff;
        Mat groundTruthColor = imread(gtPathColor);
        groundTruthColor.convertTo(groundTruthColor, CV_32F, (1.f / 255.f));
        compareRGB(groundTruthColor, color_buf, rgbDiff, 1.0, 5.012e-05);

        Mat groundTruthDepth = imread(gtPathDepth, cv::IMREAD_GRAYSCALE | cv::IMREAD_ANYDEPTH);
        compareDepth(groundTruthDepth, depth_buf, depthDiff, zFar, depthScale, 121.0, 485.0, 0.00681);

        // add --test_debug to output resulting images
        if (debugLevel > 0)
        {
            std::string outColorPath = "color_image_" + suffix + "_" + shadingName + ".png";
            std::string outDepthPath = "depth_image_" + suffix + "_" + shadingName + ".png";
            imwrite(outColorPath, color_buf * 255.f);
            imwrite(outDepthPath, depth_buf);
            imwrite("diff_" + outColorPath, rgbDiff * 255.f);
            imwrite("diff_" + outDepthPath, depthDiff);
        }
    }
}


// drawing model as a whole or as two halves should give the same result
TEST_P(RenderingTest, keepDrawnData)
{
    if (modelType != ModelType::Empty)
    {
        Mat depth_buf2(height, width, ftype, zFar);
        Mat color_buf2(height, width, CV_MAKETYPE(ftype, 3), Scalar::all(0));

        Mat idx1, idx2;
        int nTriangles = (int)indices.total();
        idx1 = indices.reshape(3, 1)(Range::all(), Range(0, nTriangles / 2));
        idx2 = indices.reshape(3, 1)(Range::all(), Range(nTriangles / 2, nTriangles));

        triangleRasterize(verts, idx1, colors, cameraPose, fovYradians, zNear, zFar, settings, depth_buf2, color_buf2);
        triangleRasterize(verts, idx2, colors, cameraPose, fovYradians, zNear, zFar, settings, depth_buf2, color_buf2);

        Mat rgbDiff, depthDiff;
        compareRGB(color_buf, color_buf2, rgbDiff, 0, 0);
        depth_buf.convertTo(depth_buf, CV_16U, depthScale);
        depth_buf2.convertTo(depth_buf2, CV_16U, depthScale);
        compareDepth(depth_buf, depth_buf2, depthDiff, zFar, depthScale, 0, 0, 0);
    }
}


TEST_P(RenderingTest, glCompatibleDepth)
{
    Mat depth_buf2(height, width, ftype, 1.0);

    triangleRasterize(verts, indices, colors, cameraPose, fovYradians, zNear, zFar,
                      settings.setGlCompatibleMode(TriangleGlCompatibleMode::InvertedDepth),
                      depth_buf2, cv::noArray());

    Mat convertedDepth(height, width, ftype);
    // map from [0, 1] to [zNear, zFar]
    double scaleNear = (1.0 / zNear);
    double scaleFar  = (1.0 / zFar);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            double z = (double)depth_buf2.at<float>(y, x);
            convertedDepth.at<float>(y, x) = (float)(1.0 / ((1.0 - z) * scaleNear + z * scaleFar ));
        }
    }

    double normL2Diff = cv::norm(depth_buf, convertedDepth, cv::NORM_L2) / (height * width);
    const double normL2Threshold = 1.e-9;
    EXPECT_LE(normL2Diff, normL2Threshold);
    // add --test_debug to output differences
    if (debugLevel > 0)
    {
        std::cout << "normL2Diff: "  << normL2Diff  << " vs " << normL2Threshold << std::endl;
    }
}


INSTANTIATE_TEST_CASE_P(Rendering, RenderingTest, ::testing::Combine(
    ::testing::Values(std::make_tuple(700, 700), std::make_tuple(640, 480)),
    ShadingTypeEnum::all(),
    CullingModeEnum::all(),
    ModelTypeEnum::all(),
    // float type
    //::testing::Values(CV_32F, CV_64F), // not supported yet
    ::testing::Values(CV_32F), // float type
    // index type
    //::testing::Values(CV_8U, CV_8S, CV_16U, CV_16S, CV_32U, CV_32S) // not supported yet
    ::testing::Values(CV_32S) // not supported yet
));

}
}
