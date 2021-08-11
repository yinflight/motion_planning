#include "JPS_searcher.h"

using namespace std;
using namespace Eigen;

inline void JPSPathFinder::JPSGetSucc(GridNodePtr currentPtr,
                                      vector<GridNodePtr> &neighborPtrSets,
                                      vector<double> &edgeCostSets) {
    neighborPtrSets.clear();
    edgeCostSets.clear();
    const int norm1 = abs(currentPtr->dir(0))
                      + abs(currentPtr->dir(1))
                      + abs(currentPtr->dir(2));

    int num_neib = jn3d->nsz[norm1][0];//需要添加的邻居个数
    int num_fneib = jn3d->nsz[norm1][1];//可能存在的Forced neighbor数量
    //以当前栅格为中心的3x3x3的立方体栅格的索引，以中心点为起始坐标
    //不同的id对应一个不同的运动方向，不同的运动方向对应的自然节点和Forced neighbor也不一样
    int id = (currentPtr->dir(0) + 1) +
             3 * (currentPtr->dir(1) + 1) +
             9 * (currentPtr->dir(2) + 1);

    for (int dev = 0; dev < num_neib + num_fneib; ++dev) {
        Vector3i neighborIdx;
        Vector3i expandDir;

        //当前运动方向之下，把对应的自然节点给添加
        if (dev < num_neib) {
            expandDir(0) = jn3d->ns[id][0][dev];
            expandDir(1) = jn3d->ns[id][1][dev];
            expandDir(2) = jn3d->ns[id][2][dev];

            //当前节点是否是一个跳跃节点
            if (!jump(currentPtr->index, expandDir, neighborIdx))
                continue;
        } else {
            int nx = currentPtr->index(0) + jn3d->f1[id][0][dev - num_neib];
            int ny = currentPtr->index(1) + jn3d->f1[id][1][dev - num_neib];
            int nz = currentPtr->index(2) + jn3d->f1[id][2][dev - num_neib];

            if (isOccupied(nx, ny, nz)) {
                expandDir(0) = jn3d->f2[id][0][dev - num_neib];
                expandDir(1) = jn3d->f2[id][1][dev - num_neib];
                expandDir(2) = jn3d->f2[id][2][dev - num_neib];

                if (!jump(currentPtr->index, expandDir, neighborIdx))
                    continue;
            } else
                continue;
        }

        //只要是Jump Point都需要添加到Open List中
        GridNodePtr nodePtr = GridNodeMap[neighborIdx(0)][neighborIdx(1)][neighborIdx(2)];
        nodePtr->dir = expandDir;

        neighborPtrSets.push_back(nodePtr);
        edgeCostSets.push_back(
                sqrt((neighborIdx(0) - currentPtr->index(0)) * (neighborIdx(0) - currentPtr->index(0)) +
                     (neighborIdx(1) - currentPtr->index(1)) * (neighborIdx(1) - currentPtr->index(1)) +
                     (neighborIdx(2) - currentPtr->index(2)) * (neighborIdx(2) - currentPtr->index(2)))
        );
    }
}

/*!
 * 递归的调用jump()函数执行跳跃操作
 * 某一次跳跃满足以下条件则表示跳跃有有效的：
 *     (1)找到了目标节点
 *     (2)有Forced neighbor节点
 * 否则是此次跳跃无效：
 *     (1)碰到障碍物或者跳出边界
 * @param curIdx
 * @param expDir
 * @param neiIdx
 * @return  返回true表示当前节点是一个跳跃节点，需要加入Open List
 */
bool JPSPathFinder::jump(const Vector3i &curIdx, const Vector3i &expDir, Vector3i &neiIdx) {
    //按照当前扩展方向，计算下一个点的索引
    neiIdx = curIdx + expDir;

    //是否有障碍物
    if (!isFree(neiIdx))
        return false;

    //是否是目标节点
    if (neiIdx == goalIdx)
        return true;

    //是否有Forced neighbor
    if (hasForced(neiIdx, expDir))
        return true;

    const int id = (expDir(0) + 1) + 3 * (expDir(1) + 1) + 9 * (expDir(2) + 1);
    const int norm1 = abs(expDir(0)) + abs(expDir(1)) + abs(expDir(2));
    int num_neib = jn3d->nsz[norm1][0];

    for (int k = 0; k < num_neib - 1; ++k) {
        Vector3i newNeiIdx;
        Vector3i newDir(jn3d->ns[id][0][k], jn3d->ns[id][1][k], jn3d->ns[id][2][k]);
        if (jump(neiIdx, newDir, newNeiIdx))
            return true;
    }

    return jump(neiIdx, expDir, neiIdx);
}

/*!
 * 计算当前节点是否包含Forced neighbor
 * @param idx 当前点的邻居节点索引
 * @param dir 当前扩展方向
 * @return 当返回值是true就表示当前含有Forced neighbor， 否则没有
 */
inline bool JPSPathFinder::hasForced(const Vector3i &idx, const Vector3i &dir) {
    int norm1 = abs(dir(0)) + abs(dir(1)) + abs(dir(2));
    int id = (dir(0) + 1) + 3 * (dir(1) + 1) + 9 * (dir(2) + 1);

    switch (norm1) {
        case 1:
            // 1-d move, check 8 neighbors
            //根据规则只移动一步的话，只有可能是x,y,z其中一个方向增加一步,
            //那么此时根据规则字需要检查垂直与当前运动直线的平面上的八个点,
            //只要有一个点被障碍物占据就表示有Forced neighbor
            for (int fn = 0; fn < 8; ++fn) {
                int nx = idx(0) + jn3d->f1[id][0][fn];
                int ny = idx(1) + jn3d->f1[id][1][fn];
                int nz = idx(2) + jn3d->f1[id][2][fn];
                if (isOccupied(nx, ny, nz))
                    return true;
            }
            return false;

        case 2:
            // 2-d move, check 8 neighbors
            //类比与case 1
            for (int fn = 0; fn < 8; ++fn) {
                int nx = idx(0) + jn3d->f1[id][0][fn];
                int ny = idx(1) + jn3d->f1[id][1][fn];
                int nz = idx(2) + jn3d->f1[id][2][fn];
                if (isOccupied(nx, ny, nz))
                    return true;
            }
            return false;

        case 3:
            // 3-d move, check 6 neighbors
            for (int fn = 0; fn < 6; ++fn) {
                int nx = idx(0) + jn3d->f1[id][0][fn];
                int ny = idx(1) + jn3d->f1[id][1][fn];
                int nz = idx(2) + jn3d->f1[id][2][fn];
                if (isOccupied(nx, ny, nz))
                    return true;
            }
            return false;

        default:
            return false;
    }
}

inline bool JPSPathFinder::isOccupied(const Eigen::Vector3i &index) const {
    return isOccupied(index(0), index(1), index(2));
}

inline bool JPSPathFinder::isFree(const Eigen::Vector3i &index) const {
    return isFree(index(0), index(1), index(2));
}

inline bool JPSPathFinder::isOccupied(const int &idx_x, const int &idx_y, const int &idx_z) const {
    return (idx_x >= 0 && idx_x < GLX_SIZE && idx_y >= 0 && idx_y < GLY_SIZE && idx_z >= 0 && idx_z < GLZ_SIZE &&
            (data[idx_x * GLYZ_SIZE + idx_y * GLZ_SIZE + idx_z] == 1));
}

inline bool JPSPathFinder::isFree(const int &idx_x, const int &idx_y, const int &idx_z) const {
    return (idx_x >= 0 && idx_x < GLX_SIZE && idx_y >= 0 && idx_y < GLY_SIZE && idx_z >= 0 && idx_z < GLZ_SIZE &&
            (data[idx_x * GLYZ_SIZE + idx_y * GLZ_SIZE + idx_z] < 1));
}

void JPSPathFinder::JPSGraphSearch(Eigen::Vector3d start_pt, Eigen::Vector3d end_pt) {
    ros::Time time_1 = ros::Time::now();

    //index of start_point and end_point
    Vector3i start_idx = coord2gridIndex(start_pt);
    Vector3i end_idx = coord2gridIndex(end_pt);
    goalIdx = end_idx;

    //position of start_point and end_point
    start_pt = gridIndex2coord(start_idx);
    end_pt = gridIndex2coord(end_idx);

    //Initialize the pointers of struct GridNode which represent start node and goal node
    GridNodePtr startPtr = new GridNode(start_idx, start_pt);
    GridNodePtr endPtr = new GridNode(end_idx, end_pt);

    //openSet is the open_list implemented through multimap in STL library
    openSet.clear();
    // currentPtr represents the node with lowest f(n) in the open_list
    GridNodePtr currentPtr = NULL;
    GridNodePtr neighborPtr = NULL;

    //put start node in open set
    startPtr->gScore = 0;
    startPtr->fScore = getHeu(startPtr, endPtr);
    //STEP 1: finish the AstarPathFinder::getHeu , which is the heuristic function
    startPtr->id = 1;
    startPtr->coord = start_pt;
    openSet.insert(make_pair(startPtr->fScore, startPtr));
    /*
    *
    STEP 2 :  some else preparatory works which should be done before while loop
    please write your code below
    *
    *
    */
    double tentative_gScore;
    vector<GridNodePtr> neighborPtrSets;
    vector<double> edgeCostSets;

    // this is the main loop
    while (!openSet.empty()) {
        /*
        *
        *
        step 3: Remove the node with lowest cost function from open set to closed set
        please write your code below
        
        IMPORTANT NOTE!!!
        This part you should use the C++ STL: multimap, more details can be find in Homework description
        *
        *
        */
        currentPtr = openSet.begin()->second;
        currentPtr->id = 1;
        openSet.erase(openSet.begin());

        // if the current node is the goal 
        if (currentPtr->index == goalIdx) {
            ros::Time time_2 = ros::Time::now();
            terminatePtr = currentPtr;
            ROS_WARN("[JPS]{sucess} Time in JPS is %f ms, path cost if %f m", (time_2 - time_1).toSec() * 1000.0,
                     currentPtr->gScore * resolution);
            return;
        }
        //get the succetion
        JPSGetSucc(currentPtr, neighborPtrSets, edgeCostSets); //we have done it for you

        /*
        *
        *
        STEP 4:  For all unexpanded neigbors "m" of node "n", please finish this for loop
        please write your code below
        *        
        */
        for (int i = 0; i < (int) neighborPtrSets.size(); i++) {
            /*
            *
            *
            Judge if the neigbors have been expanded
            please write your code below
            
            IMPORTANT NOTE!!!
            neighborPtrSets[i]->id = -1 : unexpanded
            neighborPtrSets[i]->id = 1 : expanded, equal to this node is in close set
            *        
            */
            if (neighborPtr->id == 0) { //discover a new node
                /*
                *
                *
                STEP 6:  As for a new node, do what you need do ,and then put neighbor in open set and record it
                please write your code below
                *        
                */
                neighborPtr->gScore = currentPtr->gScore + edgeCostSets.at(i);
                neighborPtr->fScore = getHeu(neighborPtr, endPtr);
                neighborPtr->cameFrom = currentPtr;
                openSet.insert(std::make_pair(neighborPtr->fScore, neighborPtr));
                neighborPtr->id = 1;
                continue;
            } else if (neighborPtr->id == 1) { //in open set and need update
                /*
                *
                *
                STEP 7:  As for a node in open set, update it , maintain the openset ,and then put neighbor in open set and record it
                please write your code below
                *        
                */
                if (neighborPtr->gScore > currentPtr->gScore + edgeCostSets.at(i)){
                    neighborPtr->gScore = currentPtr->gScore + edgeCostSets.at(i);
                    neighborPtr->fScore = neighborPtr->gScore + getHeu(neighborPtr, endPtr);
                    neighborPtr->cameFrom = currentPtr;

                    // if change its parents, update the expanding direction
                    //THIS PART IS ABOUT JPS, you can ignore it when you do your Astar work
                    for (int i = 0; i < 3; i++) {
                        neighborPtr->dir(i) = neighborPtr->index(i) - currentPtr->index(i);
                        if (neighborPtr->dir(i) != 0)
                            neighborPtr->dir(i) /= abs(neighborPtr->dir(i));
                    }
                }
                continue;
            } else {
                continue;
            }
        }
    }
    //if search fails
    ros::Time time_2 = ros::Time::now();
    if ((time_2 - time_1).toSec() > 0.1)
        ROS_WARN("Time consume in JPS path finding is %f", (time_2 - time_1).toSec());
}