// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html

#include "precomp.hpp"
#include "octree.hpp"
#include "opencv2/3d.hpp"

namespace cv{

void getPointRecurse(std::vector<Point3f> &restorePointCloud, std::vector<Point3f> &restoreColor, size_t x_key, size_t y_key,
                     size_t z_key, Ptr<OctreeNode> &_node, double resolution, Point3f ori, bool hasColor);

// Locate the OctreeNode corresponding to the input point from the given OctreeNode.
static Ptr<OctreeNode> index(const Point3f& point, Ptr<OctreeNode>& node, OctreeKey& key, size_t depthMask);

OctreeNode::OctreeNode() :
    children(),
    depth(0),
    size(0),
    origin(0,0,0),
    pointNum(0),
    neigh(),
    parentIndex(-1)
{ }

OctreeNode::OctreeNode(int _depth, double _size, const Point3f &_origin, const Point3f &_color,
                       int _parentIndex) :
    children(),
    depth(_depth),
    size(_size),
    origin(_origin),
    color(_color),
    pointNum(0),
    neigh(),
    parentIndex(_parentIndex)
{ }

bool OctreeNode::empty() const
{
    if(this->isLeaf)
    {
        if(this->pointList.empty())
            return true;
        else
            return false;
    }
    else
    {
        for(size_t i = 0; i < 8; i++)
        {
            if(!this->children[i].empty())
            {
                return false;
            }
        }
        return true;
    }
}


bool OctreeNode::isPointInBound(const Point3f& _point) const
{
    Point3f eps;
    eps.x = std::max(std::abs(_point.x), std::abs(this->origin.x));
    eps.y = std::max(std::abs(_point.y), std::abs(this->origin.y));
    eps.z = std::max(std::abs(_point.z), std::abs(this->origin.z));
    eps *= std::numeric_limits<float>::epsilon();
    Point3f ptEps = _point + eps;
    Point3f upPt = this->origin + eps + Point3f {(float)this->size, (float)this->size, (float)this->size};

    return (ptEps.x >= this->origin.x) && (ptEps.y >= this->origin.y) && (ptEps.z >= this->origin.z) &&
           (_point.x <= upPt.x) && (_point.y <= upPt.y) && (_point.z <= upPt.z);
}

struct Octree::Impl
{
public:
    Impl():maxDepth(0), size(0), origin(0,0,0), resolution(0)
    {}

    ~Impl()
    {}

    void fill(bool useResolution, const std::vector<Point3f>& pointCloud, const std::vector<Point3f>& colorAttribute);
    bool insertPoint(const Point3f& point, const Point3f &color);

    // The pointer to Octree root node.
    Ptr <OctreeNode> rootNode = nullptr;
    //! Max depth of the Octree. And depth must be greater than zero
    int maxDepth;
    //! The size of the cube of the .
    double size;
    //! The origin coordinate of root node.
    Point3f origin;
    //! The size of the leaf node.
    double resolution;
    //! Whether the point cloud has a color attribute.
    bool hasColor{};
};

Octree::Octree() : p(new Impl)
{
    p->maxDepth = 0;
    p->size = 0;
    p->origin = Point3f(0,0,0);
}

Octree::Octree(int _maxDepth, double _size, const Point3f& _origin ) : p(new Impl)
{
    p->maxDepth = _maxDepth;
    p->size = _size;
    p->origin = _origin;
}

Octree::Octree(const std::vector<Point3f>& _pointCloud, double resolution) : p(new Impl)
{
    std::vector<Point3f> v;
    this->create(_pointCloud, v, resolution);
}

Octree::Octree(int _maxDepth) : p(new Impl)
{
    p->maxDepth = _maxDepth;
    p->size = 0;
    p->origin = Point3f(0,0,0);
}

Octree::~Octree() { }

bool Octree::insertPoint(const Point3f& point, const Point3f &color)
{
    return p->insertPoint(point, color);
}

bool Octree::Impl::insertPoint(const Point3f& point, const Point3f &color)
{
    size_t depthMask = (size_t)(1 << (this->maxDepth - 1));

    if(this->rootNode.empty())
    {
        this->rootNode = new OctreeNode( 0, this->size, this->origin,  color, -1);
    }

    bool pointInBoundFlag = this->rootNode->isPointInBound(point);
    if(this->rootNode->depth == 0 && !pointInBoundFlag)
    {
        return false;
    }

    OctreeKey key((size_t)floor((point.x - this->origin.x) / this->resolution),
                  (size_t)floor((point.y - this->origin.y) / this->resolution),
                  (size_t)floor((point.z - this->origin.z) / this->resolution));

    this->rootNode->insertPointRecurse(point, color, this->maxDepth, key, depthMask);
    return true;
}


static Vec6f getBoundingBox(const std::vector<Point3f>& pointCloud)
{
    const float mval = std::numeric_limits<float>::max();
    Vec6f bb(mval, mval, mval, -mval, -mval, -mval);

    for (const auto& pt : pointCloud)
    {
        bb[0] = min(bb[0], pt.x);
        bb[1] = min(bb[1], pt.y);
        bb[2] = min(bb[2], pt.z);
        bb[3] = max(bb[3], pt.x);
        bb[4] = max(bb[4], pt.y);
        bb[5] = max(bb[5], pt.z);
    }

    return bb;
}

void Octree::Impl::fill(bool useResolution, const std::vector<Point3f>& pointCloud, const std::vector<Point3f>& colorAttribute)
{
    Vec6f bbox = getBoundingBox(pointCloud);
    Point3f minBound(bbox[0], bbox[1], bbox[2]);
    Point3f maxBound(bbox[3], bbox[4], bbox[5]);

    double maxSize = max(max(maxBound.x - minBound.x, maxBound.y - minBound.y), maxBound.z - minBound.z);

    // Extend maxSize to the closest power of 2 that exceeds it for bit operations
    maxSize = double(1 << int(ceil(log2(maxSize))));

    // to calculate maxDepth from resolution or vice versa
    if (useResolution)
    {
        this->maxDepth = ceil(log2(maxSize / this->resolution));
    }
    else
    {
        this->resolution = (maxSize / (1 << (this->maxDepth + 1)));
    }

    this->size = (1 << this->maxDepth) * this->resolution;
    this->origin = Point3f(float(floor(minBound.x / this->resolution) * this->resolution),
                           float(floor(minBound.y / this->resolution) * this->resolution),
                           float(floor(minBound.z / this->resolution) * this->resolution));

    this->hasColor = !colorAttribute.empty();

    // Insert every point in PointCloud data.
    for (size_t idx = 0; idx < pointCloud.size(); idx++)
    {
        Point3f insertColor = this->hasColor ? colorAttribute[idx] : Point3f(0.0f, 0.0f, 0.0f);
        if (!this->insertPoint(pointCloud[idx], insertColor))
        {
            CV_Error(Error::StsBadArg, "The point is out of boundary!");
        }
    }
}


void Octree::create(const std::vector<Point3f> &pointCloud, double _resolution)
{
    CV_Assert(_resolution > 0);

    p->resolution = _resolution;

    CV_Assert(!pointCloud.empty());

    p->fill( /* useResolution */ true, pointCloud, { });
}

void Octree::create(const std::vector<Point3f> &pointCloud, int _maxDepth)
{
    CV_Assert(_maxDepth > 0);

    p->maxDepth = _maxDepth;

    CV_Assert(!pointCloud.empty());

    p->fill( /* useResolution */ false, pointCloud, { });
}

void Octree::create(const std::vector<Point3f> &pointCloud, const std::vector<Point3f> &colorAttribute, double _resolution)
{
    CV_Assert(_resolution > 0);

    p->resolution = _resolution;

    CV_Assert(!pointCloud.empty());
    CV_Assert(pointCloud.size() == colorAttribute.size() || colorAttribute.empty());

    p->fill( /* useResolution */ true, pointCloud, colorAttribute);
}

void Octree::create(const std::vector<Point3f> &pointCloud, const std::vector<Point3f> &colorAttribute, int _maxDepth)
{
    CV_Assert(_maxDepth > 0);

    p->maxDepth = _maxDepth;

    CV_Assert(!pointCloud.empty());
    CV_Assert(pointCloud.size() == colorAttribute.size() || colorAttribute.empty());

    p->fill( /* useResolution */ false, pointCloud, colorAttribute);
}

void Octree::setMaxDepth(int _maxDepth)
{
    CV_Assert(_maxDepth > 0);
    this->p->maxDepth = _maxDepth;
}

void Octree::setSize(double _size)
{
    CV_Assert(_size > 0);
    this->p->size = _size;
};

void Octree::setOrigin(const Point3f& _origin)
{
    this->p->origin = _origin;
}

void Octree::clear()
{
    if(!p->rootNode.empty())
    {
        p->rootNode.release();
    }

    p->size = 0;
    p->maxDepth = 0;
    p->origin = Point3f (0,0,0); // origin coordinate
}

bool Octree::empty() const
{
    return p->rootNode.empty();
}

Ptr<OctreeNode> index(const Point3f& point, Ptr<OctreeNode>& _node, OctreeKey &key, size_t depthMask)
{
    OctreeNode &node = *_node;

    if(node.empty())
    {
        return Ptr<OctreeNode>();
    }

    if(node.isLeaf)
    {
        for(size_t i = 0; i < node.pointList.size(); i++ )
        {
            //TODO: epsilon comparison
            if((point.x == node.pointList[i].x) &&
               (point.y == node.pointList[i].y) &&
               (point.z == node.pointList[i].z)
                    )
            {
                return _node;
            }
        }
        return Ptr<OctreeNode>();
    }


    size_t childIndex = key.findChildIdxByMask(depthMask);
    if(!node.children[childIndex].empty())
    {
        return index(point, node.children[childIndex],key,depthMask>>1);
    }
    return Ptr<OctreeNode>();
}

bool Octree::isPointInBound(const Point3f& _point) const
{
    return p->rootNode->isPointInBound(_point);
}

bool Octree::deletePoint(const Point3f& point)
{
    OctreeKey key=OctreeKey((size_t)floor((point.x - this->p->origin.x) / p->resolution),
                            (size_t)floor((point.y - this->p->origin.y) / p->resolution),
                            (size_t)floor((point.z - this->p->origin.z) / p->resolution));
    size_t depthMask=(size_t)1 << (p->maxDepth - 1);
    Ptr<OctreeNode> node = index(point, p->rootNode,key,depthMask);

    if(node.empty())
        return false;

    // we've found a leaf node and delete all verts equal to given one
    size_t ctr = 0;
    while (!node->pointList.empty() && ctr < node->pointList.size())
    {
        // TODO: epsilon comparison
        if ((point.x == node->pointList[ctr].x) &&
            (point.y == node->pointList[ctr].y) &&
            (point.z == node->pointList[ctr].z))
        {
            node->pointList.erase(node->pointList.begin() + ctr);
        }
        else
        {
            ctr++;
        }
    }

    if (node->pointList.empty())
    {
        // empty node and its empty parents should be removed
        OctreeNode *parentPtr = node->parent;
        int parentdIdx = node->parentIndex;

        while (parentPtr)
        {
            parentPtr->children[parentdIdx].release();

            // check if all children were deleted
            bool deleteFlag = true;
            for (size_t i = 0; i < 8; i++)
            {
                if (!parentPtr->children[i].empty())
                {
                    deleteFlag = false;
                    break;
                }
            }

            if (deleteFlag)
            {
                // we're at empty node, going up
                parentdIdx = parentPtr->parentIndex;
                parentPtr = parentPtr->parent;
            }
            else
            {
                // reached first non-empty node, stopping
                parentPtr = nullptr;
            }
        }
    }
    return true;
}


void OctreeNode::insertPointRecurse( const Point3f& point,const Point3f &colorVertex, int maxDepth,
                                     const OctreeKey &key, size_t depthMask)
{
    //add point to the leaf node.
    if (this->depth == maxDepth)
    {
        this->isLeaf = true;
        this->color = colorVertex;
        this->pointNum++;
        this->pointList.push_back(point);
        return;
    }

    double childSize = this->size * 0.5;
    //calculate the index and the origin of child.
    size_t childIndex = key.findChildIdxByMask(depthMask);
    size_t xIndex = (childIndex & 1) ? 1 : 0;
    size_t yIndex = (childIndex & 2) ? 1 : 0;
    size_t zIndex = (childIndex & 4) ? 1 : 0;
    Point3f childOrigin = this->origin + Point3f(float(xIndex), float(yIndex), float(zIndex)) * float(childSize);

    if (this->children[childIndex].empty())
    {
        this->children[childIndex] = new OctreeNode(this->depth + 1, childSize, childOrigin, Point3f(0, 0, 0),
                                                    int(childIndex));
        this->children[childIndex]->parent = this;
    }

    this->children[childIndex]->insertPointRecurse( point, colorVertex, maxDepth, key, depthMask >> 1);
    this->pointNum += 1;
}

void Octree::getPointCloudByOctree(std::vector<Point3f> &restorePointCloud, std::vector<Point3f> &restoreColor)
{
    Ptr<OctreeNode> root = p->rootNode;
    double resolution = p->resolution;
    getPointRecurse(restorePointCloud, restoreColor, 0, 0, 0, root, resolution, p->origin, p->hasColor);
}

void getPointRecurse(std::vector<Point3f> &restorePointCloud, std::vector<Point3f> &restoreColor, size_t x_key,
                     size_t y_key,size_t z_key, Ptr<OctreeNode> &_node, double resolution, Point3f ori,
                     bool hasColor) {
    OctreeNode node = *_node;
    if (node.isLeaf) {
        restorePointCloud.emplace_back(
                (float) (resolution * x_key) + (float) (resolution * 0.5) + ori.x,
                (float) (resolution * y_key) + (float) (resolution * 0.5) + ori.y,
                (float) (resolution * z_key) + (float) (resolution * 0.5) + ori.z);
        if (hasColor) {
            restoreColor.emplace_back(node.color);
        }
        return;
    }
    unsigned char x_mask = 1;
    unsigned char y_mask = 2;
    unsigned char z_mask = 4;
    for (unsigned char i = 0; i < 8; i++) {
        size_t x_copy = x_key;
        size_t y_copy = y_key;
        size_t z_copy = z_key;
        if (!node.children[i].empty()) {
            size_t x_offSet = !!(x_mask & i);
            size_t y_offSet = !!(y_mask & i);
            size_t z_offSet = !!(z_mask & i);
            x_copy = (x_copy << 1) | x_offSet;
            y_copy = (y_copy << 1) | y_offSet;
            z_copy = (z_copy << 1) | z_offSet;
            getPointRecurse(restorePointCloud, restoreColor, x_copy, y_copy, z_copy, node.children[i], resolution,
                            ori, hasColor);
        }
    }
};

static float SquaredDistance(const Point3f& query, const Point3f& origin)
{
    Point3f diff = query - origin;
    return diff.dot(diff);
}

bool OctreeNode::overlap(const Point3f& query, float squareRadius) const
{
    float halfSize = float(this->size * 0.5);
    Point3f center = this->origin + Point3f( halfSize, halfSize, halfSize );

    float dist = SquaredDistance(center, query);
    float temp = float(this->size) * float(this->size) * 3.0f;

    return ( dist + dist * std::numeric_limits<float>::epsilon() ) <= float(temp * 0.25f + squareRadius + sqrt(temp * squareRadius)) ;
}

void OctreeNode::radiusNNSearchRecurse(const Point3f& query, float squareRadius,
                                       std::vector<PQueueElem<Point3f> >& candidatePoint) const
{
    float dist;
    Ptr<OctreeNode> child;

    // iterate eight children
    for(size_t i = 0; i < 8; i++)
    {
        if( !this->children[i].empty() && this->children[i]->overlap(query, squareRadius))
        {
            if(!this->children[i]->isLeaf)
            {
                // Reach the branch node
                this->children[i]->radiusNNSearchRecurse(query, squareRadius, candidatePoint);
            }
            else
            {
                // Reach the leaf node
                child = this->children[i];

                for(size_t j = 0; j < child->pointList.size(); j++)
                {
                    Point3f pt = child->pointList[j];
                    dist = SquaredDistance(pt, query);
                    if(dist + dist * std::numeric_limits<float>::epsilon() <= squareRadius )
                    {
                        candidatePoint.emplace_back(dist, pt);
                    }
                }
            }
        }
    }
}

int Octree::radiusNNSearch(const Point3f& query, float radius,
                           std::vector<Point3f>& pointSet, std::vector<float>& squareDistSet) const
{
    pointSet.clear();
    squareDistSet.clear();

    if(p->rootNode.empty())
        return 0;

    float squareRadius = radius * radius;

    PQueueElem<Point3f> elem;
    std::vector<PQueueElem<Point3f> > candidatePoint;

    p->rootNode->radiusNNSearchRecurse(query, squareRadius, candidatePoint);

    for(size_t i = 0; i < candidatePoint.size(); i++)
    {
        pointSet.push_back(candidatePoint[i].t);
        squareDistSet.push_back(candidatePoint[i].dist);
    }
    return int(pointSet.size());
}

void OctreeNode::KNNSearchRecurse(const Point3f& query, const int K,
                                  float& smallestDist, std::vector<PQueueElem<Point3f> >& candidatePoint) const
{
    std::vector<PQueueElem<int> > priorityQue;

    Ptr<OctreeNode> child;
    float dist = 0;
    Point3f center; // the OctreeNode Center

    // Add the non-empty OctreeNode to priorityQue
    for(size_t i = 0; i < 8; i++)
    {
        if(!this->children[i].empty())
        {
            float halfSize = float(this->children[i]->size * 0.5);

            center = this->children[i]->origin + Point3f(halfSize, halfSize, halfSize);

            dist = SquaredDistance(query, center);
            priorityQue.emplace_back(dist, int(i));
        }
    }

    std::sort(priorityQue.rbegin(), priorityQue.rend());
    child = this->children[priorityQue.back().t];

    while (!priorityQue.empty() && child->overlap(query, smallestDist))
    {
        if (!child->isLeaf)
        {
            child->KNNSearchRecurse(query, K, smallestDist, candidatePoint);
        }
        else
        {
            for (size_t i = 0; i < child->pointList.size(); i++)
            {
                dist = SquaredDistance(child->pointList[i], query);

                if ( dist + dist * std::numeric_limits<float>::epsilon() <= smallestDist )
                {
                    candidatePoint.emplace_back(dist, child->pointList[i]);
                }
            }

            std::sort(candidatePoint.begin(), candidatePoint.end());

            if (int(candidatePoint.size()) > K)
            {
                candidatePoint.resize(K);
            }

            if (int(candidatePoint.size()) == K)
            {
                smallestDist = candidatePoint.back().dist;
            }
        }

        priorityQue.pop_back();

        // To next child
        if(!priorityQue.empty())
            child = this->children[priorityQue.back().t];
    }
}

void Octree::KNNSearch(const Point3f& query, const int K, std::vector<Point3f>& pointSet, std::vector<float>& squareDistSet) const
{
    pointSet.clear();
    squareDistSet.clear();

    if(p->rootNode.empty())
        return;

    PQueueElem<Ptr<Point3f> > elem;
    std::vector<PQueueElem<Point3f> > candidatePoint;
    float smallestDist = std::numeric_limits<float>::max();

    p->rootNode->KNNSearchRecurse(query, K, smallestDist, candidatePoint);

    for(size_t i = 0; i < candidatePoint.size(); i++)
    {
        pointSet.push_back(candidatePoint[i].t);
        squareDistSet.push_back(candidatePoint[i].dist);
    }
}

}