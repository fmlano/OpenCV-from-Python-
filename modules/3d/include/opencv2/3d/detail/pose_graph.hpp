// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

#ifndef OPENCV_3D_DETAIL_POSE_GRAPH_HPP
#define OPENCV_3D_DETAIL_POSE_GRAPH_HPP

#include "opencv2/core/affine.hpp"
#include "opencv2/core/quaternion.hpp"

namespace cv
{
namespace detail
{

// ATTENTION! This class is used internally in Large KinFu.
// It has been pushed to publicly available headers for tests only.
// Source compatibility of this API is not guaranteed in the future.

// This class provides tools to solve so-called pose graph problem often arisen in SLAM problems
// The pose graph format, cost function and optimization techniques
// repeat the ones used in Ceres 3D Pose Graph Optimization:
// http://ceres-solver.org/nnls_tutorial.html#other-examples, pose_graph_3d.cc bullet
class CV_EXPORTS_W PoseGraph
{
public:
    static Ptr<PoseGraph> create();
    virtual ~PoseGraph();

    // Node may have any id >= 0
    virtual void addNode(size_t _nodeId, const Affine3d& _pose, bool fixed) = 0;
    virtual bool isNodeExist(size_t nodeId) const = 0;
    virtual bool setNodeFixed(size_t nodeId, bool fixed) = 0;
    virtual bool isNodeFixed(size_t nodeId) const = 0;
    virtual Affine3d getNodePose(size_t nodeId) const = 0;
    virtual std::vector<size_t> getNodesIds() const = 0;
    virtual size_t getNumNodes() const = 0;

    // Edges have consequent indices starting from 0
    virtual void addEdge(size_t _sourceNodeId, size_t _targetNodeId, const Affine3f& _transformation,
                         const Matx66f& _information = Matx66f::eye()) = 0;
    virtual size_t getEdgeStart(size_t i) const = 0;
    virtual size_t getEdgeEnd(size_t i) const = 0;
    virtual Affine3d getEdgePose(size_t i) const = 0;
    virtual Matx66f getEdgeInfo(size_t i) const = 0;
    virtual size_t getNumEdges() const = 0;

    // checks if graph is connected and each edge connects exactly 2 nodes
    virtual bool isValid() const = 0;

    // creates an optimizer and returns a pointer on it
    // this function can be omited if a user wants to run optimizer with default settings
    virtual Ptr<BaseLevMarq> createOptimizer() = 0;

    // Returns number of iterations elapsed or -1 if failed to optimize
    virtual BaseLevMarq::Report optimize() = 0;

    // calculate cost function based on current nodes parameters
    virtual double calcEnergy() const = 0;
};

}  // namespace detail
}  // namespace cv

#endif // include guard
