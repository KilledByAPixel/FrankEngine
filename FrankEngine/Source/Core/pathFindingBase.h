////////////////////////////////////////////////////////////////////////////////////////
/*
	Base Class for Path Finding
	Copyright 2018 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

struct TerrainTile;
class GameObject;

class PathFindingBase
{
public:

	~PathFindingBase();
	
	void Update();

	bool GetPath(const Vector2& startPos, const Vector2& endPos, list<Vector2>& path, bool canFly = false);
	int GetPathFindCostThisFrame() const { return pathFindCostThisFrame; }
	int GetPathFindCostLastFrame() const { return pathFindCostLastFrame; }
	bool IsOkToPathFind() const { return pathFindCostThisFrame <= maxFrameCost; }
	
	static bool pathFindingEnable;
	static bool pathFindingDebug;
	static bool pathFindingTryToShorten;
	static float pathFindingDebugTime;
	static float heuristicWeight;
	static int maxFrameCost;
	static int maxLoop;

protected:

	// override these with custom game logic
	virtual bool IsTileWalkable(const TerrainTile& tile, bool canFly) const;
	virtual float GetObjectCost(const GameObject& object, bool canFly) const;

	struct Node
	{	
		Node() {}
		Node(const IntVector2& _pos) : pos(_pos) { Init(); }
		
		void Init()
		{
			Reset();
			cost = maxCost;
			isWalkable = false;
		}

		void Reset()
		{
			parent = NULL;
			f = g = 0;
			touchCount = 0;
			isClosed = isOpen = false;
		}
		
		bool IsClear() const { return isWalkable && cost == 0; }
		void Render(const Color& color = Color::White(0.5f), float time = 0) const;

		float cost;
		bool isWalkable;
		Node* parent;

		bool isClosed;
		bool isOpen;
		float f, g;
		Vector2 posWorld;
		IntVector2 pos;
		int touchCount;
		static const int maxCost = 1000;
	};

	typedef list<Node*> NodeList;
	
	void BuildNodeData(bool canFly);
	void ClearNodeData();
	void ResetNodeData();
	
	bool AStarSearch(Node* startNode, Node* endNode);
	void ShortenPath(NodeList& nodePath);
	void ShortenPath2(NodeList& nodePath);

	IntVector2 GetNodePos(const Vector2& worldPos) const;
	Node* GetNode(const IntVector2& index) const;
	Node& GetNodeFast(const IntVector2& index) const;
	Node* GetNode(const Vector2& worldPos) const; 
	Node* GetNearestClearNode(const Vector2& worldPos, int searchRange = 2) const;
	bool CheckLine(const IntVector2& startPos, const IntVector2& endPos) const;
	bool IsNodeClear(const IntVector2& pos) const;
	IntVector2 GetWindowOffset() const;
	float GetShortestPossibleDistance(Node* startNode, Node* endNode) const;

	int pathFindCostThisFrame = 0;
	int pathFindCostLastFrame = 0;
	int arrayWidth = 0;
	float debugRenderTime = 5;
	Node* nodes = NULL;
	int windowSize = 0;
};
