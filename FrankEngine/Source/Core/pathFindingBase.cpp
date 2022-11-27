////////////////////////////////////////////////////////////////////////////////////////
/*
	Base Class for Path Finding
	Copyright 2018 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../core/pathFindingBase.h"

ConsoleCommandSimple(bool, pathFindingTest, false);

bool PathFindingBase::pathFindingEnable = true;
ConsoleCommand(PathFindingBase::pathFindingEnable, pathFindingEnable);

bool PathFindingBase::pathFindingTryToShorten = true;
ConsoleCommand(PathFindingBase::pathFindingTryToShorten, pathFindingTryToShorten);

bool PathFindingBase::pathFindingDebug = 0;
ConsoleCommand(PathFindingBase::pathFindingDebug, pathFindingDebug);

float PathFindingBase::pathFindingDebugTime = 5;
ConsoleCommand(PathFindingBase::pathFindingDebugTime, pathFindingDebugTime);

// huristic controls how much to search directly towards the destination versus searching in all directions
float PathFindingBase::heuristicWeight = 1;
ConsoleCommand(PathFindingBase::heuristicWeight, pathFindingHeuristicWeight);

// limit how much pathfinding can be done each frame
int PathFindingBase::maxFrameCost = 250;
ConsoleCommand(PathFindingBase::maxFrameCost, pathFindingMaxFrameCost);

// limit how much the pathfinding algorithm can loop before it gives up
int PathFindingBase::maxLoop = 500;
ConsoleCommand(PathFindingBase::maxLoop, pathFindingMaxLoop);

PathFindingBase::~PathFindingBase()
{
	SAFE_DELETE_ARRAY(nodes);
}

inline PathFindingBase::Node* PathFindingBase::GetNode(const IntVector2& nodePos) const
{
	if (nodePos.x < 0 || nodePos.y < 0 || nodePos.x >= arrayWidth || nodePos.y >= arrayWidth)
		return NULL;

	return &GetNodeFast(nodePos);
}

inline PathFindingBase::Node& PathFindingBase::GetNodeFast(const IntVector2& nodePos) const
{
	// same as GetNode except it asserts if node is out of bounds instead of returning null
	ASSERT(!(nodePos.x < 0 || nodePos.y < 0 || nodePos.x >= arrayWidth || nodePos.y >= arrayWidth));
	return nodes[nodePos.x + arrayWidth * nodePos.y];
}

inline IntVector2 PathFindingBase::GetNodePos(const Vector2& worldPos) const
{
	IntVector2 tileIndex = g_terrain->GetTileIndex(worldPos);
	IntVector2 nodeIndex = tileIndex - GetWindowOffset();
	return nodeIndex;
}

inline PathFindingBase::Node* PathFindingBase::GetNode(const Vector2& worldPos) const
{
	return GetNode(GetNodePos(worldPos));
}

inline IntVector2 PathFindingBase::GetWindowOffset() const
{
	const Vector2& pos = g_gameControlBase->GetUserPosition();
	IntVector2 centerPatch = g_terrain->GetPatchIndex(pos);
	return Terrain::patchSize*(centerPatch - IntVector2(windowSize));
}

inline void PathFindingBase::Node::Render(const Color& color, float time) const
{
	if (g_cameraBase->CameraTest(posWorld, TerrainTile::GetRadius()))
		posWorld.RenderDebug(color, TerrainTile::GetSize()/2, time);
}

inline bool PathFindingBase::IsNodeClear(const IntVector2& pos) const
{
	//GetNodeFast(pos)->Render();
	return GetNodeFast(pos).IsClear();
}

inline float PathFindingBase::GetShortestPossibleDistance(Node* startNode, Node* endNode) const
{
	float distance = 0;
	Vector2 deltaPos = endNode->pos - startNode->pos;
	deltaPos = deltaPos.Abs();
	float minSide = Min(deltaPos.x, deltaPos.y);
	distance += sqrt(2*Square(minSide));
	distance += Max(deltaPos.x, deltaPos.y) - minSide;
	return distance;
}

bool PathFindingBase::IsTileWalkable(const TerrainTile& tile, bool canFly) const
{ 
	if (tile.IsAreaClear())
		return true;
	
	const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(tile.GetSurfaceData(0));
	const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(tile.GetSurfaceData(1));
	return (!tile0Info.HasCollision() || !tile.GetSurfaceHasArea(0)) && (!tile1Info.HasCollision() || !tile.GetSurfaceHasArea(1));
}

float PathFindingBase::GetObjectCost(const GameObject& object, bool canFly) const
{
	return object.IsPathable(canFly) ? 0.0f : Node::maxCost;
}

void PathFindingBase::ClearNodeData()
{
	const IntVector2 offset = GetWindowOffset();
	for (int y = 0; y < arrayWidth; ++y)
	for (int x = 0; x < arrayWidth; ++x)
	{
		IntVector2 intPos(x, y);
		Node& node = GetNodeFast(intPos);
		node = Node(intPos);
		IntVector2 nodePos = intPos + offset;
		node.posWorld = g_terrain->GetTilePos(nodePos.x, nodePos.y) + Vector2(0.5f*TerrainTile::GetSize());
	}
}

void PathFindingBase::ResetNodeData()
{
	for (int y = 0; y < arrayWidth; ++y)
	for (int x = 0; x < arrayWidth; ++x)
	{
		IntVector2 intPos(x, y);
		Node& node = GetNodeFast(intPos);
		node.Reset();
	}
}

void PathFindingBase::Update()
{
	if (!nodes || windowSize != Terrain::windowSize)
	{
		// build the node array for tiles in stream window
		SAFE_DELETE_ARRAY(nodes);
		windowSize = Terrain::windowSize;
		arrayWidth = Terrain::patchSize*(2*windowSize+1);
		nodes = new Node[arrayWidth*arrayWidth];
	}

	if (pathFindingTest)
	{
		if (!g_gameControlBase->IsEditMode())
		{
			/* // line test
			BuildNodeData(false);
			Node* startNode = GetNearestClearNode(g_gameControlBase->GetUserPosition());
			Node* endNode = GetNearestClearNode(g_input->GetMousePosWorldSpace());
			if (startNode && endNode)
			{
				IntVector2 startPos = startNode->pos;
				IntVector2 endPos = endNode->pos;
				bool c = CheckLine(startPos, endPos);
				Line2(startNode->posWorld, endNode->posWorld).RenderDebug(c ? Color::Green() : Color::Red());
			}*/
			
			const float oldPathFindingDebugTime = pathFindingDebugTime;
			pathFindingDebugTime = 0;
			list<Vector2> path;
			GetPath(g_gameControlBase->GetUserPosition(), g_input->GetMousePosWorldSpace(), path);
			pathFindingDebugTime = oldPathFindingDebugTime;

			if (!pathFindingDebug && !path.empty())
			{
				Vector2 lastPos = path.front();
				for (const Vector2& pos : path)
				{
					Line2(pos, lastPos).RenderDebug(Color::Red());
					lastPos = pos;
				}
				path.front().RenderDebug(Color::Green(0.5f), TerrainTile::GetSize() / 2);
				lastPos.RenderDebug(Color::Green(0.5f), TerrainTile::GetSize() / 2);
			}
		}

	}
	if (pathFindingDebug)
	{
		WCHAR text[256];
		swprintf_s(text, 256, L"Pathfinding Cost: %d", pathFindCostThisFrame);
		g_debugRender.RenderText(Vector2(g_backBufferWidth / 2.0f, 50), text, Color::White(), true, 0, true);
	}

	pathFindCostLastFrame = pathFindCostThisFrame;
	pathFindCostThisFrame = 0;
}

PathFindingBase::Node* PathFindingBase::GetNearestClearNode(const Vector2& worldPos, int searchRange) const
{
	// search for nearst walkable node
	int offset = 0;
	const IntVector2 centerNodePos = GetNodePos(worldPos);
	while (1)
	{
		float nearestDistanceSquared = 0;
		Node* nearestNode = NULL;
		for (int i = -offset; i <= offset; ++i)
		for (int j = -offset; j <= offset; ++j)
		{
			const IntVector2 nodePos = centerNodePos + IntVector2(i, j);
			Node* node = GetNode(nodePos);
			if (node && node->IsClear())
			{
				float distanceSquared = (node->posWorld - worldPos).LengthSquared();
				if (!nearestNode || distanceSquared < nearestDistanceSquared)
				{
					nearestNode = node;
					nearestDistanceSquared = distanceSquared;
				}
			}
		}
		if (nearestNode)
			return nearestNode;
		++offset;
		if (offset > searchRange) // limit search range
			return NULL;
	}

	return NULL;
}

bool PathFindingBase::GetPath(const Vector2& startPos, const Vector2& endPos, list<Vector2>& path, bool canFly)
{
	FrankProfilerEntryDefine(L"PathFinding::GetPath()", Color::Cyan(), 1000);

	if (!pathFindingEnable || !IsOkToPathFind())
		return false;

	path.clear();
	BuildNodeData(canFly);

	Node* startNode = GetNearestClearNode(startPos);
	Node* endNode = GetNearestClearNode(endPos);
	if (!startNode || !startNode->isWalkable || !endNode || !endNode->isWalkable || endNode->cost > 0)
		return false;

	if (startNode == endNode)
	{
		path.push_front(startNode->posWorld);
		return true;
	}	
	if (!AStarSearch(startNode, endNode))
		return false;

	NodeList nodePath;
	while (endNode)
	{
		nodePath.push_front(endNode);
		endNode = endNode->parent;
	}
	
	ShortenPath(nodePath);
	ShortenPath2(nodePath);

	for (NodeList::iterator it = nodePath.begin(); it != nodePath.end(); ++it)
		path.push_back((*it)->posWorld);

	if (pathFindingDebug && !path.empty())
	{
		Vector2 lastPos = path.front();
		for (const Vector2& pos : path)
		{
			Circle(pos, TerrainTile::GetSize() / 2).RenderDebug(Color::Red(0.3f), pathFindingDebugTime);
			Line2(pos, lastPos).RenderDebug(Color::Red(), pathFindingDebugTime);
			lastPos = pos;
		}
		path.front().RenderDebug(Color::Green(0.5f), TerrainTile::GetSize() / 2, pathFindingDebugTime);
		lastPos.RenderDebug(Color::Green(0.5f), TerrainTile::GetSize() / 2, pathFindingDebugTime);
	}

	return true;
}

void PathFindingBase::BuildNodeData(bool canFly)
{
	ClearNodeData();

	// init the nodes using terrain data to tell if an area is walkable
	IntVector2 offset = GetWindowOffset();
	for (int y = 0; y < arrayWidth; ++y)
	for (int x = 0; x < arrayWidth; ++x)
	{
		const IntVector2 nodeTilePos = IntVector2(x, y) + offset;
		TerrainTile* tile = g_terrain->GetTile(nodeTilePos.x, nodeTilePos.y, 0);
		if (tile && IsTileWalkable(*tile, canFly))
		{
			Node& node = GetNodeFast(IntVector2(x, y));
			node.isWalkable = true;
			node.cost = 0;
		}
	}
	
	// check for world object blockers to add extra cost
	const float tileSize = TerrainTile::GetSize();
	const float halfTileSize = tileSize/2;
	//const Vector2 blockerShrink = Vector2(0.05f);
	const GameObjectHashTable& objects = g_objectManager.GetObjects();
	for (const GameObjectHashPair& hashPair : objects)
	{
		GameObject& object = *hashPair.second;

		if (object.IsDestroyed())
			continue;

		const float cost = GetObjectCost(object, canFly);
		if (cost == 0 || !object.HasPhysics())
			continue;

		Box2AABB aabb = object.GetPhysicsAABB();
		aabb.lowerBound += Vector2(0.05f);
		aabb.lowerBound.x = floor(aabb.lowerBound.x);
		aabb.lowerBound.y = floor(aabb.lowerBound.y);
		
		aabb.upperBound -= Vector2(0.05f);
		aabb.upperBound.x = ceil(aabb.upperBound.x);
		aabb.upperBound.y = ceil(aabb.upperBound.y);
		if (aabb.lowerBound.x >= aabb.upperBound.x || aabb.lowerBound.y >= aabb.upperBound.y)
		{
			Node* node = GetNode(aabb.GetCenter());
			if (!node)
				continue;
			node->cost += cost;
			if (cost >= Node::maxCost)
				node->isWalkable = false;
			if (pathFindingDebug)
				node->Render(Color::Red(0.1f), pathFindingDebugTime);
			continue;
		}

		// fill in blocked nodes
		Vector2 nodePos;
		for (nodePos.x = aabb.lowerBound.x; nodePos.x < aabb.upperBound.x; nodePos.x += tileSize)
		for (nodePos.y = aabb.lowerBound.y; nodePos.y < aabb.upperBound.y; nodePos.y += tileSize)
		{
			Node* node = GetNode(nodePos);
			if (!node)
				continue;

			node->cost += cost;
			if (cost >= Node::maxCost)
				node->isWalkable = false;
			if (pathFindingDebug)
				node->Render(Color::Red(0.1f), pathFindingDebugTime);
		}
	}
}

bool PathFindingBase::AStarSearch(Node* startNode, Node* endNode)
{
	ASSERT(startNode && endNode);
	ASSERT(startNode != endNode);
	ASSERT(startNode->isWalkable && endNode->isWalkable);

	NodeList openList;
	openList.push_back(startNode);
	startNode->isOpen = true;

	int loopCount = 0;
	while (!openList.empty())
	{
		// find node with least f
		float bestF = FLT_MAX;
		Node* currentNode = NULL;
		for (Node* node : openList) 
		{
			if (node->f < bestF)
			{
				bestF = node->f;
				currentNode = node;
			}
		}

		// stop when we reach goal or loop too much
		++pathFindCostThisFrame;
		if (currentNode == endNode || ++loopCount > maxLoop)
			break;

		// remove from open list
		currentNode->isOpen = false;
		openList.remove(currentNode);
		currentNode->isClosed = true;

		if (pathFindingDebug)
		{
			currentNode->Render(Color::White(0.05f), pathFindingDebugTime);
			//g_debugRender.RenderTextFormatted(currentNode->posWorld, Color::White(), true, pathFindingDebugTime, L"%d", loopCount);
		}

		// for each neighbor node
		IntVector2 offset;
		for (offset.x = -1; offset.x <= 1; ++offset.x)
		for (offset.y = -1; offset.y <= 1; ++offset.y)
		{
			Node* neighborNode = GetNode(offset + currentNode->pos);
			if (!neighborNode || !neighborNode->isWalkable || neighborNode->isClosed)
				continue;

			float distanceCurrent = 1;
			if (abs(offset.x) == abs(offset.y))
			{
				// diagonal - skip if there is a wall on either side
				Node& neighborNode1 = GetNodeFast(IntVector2(offset.x, 0) + currentNode->pos);
				if (neighborNode1.cost > 0)
					continue;
				Node& neighborNode2 = GetNodeFast(IntVector2(0, offset.y) + currentNode->pos);
				if (neighborNode2.cost > 0)
					continue;
				distanceCurrent = ROOT_2;
			}

			// calculate new values
			//const float distanceCurrent = (currentNode->pos - neighborNode->pos).Length();
			const float tentativeG = currentNode->g + distanceCurrent + neighborNode->cost;

			// skip if better already exists
			if (!neighborNode->isOpen)
			{
				neighborNode->isOpen = true;
				openList.push_back(neighborNode);
			}
			else if (tentativeG >= neighborNode->g)
				continue;
			
			// update with new best path
			neighborNode->parent = currentNode;
			neighborNode->g = tentativeG;
			const int distanceGoalSquared = (endNode->pos - neighborNode->pos).LengthSquared();
			neighborNode->f = neighborNode->g + float(distanceGoalSquared) * heuristicWeight;
			
			//if (pathFindingDebug)
			//	neighborNode->Render(Color::BuildHSV(float(++neighborNode->touchCount)*0.1f, 1, 1, 0.5f), pathFindingDebugTime);
		}
	}

	return (endNode->parent != NULL);
}

void PathFindingBase::ShortenPath(NodeList& nodePath)
{
	if (!pathFindingTryToShorten || nodePath.size() <= 2)
		return;
		
	if (pathFindingDebug)
	{
		Vector2 lastPos = nodePath.front()->posWorld;
		for (const Node* node : nodePath)
		{
			Line2(node->posWorld, lastPos).RenderDebug(Color::White(0.3f), pathFindingDebugTime);
			lastPos = node->posWorld;
		}
	}

	// shortcut nodes by pushing them along direction of least resistance if avaliable
	// this fixes up paths that are long due to greedy heuristic
	NodeList::iterator it = nodePath.begin();
	while (next(it) != nodePath.end())
	{
		if (it == nodePath.begin())
			++it;

		const NodeList::iterator itPrev = prev(it);
		const NodeList::iterator itNext = next(it);
		const Node& nodePrev = **itPrev;
		const Node& node = **it;
		const Node& nodeNext = **itNext;

		// use length to get shape of line
		const IntVector2 deltaPos = node.pos - nodePrev.pos;
		const IntVector2 deltaPosNext = nodeNext.pos - node.pos;
		const IntVector2 deltaPosPrevNext = nodeNext.pos - nodePrev.pos;
		const int lengthSquared = deltaPosPrevNext.LengthSquared();
		ASSERT(lengthSquared == 1 || lengthSquared == 2 || lengthSquared == 4 || lengthSquared == 5 || lengthSquared == 8);

		if (lengthSquared == 1)
		{
			// 45 degree angle - remove node
			if (pathFindingDebug)
				node.posWorld.RenderDebug(Color::Purple(0.5f), 0.2f, pathFindingDebugTime);
			nodePath.erase(it);
			it = itPrev;
			continue;
		}
		else if (lengthSquared == 2)
		{
			// 90 degree angle
			ASSERT((nodePrev.pos.y == node.pos.y && nodeNext.pos.x == node.pos.x) ||  (nodePrev.pos.x == node.pos.x && nodeNext.pos.y == node.pos.y))
			if (pathFindingDebug)
				node.posWorld.RenderDebug(Color::Red(0.5f), 0.2f, pathFindingDebugTime);

			IntVector2 shortcutPos;
			if (nodePrev.pos.y == node.pos.y && nodeNext.pos.x == node.pos.x)
				shortcutPos = IntVector2(nodePrev.pos.x, nodeNext.pos.y);
			else
				shortcutPos = IntVector2(nodeNext.pos.x, nodePrev.pos.y);

			const Node& nodeShortcut = GetNodeFast(shortcutPos);
			if (nodeShortcut.IsClear())
			{
				nodePath.erase(it);
				it = itPrev;
				continue;
		}
		}
		else if (lengthSquared == 5)
		{
			// 135 degree angle
			if (pathFindingDebug)
				node.posWorld.RenderDebug(Color::Yellow(0.5f), 0.2f, pathFindingDebugTime);

			const NodeList::iterator itPrevPrev = (itPrev == nodePath.begin()) ? itPrev :  prev(itPrev);
			const Node& nodePrevPrev = **itPrevPrev;

			IntVector2 shortcutPos1;
			IntVector2 shortcutPos2;
			if (deltaPos.x == 0 || deltaPosNext.x == 0)
			{
				// mostly vertical
				shortcutPos1 = IntVector2(nodeNext.pos.x, node.pos.y);
				shortcutPos2 = IntVector2(nodePrev.pos.x, node.pos.y);

			}
			else
			{
				// mostly horizontal
				shortcutPos1 = IntVector2(node.pos.x, nodeNext.pos.y);
				shortcutPos2 = IntVector2(node.pos.x, nodePrev.pos.y);
			}

			// get closer shortcut
			const IntVector2 shortcutDeltaPos1 = shortcutPos1 - nodePrevPrev.pos;
			const IntVector2 shortcutDeltaPos2 = shortcutPos2 - nodePrevPrev.pos;
			ASSERT(shortcutDeltaPos1.LengthSquared() != shortcutDeltaPos2.LengthSquared());

			const IntVector2 shortcutPos = (shortcutDeltaPos1.LengthSquared() < shortcutDeltaPos2.LengthSquared()) ? shortcutPos1 : shortcutPos2;
			Node& nodeShortcut = GetNodeFast(shortcutPos);
			if (&nodeShortcut != &node && nodeShortcut.IsClear())
			{
				// check if corner cut is clear
				const IntVector2  cutCornerPos = nodeNext.pos + shortcutPos2 - shortcutPos1;
				Node& cutCorner = GetNodeFast(cutCornerPos);
				if (cutCorner.IsClear())
				{
					*it = &nodeShortcut;
					it = itPrev;
					continue;
				}
			}
		}
		else if (lengthSquared == 4 || lengthSquared == 8)
		{
			// straight line or bump
			if (pathFindingDebug)
				node.posWorld.RenderDebug(Color::Green(0.5f), 0.2f, pathFindingDebugTime);

			if (deltaPos == deltaPosNext)
			{
				// straight line
				++it;
				continue;
			}
			else
			{
				// bump
				ASSERT((nodePrev.pos.y == nodeNext.pos.y) || (nodePrev.pos.x == nodeNext.pos.x));
				
				IntVector2 shortcutPos;
				if (nodePrev.pos.y == nodeNext.pos.y)
					shortcutPos = IntVector2(node.pos.x, nodePrev.pos.y);
				else
					shortcutPos = IntVector2(nodePrev.pos.x, node.pos.y);

				Node& nodeShortcut = GetNodeFast(shortcutPos);
				if (nodeShortcut.IsClear())
				{
					*it = &nodeShortcut;
					it = itPrev;
					continue;
				}
			}
		}
		
		++it;
	}
}

bool PathFindingBase::CheckLine(const IntVector2& startPos, const IntVector2& endPos) const
{
	ASSERT(GetNode(startPos) && GetNode(endPos) && GetNode(startPos)->IsClear() && GetNode(endPos)->IsClear())

	// check if there is a clear shot from startPos to endPos
	const IntVector2 d = endPos - startPos;
	const IntVector2 dAbs = d.Abs();
	const IntVector2 directionSign(GetSign(d.x), GetSign(d.y));
	IntVector2 pos = startPos;

	if (dAbs.y == dAbs.x)
	{
		// diagonal
		for (; pos.x != endPos.x; pos.x += directionSign.x)
		{
			if (pos.x != startPos.x)
			{
				if (!IsNodeClear(pos))
					return false;
				if (!IsNodeClear(pos - IntVector2(0, directionSign.y)))
					return false;
			}
			if (!IsNodeClear(pos + IntVector2(0, directionSign.y)))
				return false;
			pos.y += directionSign.y;
		}
		pos.y = endPos.y;
		if (!IsNodeClear(pos - IntVector2(0, directionSign.y)))
			return false;

	}
	else if (dAbs.y < dAbs.x)
	{
		// mostly horizontal
		if (d.y == 0)
		{
			// horizontal
			pos.x += directionSign.x;
			for (; pos.x != endPos.x; pos.x += directionSign.x)
			{
				if (!IsNodeClear(pos))
					return false;
			}
		}
		else
		{
			int lastY = startPos.y;
			for (; pos.x != endPos.x; pos.x += directionSign.x)
			{
				pos.y = startPos.y + (d.y * (pos.x - startPos.x)) / d.x;
				if (lastY != pos.y)
				{
					// extra checks on each step
					if (!IsNodeClear(pos - IntVector2(directionSign.x, -directionSign.y)))
						return false;
					if (!IsNodeClear(pos - IntVector2(0, directionSign.y)))
						return false;
				}
				lastY = pos.y;

				if (pos.x != startPos.x)
				{
					if (!IsNodeClear(pos))
						return false;
				}
				pos.y += directionSign.y;
				if (!IsNodeClear(pos))
					return false;
			}
			pos.y = endPos.y;
			pos.y -= directionSign.y;
			if (!IsNodeClear(pos))
				return false;
		}
	}
	else
	{
		// mostly vertical
		if (d.x == 0)
		{
			// vertical
			pos.y += directionSign.y;
			for (; pos.y != endPos.y; pos.y += directionSign.y)
			{
				if (!IsNodeClear(pos))
					return false;
			}
		}
		else
		{
			int lastX = startPos.x;
			for (; pos.y != endPos.y; pos.y += directionSign.y)
			{
				pos.x = startPos.x + (d.x * (pos.y - startPos.y)) / d.y;
				if (lastX != pos.x)
				{
					// extra checks on each step
					if (!IsNodeClear(pos - IntVector2(-directionSign.x, directionSign.y)))
						return false;
					if (!IsNodeClear(pos - IntVector2(directionSign.x, 0)))
						return false;
				}
				lastX = pos.x;

				if (pos.y != startPos.y)
				{
					pos.x = startPos.x + (d.x * (pos.y - startPos.y)) / d.y;
					if (!IsNodeClear(pos))
						return false;
				}
				pos.x += directionSign.x;
				if (!IsNodeClear(pos))
					return false;
			}
			pos.x = endPos.x;
			pos.x -= directionSign.x;
			if (!IsNodeClear(pos))
				return false;
		}
	}

	return true;
}

void PathFindingBase::ShortenPath2(NodeList& nodePath)
{
	if (!pathFindingTryToShorten || nodePath.size() <= 2)
		return;

	for (Node* node : nodePath)
	{
		if (!node->IsClear())
			return; // only works if all points are fully clear with 0 cost
	}

	if (pathFindingDebug)
	{
		Vector2 lastPos = nodePath.front()->posWorld;
		for (const Node* node : nodePath)
		{
			Line2(node->posWorld, lastPos).RenderDebug(Color::Blue(0.3f), pathFindingDebugTime);
			lastPos = node->posWorld;
		}
	}

	// take large shortcuts by checking where we can make straight shots from
	// this allows path to go off grid if the way is clear

	const NodeList originalNodePath = nodePath;
	nodePath.clear();
	nodePath.push_back(*originalNodePath.begin());
	NodeList::const_iterator itSearch = originalNodePath.begin();
	
	for (NodeList::const_iterator it = next(originalNodePath.begin()); it != originalNodePath.end(); ++it)
	{
		Node* node = *it;
		// if node is colinear with search and previous it can be skipped
		if (node->pos.IsOnLine((**itSearch).pos, (**prev(it)).pos))
		{
			//node->Render(Color::Magenta(0.2f));
			continue;
		}

		//Line2(nodePath.back()->posWorld, node->posWorld).RenderDebug(Color::Green());
		if (!CheckLine(node->pos, nodePath.back()->pos))
		{
			// check if there is a clear shot to one after us
			bool foundClearLineAfter = false;
			for (NodeList::const_iterator it2 = next(it); it2 != originalNodePath.end(); ++it2)
			{
				if (CheckLine((**it2).pos, nodePath.back()->pos))
				{
					foundClearLineAfter = true;
					break;
				}
			}
			if (foundClearLineAfter)
			{
				//node->Render(Color::Red(0.2f));
				continue;
			}

			// find last node we had a clear line to
			//node->Render(Color::Red());
			while (true)
			{
				Node& nodeSearch = **itSearch;
				//nodeSearch.Render(Color::Blue());
				//Line2(node->posWorld, nodeSearch.posWorld).RenderDebug(Color::Blue());
				if (CheckLine(node->pos, nodeSearch.pos))
				{
					// found clear line, use this point
					ASSERT(&nodeSearch != nodePath.back());
					//nodeSearch.Render(Color::Green());
					nodePath.push_back(&nodeSearch);
					it = itSearch;
					break;
				}
				itSearch++;
				ASSERT(itSearch != originalNodePath.end());
			}
			ASSERT(itSearch == it);
		}
	}
	
	nodePath.push_back(originalNodePath.back());
}